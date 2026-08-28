
import argparse
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "tools"))

import crystal_maps as M
from crystal_rom import Rom

PC = os.path.join(ROOT, "pokecrystal-master")
OUT = os.path.join(ROOT, "generated")

FIRST_MUSIC_CMD = 0xD0
SOUND_RET = 0xFF

ENGINE_DECIDED = {"note_type", "toggle_noise", "sfx_toggle_noise"}

MACRO_LEGACY_FIXED = {"vibrato": 2, "volume": 1}

ADDR_COMMANDS = {"restart_channel", "sound_jump_if", "sound_jump",
                 "sound_loop", "sound_call", "unknownmusic0xee"}

def command_table(pc_dir):
    path = os.path.join(pc_dir, "macros", "scripts", "audio.asm")
    src = open(path, encoding="utf-8").read()

    order, value, in_const = [], None, False
    for line in src.splitlines():
        s = line.split(";")[0].strip()
        m = re.match(r"const_def\s+\$([0-9a-fA-F]+)", s)
        if m:
            value = int(m.group(1), 16)
            in_const = True
            continue
        if not in_const:
            continue
        m = re.match(r"const\s+(\w+)_cmd\b", s)
        if m:
            order.append((value, m.group(1)))
            value += 1
            continue
        m = re.match(r"const_skip\s+(\d+)", s)
        if m:
            value += int(m.group(1))

    bodies = {}
    for m in re.finditer(r"^MACRO\s+(\w+)\s*$(.*?)^ENDM", src,
                         re.MULTILINE | re.DOTALL):
        bodies[m.group(1)] = m.group(2)

    table = {}
    for op, name in order:
        body = bodies.get(name, "")
        if name in MACRO_LEGACY_FIXED:
            table[op] = (name, MACRO_LEGACY_FIXED[name])
            continue
        if "_NARG" in body:
            table[op] = (name, None)
            continue

        arms = [0]

        def emit_bytes(s):
            if re.match(rf"db\s+{name}_cmd\b", s):
                return 0
            if re.match(r"db\s", s):
                return len(s[3:].split(","))
            if re.match(r"dw\s", s) or re.match(r"bigdw\s", s):
                return 2
            if re.match(r"dn\s", s):
                return 1
            return 0

        for line in body.splitlines():
            s = line.split(";")[0].strip()
            if re.match(r"if\s", s):
                arms.append(0)
            elif s == "else" or re.match(r"elif\s", s):
                arms.append(0)
            elif s == "endc":

                branches = arms[1:]
                if len(set(branches)) > 1:
                    sys.exit(f"macro {name}: conditional arms emit different "
                             f""
                             f"handler, this is not a static length")
                arms = [arms[0] + (branches[0] if branches else 0)]
            else:
                arms[-1] += emit_bytes(s)
        table[op] = (name, sum(arms))

    variable = {nm for _o, (nm, ln) in table.items() if ln is None}
    if variable != ENGINE_DECIDED:
        sys.exit(f""
                 f""
                 f"handler for the new one before trusting any walk.")

    for op in range(0xD0, 0xD8):
        table[op] = ("octave", 0)
    return table

def cmd_len(table, op, noise_channel, noise_on, sfx_on, cry_on):
    if op < FIRST_MUSIC_CMD:

        if sfx_on or cry_on:
            return (3 if noise_channel else 4), noise_on, sfx_on
        return 1, noise_on, sfx_on
    name, fixed = table[op]
    if name == "toggle_sfx":
        return 1, noise_on, not sfx_on
    if fixed is not None:
        return 1 + fixed, noise_on, sfx_on
    if name == "note_type":

        return (2 if noise_channel else 3), noise_on, sfx_on

    return (2 if not noise_on else 1), (not noise_on), sfx_on

def walk_channel(rom, bank, start, table, noise_channel, sfx0, cry_on):
    seen = {}
    conflicts = []
    work = [(start, False, sfx0)]
    while work:
        pc, noise_on, sfx_on = work.pop()
        while True:
            if pc in seen:

                break
            op = rom.u8(bank, pc)
            n, noise_on, sfx_on = cmd_len(table, op, noise_channel,
                                          noise_on, sfx_on, cry_on)
            seen[pc] = (op, n)
            name = table[op][0] if op >= FIRST_MUSIC_CMD else "note"
            if name in ADDR_COMMANDS:

                tgt = rom.u16(bank, pc + n - 2)
                work.append((tgt, noise_on, sfx_on))

            if name in ("sound_ret", "sound_jump", "restart_channel"):
                break
            if name == "sound_loop" and rom.u8(bank, pc + 1) == 0:
                break
            pc += n
    return seen, conflicts

