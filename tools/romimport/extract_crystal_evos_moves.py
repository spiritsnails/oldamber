
import argparse
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

from crystal_rom import Rom
import crystal_maps as M

PC = os.path.join(ROOT, "pokecrystal-master")
NUM_SPECIES = 251

CONSTS = M.const_table(os.path.join(PC, "constants", "pokemon_data_constants.asm"))
EVOLVE_STAT = CONSTS["EVOLVE_STAT"]
EVOLVE_NAMES = {CONSTS[n]: n for n in
                ("EVOLVE_LEVEL", "EVOLVE_ITEM", "EVOLVE_TRADE",
                 "EVOLVE_HAPPINESS", "EVOLVE_STAT")}

def read_species(rom, bank, addr):
    evos, moves = [], []
    p = addr
    while True:
        method = rom.u8(bank, p)
        if method == 0:
            p += 1
            break
        if method not in EVOLVE_NAMES:
            raise ValueError(f"evolution method {method} at {bank:02X}:{p:04X} "
                             f"is not one of {sorted(EVOLVE_NAMES)} -- wrong ROM "
                             f"or a desynchronised read")
        if method == EVOLVE_STAT:
            evos.append((method, rom.u8(bank, p + 1), rom.u8(bank, p + 2),
                         rom.u8(bank, p + 3)))
            p += 4
        else:
            evos.append((method, rom.u8(bank, p + 1), 0, rom.u8(bank, p + 2)))
            p += 3
    while True:
        level = rom.u8(bank, p)
        if level == 0:
            break
        moves.append((level, rom.u8(bank, p + 1)))
        p += 2
    return evos, moves

