
import argparse
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import crystal_maps as M
from crystal_rom import Rom

PC = os.path.join(ROOT, "pokecrystal-master")

TRAINERTYPE_NORMAL, TRAINERTYPE_MOVES = 0, 1
TRAINERTYPE_ITEM, TRAINERTYPE_ITEM_MOVES = 2, 3
NAME_TERM, PARTY_END = 0x50, 0xFF
FALLBACK_SPECIES = "RATTATA"

def decode_name(rom, bank, addr, limit=16):
    out = []
    for i in range(limit):
        b = rom.u8(bank, addr + i)
        if b == NAME_TERM:
            return "".join(out), addr + i + 1
        if 0x80 <= b <= 0x99:
            out.append(chr(ord("A") + b - 0x80))
        elif 0xA0 <= b <= 0xB9:
            out.append(chr(ord("a") + b - 0xA0))
        elif 0xF6 <= b <= 0xFF:
            out.append(chr(ord("0") + b - 0xF6))
        elif b == 0x7F:
            out.append(" ")
        else:
            out.append("?")
    return "".join(out), addr + limit

def read_groups(rom, num_classes):
    bank, addr = rom.addr_of("TrainerGroups")
    return [(bank, rom.u16(bank, addr + i * 2)) for i in range(num_classes)]

def read_class(rom, bank, addr, species_of, moves_of, items_of, end):
    out = []
    while addr < end:
        if rom.u8(bank, addr) in (0x00, 0xFF):
            break
        name, a = decode_name(rom, bank, addr)
        ttype = rom.u8(bank, a)
        a += 1
        if ttype not in (TRAINERTYPE_NORMAL, TRAINERTYPE_MOVES,
                         TRAINERTYPE_ITEM, TRAINERTYPE_ITEM_MOVES):
            break
        party = []
        while rom.u8(bank, a) != PARTY_END:
            level = rom.u8(bank, a)
            sp = rom.u8(bank, a + 1)
            a += 2
            item = None
            moves = []
            if ttype in (TRAINERTYPE_ITEM, TRAINERTYPE_ITEM_MOVES):
                item = items_of.get(rom.u8(bank, a))
                a += 1
            if ttype in (TRAINERTYPE_MOVES, TRAINERTYPE_ITEM_MOVES):
                moves = [moves_of.get(rom.u8(bank, a + k)) for k in range(4)]
                a += 4
            party.append({"level": level, "species": species_of.get(sp, f"#{sp}"),
                          "dex": sp, "item": item,
                          "moves": [m for m in moves if m and m != "NO_MOVE"]})
            if len(party) > 6:
                break
        out.append({"name": name, "type": ttype, "party": party})
        addr = a + 1
    return out, addr