def read_header(rom, bank, addr):
    first = rom.u8(bank, addr)
    count = ((first >> 6) & 3) + 1
    out = []
    for i in range(count):
        b = rom.u8(bank, addr + i * 3)
        out.append(((b & 0x0F) + 1, rom.u16(bank, addr + i * 3 + 1)))
    return out

def extract_one(rom, bank, addr, table, sfx0, cry_on):
    chans = []
    for cid, caddr in read_header(rom, bank, addr):
        noise_channel = ((cid - 1) & 3) == 3
        seen, _c = walk_channel(rom, bank, caddr, table, noise_channel,
                                sfx0, cry_on)
        if not seen:
            continue
        base = min(seen)
        end = max(a + n for a, (_o, n) in seen.items())
        blob = bytearray(rom.read(bank, base, end - base))

        for a, (op, n) in seen.items():
            name = table[op][0] if op >= FIRST_MUSIC_CMD else "note"
            if name not in ADDR_COMMANDS:
                continue
            off = a + n - 2 - base
            tgt = blob[off] | (blob[off + 1] << 8)
            rel = tgt - base
            if not (0 <= rel < len(blob)):
                sys.exit(f"channel at ${caddr:04X}: target ${tgt:04X} is "
                         f"outside the walked blob [${base:04X},${end:04X}) -- "
                         f"the walk missed a path")
            blob[off] = rel & 0xFF
            blob[off + 1] = (rel >> 8) & 0xFF
        chans.append((cid, bytes(blob), caddr - base))
    return chans