def oracle(disasm, species_of, moves_of, items_of):
    path = os.path.join(disasm, "data", "pokemon", "evos_attacks.asm")
    out, cur = {}, None
    stat_cmp = {n: CONSTS[n] for n in ("ATK_LT_DEF", "ATK_GT_DEF", "ATK_EQ_DEF")}
    tr = {n: CONSTS[n] for n in ("TR_ANYTIME", "TR_MORNDAY", "TR_NITE")}
    for line in open(path, encoding="utf-8", errors="replace"):
        line = line.split(";")[0].strip()
        m = re.match(r"^(\w+)EvosAttacks:", line)
        if m:
            cur = m.group(1)
            out[cur] = ([], [])
            continue
        if cur is None or not line.startswith("db "):
            continue
        args = [a.strip() for a in line[3:].split(",")]
        if args[0].startswith("EVOLVE_"):
            method = {v: k for k, v in EVOLVE_NAMES.items()}[args[0]]
            if method == EVOLVE_STAT:
                out[cur][0].append((method, int(args[1]),
                                    stat_cmp[args[2]], species_of[args[3]]))
            else:
                p = args[1]
                if p in tr:
                    val = tr[p]
                elif p in items_of:
                    val = items_of[p]
                elif re.match(r"^-?\d+$", p):
                    val = int(p) & 0xFF
                else:
                    val = species_of.get(p, 0)
                out[cur][0].append((method, val, 0, species_of[args[2]]))
        elif len(args) == 2 and args[1] in moves_of:
            out[cur][1].append((int(args[0]), moves_of[args[1]]))
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=os.path.join(PC, "pokecrystal.gbc"))
    ap.add_argument("--sym", default=os.path.join(PC, "pokecrystal.sym"))
    ap.add_argument("--verify", action="store_true")
    args = ap.parse_args()

    rom = Rom(args.rom, args.sym)
    pbank, paddr = rom.sym["EvosAttacksPointers"]

    pokemon = M.const_table(os.path.join(PC, "constants", "pokemon_constants.asm"))
    species_of = {k: v for k, v in pokemon.items()}
    name_of = {}
    for k, v in pokemon.items():
        name_of.setdefault(v, k)
    moves_of = M.const_table(os.path.join(PC, "constants", "move_constants.asm"))
    items_of = M.const_table(os.path.join(PC, "constants", "item_constants.asm"))

    data = {}
    for dex in range(1, NUM_SPECIES + 1):
        ptr = rom.u16(pbank, paddr + (dex - 1) * 2)
        data[dex] = read_species(rom, pbank, ptr)

    n_ev = sum(len(e) for e, _ in data.values())
    n_mv = sum(len(m) for _, m in data.values())
    print(f"{NUM_SPECIES} species, {n_ev} evolutions, {n_mv} level-up moves")

    if args.verify:
        want = oracle(PC, species_of, moves_of, items_of)

        def norm(s):
            return "".join(c for c in s.lower() if c.isalnum())
        by_norm = {norm(n): d for d, n in name_of.items()}
        ok = bad = 0
        for label, (evos, moves) in want.items():
            dex = by_norm.get(norm(label))
            if dex is None or dex > NUM_SPECIES:
                continue
            got_e, got_m = data[dex]
            for what, a, b in (("evos", got_e, evos), ("moves", got_m, moves)):
                if a == b:
                    ok += 1
                else:
                    bad += 1
                    print(f"FAIL {label} ({dex}): {what} differ")
                    print(f"  rom  {a}")
                    print(f"  want {b}")
        print(f"verify: {ok} checks passed, {bad} failed")
        if bad:
            sys.exit("verification failed -- not writing anything")

    out_dir = os.path.join(ROOT, "generated")
    os.makedirs(out_dir, exist_ok=True)
    banner = ("/* {} -- GENERATED by tools/romimport/extract_crystal_evos_moves.py\n"
              " * from pokecrystal.gbc.  Evolutions and level-up learnsets for all\n"
              " * 251 species.  ROM-derived: never commit this.\n"
              " */\n")

    with open(os.path.join(out_dir, "gen2_evos_moves.h"), "w", newline="\n") as f:
        f.write(banner.format("gen2_evos_moves.h"))
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write(f"#define GEN2_EVOS_NUM_SPECIES {NUM_SPECIES}\n\n")
        f.write("/* method values are pokecrystal's EVOLVE_* (1-based, in\n"
                ""
                " * id or happiness trigger; `cmp` is only meaningful for\n"
                " * EVOLVE_STAT (0 = ATK_LT_DEF, 1 = GT, 2 = EQ). */\n")
        f.write("typedef struct {\n"
                "    uint8_t method, param, cmp, target;  /* target is a DEX number */\n"
                "} gen2_evolution_t;\n\n")
        f.write("typedef struct {\n"
                "    uint8_t level, move;\n"
                "} gen2_level_move_t;\n\n")
        f.write("typedef struct {\n"
                "    const gen2_evolution_t  *evos;   /* NULL if the species does not evolve */\n"
                "    const gen2_level_move_t *moves;\n"
                "    uint8_t num_evos, num_moves;\n"
                "} gen2_evos_moves_t;\n\n")
        f.write("/* Indexed by DEX NUMBER - 1. */\n")
        f.write("extern const gen2_evos_moves_t gGen2EvosMoves[GEN2_EVOS_NUM_SPECIES];\n\n")
        f.write("/* The four moves a species of this dex number knows at `level`,\n"
                " * by Gen 1's rule: walk the learnset in order and keep the last\n"
                " * four learned at or below that level. Returns how many were\n"
                " * written (0-4); out_moves is zero-filled first. */\n")
        f.write("int Gen2EvosMoves_MovesAtLevel(uint8_t dex, uint8_t level, uint8_t out_moves[4]);\n")

    with open(os.path.join(out_dir, "gen2_evos_moves.c"), "w", newline="\n") as f:
        f.write(banner.format("gen2_evos_moves.c"))
        f.write('#include "gen2_evos_moves.h"\n#include <string.h>\n\n')
        for dex in range(1, NUM_SPECIES + 1):
            evos, moves = data[dex]
            nm = name_of.get(dex, f"DEX{dex}")
            if evos:
                f.write(f"static const gen2_evolution_t kEvos{dex}[] = {{ /* {nm} */\n")
                for (me, pa, cm, tg) in evos:
                    f.write(f"    {{ {me}, {pa}, {cm}, {tg} }},\n")
                f.write("};\n")
            if moves:
                f.write(f"static const gen2_level_move_t kMoves{dex}[] = {{ /* {nm} */\n")
                f.write("    " + " ".join(f"{{ {lv}, {mv} }}," for lv, mv in moves) + "\n")
                f.write("};\n")
        f.write("\nconst gen2_evos_moves_t gGen2EvosMoves[GEN2_EVOS_NUM_SPECIES] = {\n")
        for dex in range(1, NUM_SPECIES + 1):
            evos, moves = data[dex]
            e = f"kEvos{dex}" if evos else "0"
            m = f"kMoves{dex}" if moves else "0"
            f.write(f"    /* {dex:3d} {name_of.get(dex,'?'):14s} */ "
                    f"{{ {e}, {m}, {len(evos)}, {len(moves)} }},\n")
        f.write("};\n\n")
        f.write("""int Gen2EvosMoves_MovesAtLevel(uint8_t dex, uint8_t level, uint8_t out_moves[4]) {
    const gen2_evos_moves_t *e;
    int n = 0;
    memset(out_moves, 0, 4);
    if (dex < 1 || dex > GEN2_EVOS_NUM_SPECIES) return 0;
    e = &gGen2EvosMoves[dex - 1];
    for (int i = 0; i < e->num_moves; i++) {
        if (e->moves[i].level > level) break;   /* the table is level-ordered */
        /* Gen 1's LearnMoves: a full moveset shifts the oldest move out. */
        if (n < 4) {
            out_moves[n++] = e->moves[i].move;
        } else {
            out_moves[0] = out_moves[1];
            out_moves[1] = out_moves[2];
            out_moves[2] = out_moves[3];
            out_moves[3] = e->moves[i].move;
        }
    }
    return n;
}
""")
    print(f"wrote {out_dir}\\gen2_evos_moves.h / .c")
    return 0

if __name__ == "__main__":
    sys.exit(main())
