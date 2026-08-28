
import argparse
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
DEFAULT_GEN = os.path.join(ROOT, "mod_runtime", "generatedmaps", "johto")

LAND_TILE = 0x00
WATER_TILE = 0x01
WALL_TILE = 0x0F
QUADS = {(0, 0): (0, 1, 4, 5), (1, 0): (2, 3, 6, 7),
         (0, 1): (8, 9, 12, 13), (1, 1): (10, 11, 14, 15)}
COLL_INDEX = {(0, 0): 0, (1, 0): 1, (0, 1): 2, (1, 1): 3}
PROPS = ("passable", "grass", "grass_rustle", "surfable", "cuttable")

def read_blocks_file(path):
    out, cur = {}, None
    for line in open(path, encoding="utf-8", errors="replace"):
        s = line.strip()
        if s.startswith("block "):
            cur = s.split()[1]
            out[cur] = {"props": set(), "tiles": [], "cut": None}
        elif s == "end":
            cur = None
        elif cur is None:
            continue
        elif s.startswith("source quad "):

            out[cur]["tiles"] = [int(t.rsplit("_t", 1)[1]) for t in s.split()[2:]]
        elif s.startswith("cut_replacement "):
            out[cur]["cut"] = s.split()[1]
        else:
            f = s.split()
            if len(f) == 2 and f[0] in PROPS and f[1] == "yes":
                out[cur]["props"].add(f[0])
    return out

def emitted_cells(vmap_path, defs):
    got = {p: set() for p in PROPS}
    at = {}
    for line in open(vmap_path, encoding="utf-8", errors="replace"):
        m = re.match(r"^(\d+)\s+(\d+)\s+custom\s+(\S+)", line.strip())
        if not m:
            continue
        cell = (int(m.group(1)), int(m.group(2)))
        name = m.group(3)
        at[cell] = name
        for p in defs.get(name, {}).get("props", ()):
            got[p].add(cell)
    return got, at

def rom_cells(rom, tab, perms, enc, cut_coll, name):
    g, mno = tab[1][name]
    hdr = M.read_map_header(rom, g, mno)
    attr = M.read_attributes(rom, *hdr["attributes"], tab[0])
    ts = M.read_tileset_entry(rom, hdr["tileset"])
    n = M.metatile_count(rom, ts)
    coll = M.read_collision(rom, *ts["coll"], count=n)
    meta = M.read_metatiles(rom, *ts["meta"], count=n)
    blocks = M.read_blocks(rom, *attr["blocks"], attr["width"], attr["height"])
    cut_table = M.cut_tree_blocks(rom).get(hdr["tileset"], {})

    want = {p: set() for p in PROPS}
    cut_at = {}
    for cy in range(attr["height"] * 2):
        for cx in range(attr["width"] * 2):
            mi = blocks[(cy // 2) * attr["width"] + (cx // 2)]
            q = (cx % 2, cy % 2)
            cid = coll[mi][COLL_INDEX[q]]
            p = perms[cid] if cid < len(perms) else WALL_TILE
            if cid in enc and p != WATER_TILE:
                want["grass"].add((cx, cy))
            if M.grass_rustles(cid):
                want["grass_rustle"].add((cx, cy))
            if p == WATER_TILE:
                want["surfable"].add((cx, cy))

            if p == LAND_TILE:
                want["passable"].add((cx, cy))
            if mi in cut_table and cid in cut_coll:
                want["cuttable"].add((cx, cy))
            if mi in cut_table:
                cut_at[(cx, cy)] = (cut_table[mi][0], q)
    return want, cut_at, meta, n

def check(gen_dir=DEFAULT_GEN, only=None, verbose=False):
    rom = Rom(os.path.join(PC, "pokecrystal.gbc"),
              os.path.join(PC, "pokecrystal.sym"))
    tab = M.build_map_table(rom)
    enc = M.grass_encounter_set(rom)
    cut_coll = M.cut_collision_set(rom)
    pb, pa = rom.addr_of("CollisionPermissionTable")
    pend = rom.next_symbol_addr(pb, pa)
    perms = list(rom.read(pb, pa, (pend - pa) if pend else 256))

    blocks_dir = os.path.join(gen_dir, "blocks")
    edits_dir = os.path.join(gen_dir, "map_edits")
    if not os.path.isdir(blocks_dir):
        print(f"  no emitted maps at {blocks_dir}")
        return True

    names = sorted(f[:-6] for f in os.listdir(blocks_dir) if f.endswith(".block"))
    if only:
        names = [n for n in names if n in set(only)]

    checked = failed = 0
    totals = {p: 0 for p in PROPS}
    maps_with = {p: 0 for p in PROPS}
    cut_ok = cut_bad = cut_unresolved = 0

    for name in names:
        vmap = os.path.join(edits_dir, f"vmap_{name}.txt")
        if name not in tab[1] or not os.path.exists(vmap):
            continue
        want, cut_at, meta, nmeta = rom_cells(
            rom, tab, perms, enc, cut_coll, name)
        defs = read_blocks_file(os.path.join(blocks_dir, f"{name}.block"))
        got, at = emitted_cells(vmap, defs)
        checked += 1
        bad = False

        for p in PROPS:
            totals[p] += len(want[p])
            if want[p]:
                maps_with[p] += 1
            miss, extra = want[p] - got[p], got[p] - want[p]
            if miss or extra:
                bad = True
                print(f"  !! {name}: {p} -- {len(miss)} cell(s) the ROM calls "
                      f"{p} are untagged, {len(extra)} tagged that the ROM "
                      f"does not")
                for c in sorted(miss)[:4]:
                    print(f"       missing {c}")
                for c in sorted(extra)[:4]:
                    print(f"       extra   {c}")

        for cell, (repl_mi, q) in cut_at.items():
            blk = defs.get(at.get(cell), {})
            rq = blk.get("cut")
            if rq is None:
                cut_unresolved += 1
                continue
            want_tiles = [meta[repl_mi][i] for i in QUADS[q]] \
                if repl_mi < nmeta else None
            got_tiles = defs.get(rq, {}).get("tiles")
            if want_tiles is not None and got_tiles == want_tiles:
                cut_ok += 1
            else:
                cut_bad += 1
                bad = True
                if cut_bad <= 4:
                    print(f"  !! {name}: cut_replacement at {cell} -> {rq} "
                          f"has tiles {got_tiles}, ROM block ${repl_mi:02X} "
                          f"quadrant {q} has {want_tiles}")
        if bad:
            failed += 1
        elif verbose and any(want[p] for p in PROPS):
            print("  ok " + name + ": "
                  + ", ".join(f"{len(want[p])} {p}" for p in PROPS if want[p]))

    print(f"field: {checked - failed}/{checked} maps match the ROM cell-for-cell")
    for p in PROPS:
        print(f"    {p:13s} {totals[p]:6d} cells across {maps_with[p]:3d} maps")
    print(f"    cut_replacement  {cut_ok} verified against the ROM's own block art"
          + (f", {cut_bad} WRONG" if cut_bad else "")
          + (f", {cut_unresolved} not emitted" if cut_unresolved else ""))
    return failed == 0

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("maps", nargs="*", help="only these maps (default: all)")
    ap.add_argument("--gen", default=DEFAULT_GEN)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()
    return 0 if check(args.gen, args.maps or None, args.verbose) else 1

if __name__ == "__main__":
    sys.exit(main())