def substitute(party, gen1):
    present = [m["species"] for m in party if m["species"] in gen1]
    common = (collections.Counter(present).most_common(1)[0][0]
              if present else FALLBACK_SPECIES)
    subs = []
    for m in party:
        if m["species"] not in gen1:
            subs.append((m["species"], common))
            m["substituted_from"] = m["species"]
            m["species"] = common
    return subs

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", action="store_true")

    ap.add_argument("--substitute-gen1", action="store_true",
                    help="replace Johto-exclusive species with Gen 1 ones "
                         "(the old lossy behaviour)")
    ap.add_argument("--rom", default=os.path.join(PC, "pokecrystal.gbc"))
    ap.add_argument("--sym", default=os.path.join(PC, "pokecrystal.sym"))
    args = ap.parse_args()

    rom = Rom(args.rom, args.sym)
    pokemon = M.const_table(os.path.join(PC, "constants", "pokemon_constants.asm"))

    species_of = {}
    for k, v in pokemon.items():
        species_of.setdefault(v, k)
    moves_of = {v: k for k, v in M.const_table(
        os.path.join(PC, "constants", "move_constants.asm")).items()}
    items_of = {v: k for k, v in M.const_table(
        os.path.join(PC, "constants", "item_constants.asm")).items()}

    gen1 = {n for n, v in pokemon.items() if 1 <= v <= 151}

    classes = [l.split()[1] for l in
               open(os.path.join(PC, "constants", "trainer_constants.asm"),
                    encoding="utf-8", errors="replace")
               if re.match(r"\s*trainerclass\s+\w+", l)]
    groups = read_groups(rom, len(classes))

    all_trainers, subs_total, mons = {}, [], 0
    for i, cname in enumerate(classes):
        if cname == "TRAINER_NONE":
            continue
        bank, addr = groups[i - 1]

        end = (groups[i][1] if i < len(groups)
               else (rom.next_symbol_addr(bank, addr) or 0x8000))
        try:
            tl, _end = read_class(rom, bank, addr, species_of, moves_of,
                                  items_of, end)
        except Exception as e:
            print(f"  !! {cname}: {e}")
            continue
        for t in tl:
            if args.substitute_gen1:
                subs_total += substitute(t["party"], gen1)
            mons += len(t["party"])

        for t in tl:
            t["class_id"] = i
        all_trainers[cname] = tl

    total = sum(len(v) for v in all_trainers.values())
    print(f"{len(all_trainers)} classes, {total} trainers, {mons} party mons")
    print(f"{len(subs_total)} species substituted "
          f"({len(subs_total) * 100 // max(mons, 1)}% of mons)")
    top = collections.Counter(s for s, _ in subs_total).most_common(8)
    for s, n in top:
        print(f"    {s} x{n}")

    if not args.report:

        out_dir = os.path.join(ROOT, "generated")
        os.makedirs(out_dir, exist_ok=True)
        pokedex = {n: v for n, v in pokemon.items() if 1 <= v <= 251}
        moves_id = {v: k for k, v in M.const_table(
            os.path.join(PC, "constants", "move_constants.asm")).items()}
        move_id_of = {k: v for v, k in moves_id.items()}

        flat, index = [], {}
        for cname, tl in all_trainers.items():
            for n, t in enumerate(tl, start=1):
                index[(cname, n)] = len(flat)
                flat.append((cname, n, t))

        origin = ("Johto-exclusive species are substituted in-family; see the\n"
                  " * extractor."
                  if args.substitute_gen1 else
                  "Species are Crystal's own, UNSUBSTITUTED: the dex runs to 251\n"
                  " * and Johto-exclusive mon appear as themselves.")
        banner = ("/* {} -- GENERATED by tools/romimport/extract_crystal_trainers.py.\n"
                  " * Crystal trainer parties, read from pokecrystal.gbc.\n"
                  " * " + origin + "  ROM-derived: never commit this.\n"
                  " */\n")
        species_note = ("DEX number, 1-151 after substitution"
                        if args.substitute_gen1 else "DEX number, 1-251")
        with open(os.path.join(out_dir, "johto_trainers.h"), "w",
                  newline="\n") as f:
            f.write(banner.format("johto_trainers.h"))
            f.write("#pragma once\n#include <stdint.h>\n\n")
            f.write(f"#define JOHTO_TRAINER_COUNT {len(flat)}\n\n")
            f.write("typedef struct {\n"
                    f"    uint8_t species;   /* {species_note} */\n"
                    "    uint8_t level;\n"
                    "    uint8_t moves[4];  /* 0 = unset (Crystal TRAINERTYPE_NORMAL) */\n"
                    "} johto_mon_t;\n\n")
            f.write("typedef struct {\n"
                    "    const char *class_name;  /* Crystal trainer class */\n"
                    "    uint8_t     trainer_no;  /* 1-based within the class */\n"
                    "    const char *name;\n"
                    "    uint8_t     count;\n"
                    "    uint8_t     class_id;    /* trainer-class constant --\n"
                    "                              * index\n"
                    "                              * gCrystalTrainerEncounterMusic\n"
                    "                              * with this for the class's\n"
                    "                              * \"noticed you\" theme */\n"
                    "    johto_mon_t party[6];\n"
                    "} johto_trainer_t;\n\n")
            f.write("extern const johto_trainer_t gJohtoTrainers[JOHTO_TRAINER_COUNT];\n")
            f.write("/* -1 if that class/number does not exist. */\n")
            f.write("int JohtoTrainer_Find(const char *class_name, int trainer_no);\n")

        with open(os.path.join(out_dir, "johto_trainers.c"), "w",
                  newline="\n") as f:
            f.write(banner.format("johto_trainers.c"))
            f.write('#include "johto_trainers.h"\n#include <string.h>\n\n')
            f.write("const johto_trainer_t gJohtoTrainers[JOHTO_TRAINER_COUNT] = {\n")
            for cname, n, t in flat:
                f.write(f'    {{ "{cname}", {n}, "{t["name"]}", {len(t["party"])},'
                        f' {t["class_id"]}, {{\n')
                for m in t["party"]:
                    mv = [str(move_id_of.get(x, 0)) for x in m["moves"]]
                    mv += ["0"] * (4 - len(mv))
                    f.write(f'        {{ {pokedex.get(m["species"], 0)}, '
                            f'{m["level"]}, {{ {", ".join(mv)} }} }},\n')
                for _ in range(6 - len(t["party"])):
                    f.write("        { 0, 0, { 0, 0, 0, 0 } },\n")
                f.write("    } },\n")
            f.write("};\n\n")
            f.write("int JohtoTrainer_Find(const char *class_name, int trainer_no) {\n"
                    "    for (int i = 0; i < JOHTO_TRAINER_COUNT; i++)\n"
                    "        if (gJohtoTrainers[i].trainer_no == trainer_no &&\n"
                    "            strcmp(gJohtoTrainers[i].class_name, class_name) == 0)\n"
                    "            return i;\n"
                    "    return -1;\n"
                    "}\n")
        print(f"wrote {out_dir}\\johto_trainers.h / .c  ({len(flat)} trainers)")

    if args.report:
        for cname in list(all_trainers)[:4]:
            for t in all_trainers[cname][:2]:
                party = ", ".join(f"L{m['level']} {m['species']}"
                                  + (f"[{m['item']}]" if m["item"] else "")
                                  for m in t["party"])
                print(f"  {cname:16s} {t['name']:12s} {party}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
