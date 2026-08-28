
import argparse
import os
import sys
from pathlib import Path

_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE))

import build_pak as B
from gen1_rom import Gen1Rom, default_paths

EVO_WIDTHS = {1: 3, 2: 4, 3: 3}

def parse_learnset(rec):
    p = 0
    while p < len(rec) and rec[p]:
        w = EVO_WIDTHS.get(rec[p])
        if w is None:
            return f"unknown evolution type {rec[p]:#04x} at {p}"
        p += w
        if p > len(rec):
            return f"evolution entry at {p - w} overruns the record"
    if p >= len(rec):
        return "no terminator after the evolution section"
    p += 1
    while p < len(rec) and rec[p]:
        if p + 1 >= len(rec):
            return f"learnset pair at {p} is missing its move byte"
        lvl = rec[p]
        if not 1 <= lvl <= 100:
            return f"learn level {lvl} at {p} is out of range"
        p += 2
    if p >= len(rec):
        return "no terminator after the learnset"
    p += 1
    if p != len(rec):
        return f"{len(rec) - p} trailing byte(s) after the record"
    return None

def parse_parties(block):
    p = 0
    n = 0
    while p < len(block):
        if block[p] == 0xFF:
            p += 1
            while p < len(block) and block[p]:
                if p + 1 >= len(block):
                    return None, f"party {n} truncated mid level/species pair"
                if not 1 <= block[p] <= 100:
                    return None, f"party {n} level {block[p]} out of range"
                p += 2
        else:
            if not 1 <= block[p] <= 100:
                return None, f"party {n} level {block[p]} out of range"
            p += 1
            while p < len(block) and block[p]:
                p += 1
        if p >= len(block):
            return None, f"party {n} has no terminator"
        p += 1
        n += 1
    return n, None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom")
    ap.add_argument("--sym")
    ap.add_argument("--root", default=str(_HERE.parent.parent))
    args = ap.parse_args()

    rom_path, sym_path = default_paths(args.root)
    rom = Gen1Rom(args.rom or rom_path, args.sym or sym_path)
    A = {a["name"]: a for a in B.ASSETS}
    fail = []

    def offsets(name):
        raw = A[name]["fn"](rom)
        return [int.from_bytes(raw[i:i + 2], "little")
                for i in range(0, len(raw), 2)]

    blob = A["gEvosMovesBlob"]["fn"](rom)
    offs = offsets("gEvosMovesOffsets")
    if len(offs) != B.EVOS_MOVES_TABLE_SIZE:
        fail.append(f"gEvosMovesOffsets has {len(offs)} entries, "
                    f"expected {B.EVOS_MOVES_TABLE_SIZE}")
    if offs and offs[0] != B.NO_ENTRY:
        fail.append("gEvosMoves[0] should be NULL (species ids are 1-based)")
    entries = B._evos_moves_entries(rom)
    ok = 0
    for i, o in enumerate(offs):
        if o == B.NO_ENTRY:
            continue
        if o >= len(blob):
            fail.append(f"gEvosMoves[{i}] offset {o} is past the blob")
            continue
        rec = entries[i - 1]
        if blob[o:o + len(rec)] != rec:
            fail.append(f"gEvosMoves[{i}] blob bytes do not match its record")
            continue
        why = parse_learnset(rec)
        if why:
            fail.append(f"gEvosMoves[{i}]: {why}")
        else:
            ok += 1
    print(f"learnsets            : {ok} well formed")

    tblob = A["gTrainerPartyBlob"]["fn"](rom)
    toffs = offsets("gTrainerPartyOffsets")
    if len(toffs) != B.NUM_TRAINERS:
        fail.append(f"gTrainerPartyOffsets has {len(toffs)} entries, "
                    f"expected {B.NUM_TRAINERS}")
    blocks = B._trainer_party_entries(rom)
    total = ok = empty = 0
    for i, o in enumerate(toffs):
        if o == B.NO_ENTRY or o >= len(tblob):
            fail.append(f"gTrainerPartyData[{i}] offset {o} is unusable")
            continue
        blk = blocks[i]
        if tblob[o:o + len(blk)] != blk:
            fail.append(f"gTrainerPartyData[{i}] blob bytes do not match")
            continue
        if blk == b"\x00":

            empty += 1
            ok += 1
            continue
        n, why = parse_parties(blk)
        if why:
            fail.append(f"gTrainerPartyData[{i}]: {why}")
        else:
            ok += 1
            total += n
    print(f"trainer classes      : {ok} well formed, {total} parties "
          f"({empty} declared-empty)")

    sq, nz, meta = B._cry_tables(rom)
    n_sq, n_nz = len(sq) // B.CRY_SQ_NOTE_BYTES, len(nz) // B.CRY_NOISE_NOTE_BYTES
    if len(meta) != B.NUM_BASE_CRIES * B.CRY_META_BYTES:
        fail.append(f"g_cry_defs_meta is {len(meta)} bytes, expected "
                    f"{B.NUM_BASE_CRIES * B.CRY_META_BYTES}")
    notes_seen = 0
    for i in range(B.NUM_BASE_CRIES):
        e = meta[i * B.CRY_META_BYTES:(i + 1) * B.CRY_META_BYTES]
        for label, n, off, total in (
                ("ch5", e[2], e[3] | (e[4] << 8), n_sq),
                ("ch6", e[7], e[8] | (e[9] << 8), n_sq),
                ("ch8", e[10], e[11] | (e[12] << 8), n_nz)):
            if off + n > total:
                fail.append(f"cry {i:02x} {label}: notes {off}..{off + n} "
                            f"run past the blob ({total})")
                continue
            notes_seen += n
            if label == "ch8":
                continue
            for k in range(n):
                base = (off + k) * B.CRY_SQ_NOTE_BYTES
                freq = sq[base + 3] | (sq[base + 4] << 8)
                if freq > 2047:
                    fail.append(f"cry {i:02x} {label} note {k}: frequency "
                                f"{freq} exceeds the 11-bit register")
    print(f"cries                : {B.NUM_BASE_CRIES} definitions, "
          f"{notes_seen} notes ({n_sq} square, {n_nz} noise)")

    money = A["gTrainerBaseMoney"]["fn"](rom)
    base = rom.offset("TrainerPicAndMoneyPointers")
    bad_bcd = 0
    for i in range(B.NUM_TRAINERS):
        for b in (rom.data[base + 5 * i + 3], rom.data[base + 5 * i + 4]):
            if (b >> 4) > 9 or (b & 0xF) > 9:
                bad_bcd += 1
    if bad_bcd:
        fail.append(f"{bad_bcd} prize-money byte(s) are not valid BCD -- the "
                    f"field offset is probably wrong")
    vals = [int.from_bytes(money[i * 2:i * 2 + 2], "little")
            for i in range(B.NUM_TRAINERS)]
    if any(v == 0 for v in vals):
        fail.append(f"{sum(1 for v in vals if v == 0)} trainer class(es) have "
                    f"a prize of 0")
    print(f"prize money          : {len(vals)} classes, "
          f"{min(vals)}..{max(vals)}")

    s2d = A["gSpeciesToDex"]["fn"](rom)
    d2s = A["gDexToSpecies"]["fn"](rom)
    holes = [d for d in range(1, 152) if d2s[d] == 0]
    if holes:
        fail.append(f"{len(holes)} dex number(s) map to no species: {holes[:6]}")
    broken = [i for i in range(256)
              if s2d[i] and s2d[i] < 152 and d2s[s2d[i]] != i]
    if broken:
        fail.append(f"{len(broken)} species do not round-trip through the dex "
                    f"tables, first {broken[:6]}")
    print(f"dex order            : {sum(1 for b in s2d if b)} species, "
          f"{152 - 1 - len(holes)}/151 dex slots filled")

    def _records(blob_key, off_key, count):
        blob = A[blob_key]["fn"](rom)
        offs = A[off_key]["fn"](rom)
        starts = [int.from_bytes(offs[i * 2:i * 2 + 2], "little")
                  for i in range(count)]
        return blob, starts

    blob, starts = _records("gMoveAnimFrameBlockBlob",
                            "gMoveAnimFrameBlockOffsets",
                            B.MOVE_ANIM_NUM_FRAMEBLOCKS)
    sprites = 0
    for o in starts:
        n = blob[o]
        sprites += n
        if o + 1 + n * 6 > len(blob):
            fail.append(f"frame block at {o} claims {n} sprites, past the blob end")
    print(f"anim frame blocks    : {len(starts)} blocks, {sprites} sprites")

    blob, starts = _records("gMoveAnimSubanimBlob", "gMoveAnimSubanimOffsets",
                            B.MOVE_ANIM_NUM_SUBANIMS)
    entries = 0
    for o in starts:
        n = blob[o + 1]
        entries += n
        if blob[o] > 7:
            fail.append(f"subanimation at {o} has type {blob[o]}, but the ROM "
                        f"packs type into 3 bits")
        if o + 2 + n * 3 > len(blob):
            fail.append(f"subanimation at {o} claims {n} entries, past the blob end")
    print(f"anim subanimations   : {len(starts)} subanims, {entries} entries")

    blob, starts = _records("gMoveAnimScriptBlob", "gMoveAnimScriptOffsets",
                            B.MOVE_ANIM_NUM_ATTACK_ANIMS)
    groups = empty = 0
    for o in starts:
        p = o
        while p < len(blob) and blob[p] != 0xFF:
            p += 2 if blob[p] >= B.MOVE_ANIM_FIRST_SE_ID else 3
            groups += 1
        if p >= len(blob):
            fail.append(f"animation script at {o} runs off the blob end")
        if p == o:
            empty += 1
    if empty:
        fail.append(f"{empty} animation script(s) are empty -- every move has "
                    f"at least one group in the ROM")
    print(f"anim scripts         : {len(starts)} scripts, {groups} groups")

    if fail:
        print(f"\nFAIL ({len(fail)}):")
        for f in fail:
            print(f"  x {f}")
        return 1
    print("\nOK")
    return 0

if __name__ == "__main__":
    sys.exit(main())