def table_entries(rom, sym, prefix):
    bank, addr = rom.addr_of(sym)
    end = rom.next_symbol_addr(bank, addr)
    n = ((end - addr) // 3) if end else 0
    out = []
    for i in range(n):
        b, a = rom.dba(bank, addr + i * 3)
        names = [s for s in rom.names_at(b, a) if not s.startswith(".")]
        nm = sorted(names, key=len)[0] if names else f"{prefix}_{i:03d}"
        out.append((i, nm, b, a))
    return out

def read_mon_cries(rom):
    bank, addr = rom.addr_of("PokemonCries")
    end = rom.next_symbol_addr(bank, addr)
    n = ((end - addr) // 6) if end else 0
    return [(rom.u16(bank, addr + i * 6),
             rom.u16(bank, addr + i * 6 + 2),
             rom.u16(bank, addr + i * 6 + 4)) for i in range(n)]

def read_trainer_encounter_music(rom):
    bank, addr = rom.addr_of("TrainerEncounterMusic")
    end = rom.next_symbol_addr(bank, addr)
    n = (end - addr) if end else 0
    return list(rom.read(bank, addr, n))

def emit(rom):
    table = command_table(PC)
    cries = read_mon_cries(rom)
    enc_music = read_trainer_encounter_music(rom)
    groups = [("Music", "gCrystalMusic", "MUSIC"),
              ("SFX", "gCrystalSfx", "SFX"),
              ("Cries", "gCrystalCries", "CRY")]

    os.makedirs(OUT, exist_ok=True)
    hpath = os.path.join(OUT, "crystal_audio.h")
    cpath = os.path.join(OUT, "crystal_audio.c")
    result = {}

    all_data = []
    decls = []
    for sym, cvar, pfx in groups:
        entries = table_entries(rom, sym, pfx)
        songs = []
        for i, nm, b, a in entries:

            chans = extract_one(rom, b, a, table,
                                sym == "SFX", sym == "Cries")
            slug = re.sub(r"[^A-Za-z0-9]", "", nm)
            parts = []
            for ci, (cid, blob, entry) in enumerate(chans):
                v = f"s_{cvar}_{i:03d}_{ci}"
                all_data.append((v, blob))
                parts.append((cid, v, entry, len(blob)))
            songs.append((i, nm, slug, parts))
        decls.append((cvar, songs))
        result[sym] = songs

    with open(hpath, "w", newline="\n") as f:
        f.write("/* crystal_audio.h -- GENERATED by\n"
                " * tools/romimport/extract_crystal_audio.py.  DO NOT EDIT.\n"
                " * ROM-derived: never commit this file. */\n"
                "#pragma once\n#include <stdint.h>\n\n")
        f.write("/* One channel's command stream, exactly as the ROM stores it,\n"
                " * except that jump/call/loop addresses are relocated to be\n"
                " * offsets into `data` (src/game/johto_music.c uses its pc as\n"
                " * an array index). `channel` is the 1-based id from the song\n"
                " * header -- 1-4 for music, 5-8 for sfx; (channel-1) & 3 picks\n"
                " * the hardware channel, and == 3 means noise. */\n")
        f.write("typedef struct {\n"
                "    uint8_t        channel;\n"
                "    uint16_t       entry;    /* start offset within data */\n"
                "    uint16_t       len;\n"
                "    const uint8_t *data;\n"
                "} crystal_audio_channel_t;\n\n")
        f.write("typedef struct {\n"
                "    const char *name;        /* symbol from the ROM's .sym */\n"
                "    uint8_t     num_channels;\n"
                "    const crystal_audio_channel_t *channels;\n"
                "} crystal_audio_track_t;\n\n")
        for cvar, songs in decls:
            up = cvar.replace("gCrystal", "CRYSTAL_NUM_").upper()
            f.write(f"#define {up} {len(songs)}\n")
        f.write("\n")

        for fname, prefix, cvar in (
                ("music_constants.asm", "MUSIC_", "gCrystalMusic"),
                ("sfx_constants.asm", "SFX_", "gCrystalSfx"),
                ("cry_constants.asm", "CRY_", "gCrystalCries")):
            consts = M.const_table(os.path.join(PC, "constants", fname))
            ids = {n: v for n, v in consts.items() if n.startswith(prefix)}
            n_entries = next(len(s) for c, s in decls if c == cvar)
            if ids and max(ids.values()) + 1 != n_entries:
                sys.exit(f"{fname}: {max(ids.values()) + 1} ids but the "
                         f"pointer table has {n_entries} entries")
            f.write(f"/* {fname} */\n")
            for nm, v in sorted(ids.items(), key=lambda kv: kv[1]):
                f.write(f"#define CRYSTAL_{nm:<34} {v}\n")
            f.write("\n")
        for cvar, songs in decls:
            up = cvar.replace("gCrystal", "CRYSTAL_NUM_").upper()
            f.write(f"extern const crystal_audio_track_t {cvar}[{up}];\n")

        f.write(f"\n#define CRYSTAL_NUM_TRAINER_CLASSES {len(enc_music)}\n")
        f.write("/* What plays when a trainer notices you, per trainer class.\n"
                " * Crystal picks from a family of Look* tracks by class;\n"
                " * Gen 1 only ever had two (MUSIC_MEET_MALE_TRAINER and\n"
                " * MUSIC_MEET_EVIL_TRAINER), so a Johto trainer routed through\n"
                " * the Gen 1 path gets the wrong theme by construction. Index\n"
                " * with johto_trainer_t.class_id. */\n"
                "extern const uint8_t gCrystalTrainerEncounterMusic"
                "[CRYSTAL_NUM_TRAINER_CLASSES];\n")

        f.write(f"\n#define CRYSTAL_NUM_MON_CRIES {len(cries)}\n")
        f.write("typedef struct { uint16_t index, pitch, length; } "
                "crystal_mon_cry_t;\n"
                "extern const crystal_mon_cry_t "
                "gCrystalMonCries[CRYSTAL_NUM_MON_CRIES];\n")

    with open(cpath, "w", newline="\n") as f:
        f.write("/* crystal_audio.c -- GENERATED, see the header. */\n"
                '#include "crystal_audio.h"\n\n')
        for v, blob in all_data:
            f.write(f"static const uint8_t {v}[] = {{")
            for j, byte in enumerate(blob):
                f.write(("\n    " if j % 16 == 0 else " ") + f"0x{byte:02X},")
            f.write("\n};\n")
        f.write("\n")
        for cvar, songs in decls:
            for i, nm, slug, parts in songs:
                if not parts:
                    continue
                f.write(f"static const crystal_audio_channel_t "
                        f"s_{cvar}_{i:03d}_ch[] = {{\n")
                for cid, v, entry, ln in parts:
                    f.write(f"    {{ {cid}, {entry}, {ln}, {v} }},\n")
                f.write("};\n")
            up = cvar.replace("gCrystal", "CRYSTAL_NUM_").upper()
            f.write(f"\nconst crystal_audio_track_t {cvar}[{up}] = {{\n")
            for i, nm, slug, parts in songs:
                arr = f"s_{cvar}_{i:03d}_ch" if parts else "0"
                f.write(f'    {{ "{nm}", {len(parts)}, {arr} }},\n')
            f.write("};\n\n")
        f.write("const uint8_t gCrystalTrainerEncounterMusic"
                "[CRYSTAL_NUM_TRAINER_CLASSES] = {\n    "
                + ", ".join(str(v) for v in enc_music) + "\n};\n\n")
        f.write("const crystal_mon_cry_t "
                "gCrystalMonCries[CRYSTAL_NUM_MON_CRIES] = {\n")
        for idx, pitch, length in cries:
            f.write(f"    {{ {idx}, {pitch}, {length} }},\n")
        f.write("};\n")

    for sym, cvar, _p in groups:
        songs = result[sym]
        nch = sum(len(p) for _i, _n, _s, p in songs)
        nby = sum(ln for _i, _n, _s, p in songs for _c, _v, _e, ln in p)
        print(f"{sym:6s}: {len(songs):3d} entries, {nch:4d} channels, "
              f"{nby:6d} bytes")
    print(f"  {hpath}\n  {cpath}")
    return result

def verify(rom, res):

    old = os.path.join(OUT, "johto_music_oracle.h")
    if not os.path.exists(old):
        old = os.path.join(ROOT, "src", "data", "johto_music_data_gen.h")
    if not os.path.exists(old):
        print("verify: no assembled oracle present -- regenerate it with\n"
              "        python tools/extract_johto_audio.py > "
              "generated/johto_music_oracle.h")
        return True
    src = open(old, encoding="utf-8", errors="replace").read()
    want = {}
    for m in re.finditer(r"kJohto(\w+?)_Ch(\d)_bytes\[\]\s*=\s*\{(.*?)\}\s*;",
                         src, re.DOTALL):
        by = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", m.group(3))]
        want[(m.group(1).lower(), int(m.group(2)))] = by

    got = {}
    for _i, nm, slug, parts in res["Music"]:
        key = re.sub(r"^Music_?", "", nm).lower()
        for cid, v, entry, ln in parts:
            got[(key, cid)] = None

    gen = open(os.path.join(OUT, "crystal_audio.c"),
               encoding="utf-8", errors="replace").read()
    blobs = {m.group(1): [int(x, 16) for x in
                          re.findall(r"0x([0-9a-fA-F]{2})", m.group(2))]
             for m in re.finditer(r"static const uint8_t (s_\w+)\[\]\s*=\s*\{(.*?)\}\s*;",
                                  gen, re.DOTALL)}
    lookup = {}
    for _i, nm, slug, parts in res["Music"]:
        key = re.sub(r"^Music_?", "", nm).lower()
        for cid, v, entry, ln in parts:
            lookup[(key, cid)] = blobs.get(v, [])

    ok = fail = missing = dead = 0
    for (name, cid), wbytes in sorted(want.items()):
        gbytes = lookup.get((name, cid))
        if gbytes is None:
            missing += 1
            print(f"  -- {name} ch{cid}: no ROM entry matched by name")
            continue
        if gbytes == wbytes:
            ok += 1
        elif wbytes[:len(gbytes)] == gbytes:

            dead += 1
            print(f"  -- {name} ch{cid}: {len(wbytes) - len(gbytes)} trailing "
                  f"byte(s) the assembler emits are unreachable; the "
                  f"{len(gbytes)} reachable bytes match")
        else:
            fail += 1
            if fail <= 5:
                n = min(len(gbytes), len(wbytes))
                first = next((i for i in range(n) if gbytes[i] != wbytes[i]), n)
                print(f"  !! {name} ch{cid}: rom {len(gbytes)} B vs asm "
                      f"{len(wbytes)} B, first difference at byte {first}")
    print(f"\nverify: {ok} channels byte-identical to the assembled "
          f"disassembly, {dead} match but the assembler trails unreachable "
          f"bytes, {fail} genuinely differ, {missing} unmatched")
    return fail == 0

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=os.path.join(PC, "pokecrystal.gbc"))
    ap.add_argument("--sym", default=os.path.join(PC, "pokecrystal.sym"))
    ap.add_argument("--verify", action="store_true")
    args = ap.parse_args()
    rom = Rom(args.rom, args.sym)
    res = emit(rom)
    if args.verify:
        return 0 if verify(rom, res) else 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
