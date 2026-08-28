
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import crystal_maps as M
from crystal_rom import Rom

PC = os.path.join(ROOT, "pokecrystal-master")
DEFAULT_ROM = os.path.join(PC, "pokecrystal.gbc")
DEFAULT_SYM = os.path.join(PC, "pokecrystal.sym")

def oracle_blocks(name):
    p = os.path.join(PC, "maps", name + ".blk")
    return open(p, "rb").read() if os.path.exists(p) else None

def oracle_metatiles(tileset_name):
    p = os.path.join(PC, "data", "tilesets", tileset_name + "_metatiles.bin")
    if not os.path.exists(p):
        return None
    raw = open(p, "rb").read()
    return [raw[i:i + 16] for i in range(0, len(raw), 16)]

def collision_constants():
    p = os.path.join(PC, "constants", "collision_constants.asm")
    if not os.path.exists(p):
        return None

    vals = {}
    for line in open(p, encoding="utf-8", errors="replace"):
        m = re.match(r"\s*DEF\s+(COLL_\w+)\s+EQU\s+\$([0-9A-Fa-f]+)", line)
        if m:
            vals[m.group(1)] = int(m.group(2), 16)
    return vals

def snake(camel):
    return re.sub(r"(?<!^)(?=[A-Z])", "_", camel).lower()

def oracle_collision(tileset_name):
    p = os.path.join(PC, "data", "tilesets", tileset_name + "_collision.asm")
    if not os.path.exists(p):
        return None
    names = collision_constants()
    if not names:
        return None
    out = []
    for line in open(p, encoding="utf-8", errors="replace"):
        m = re.match(r"\s*tilecoll\s+(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)", line)
        if m:

            try:
                out.append(tuple(names["COLL_" + g] for g in m.groups()))
            except KeyError as e:
                return ("unresolved", str(e))
    return out

def oracle_events(name):
    p = os.path.join(PC, "maps", name + ".asm")
    if not os.path.exists(p):
        return None
    src = open(p, encoding="utf-8", errors="replace").read()
    warps, bgs, objs = [], [], []
    for m in re.finditer(r"^\s*warp_event\s+(\d+)\s*,\s*(\d+)\s*,\s*(\w+)\s*,\s*(\d+)",
                         src, re.M):
        warps.append((int(m.group(1)), int(m.group(2)), int(m.group(4))))
    for m in re.finditer(r"^\s*bg_event\s+(\d+)\s*,\s*(\d+)\s*,\s*(\w+)\s*,", src, re.M):
        bgs.append((int(m.group(1)), int(m.group(2)), m.group(3)))
    for m in re.finditer(r"^\s*object_event\s+(-?\d+)\s*,\s*(-?\d+)\s*,\s*(\w+)\s*,\s*(\w+)"
                         r"\s*,\s*(-?\d+)\s*,\s*(-?\d+)", src, re.M):
        objs.append((int(m.group(1)), int(m.group(2)), m.group(3), m.group(4)))
    return {"warps": warps, "bg_events": bgs, "object_events": objs}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("map", help="CamelCase map name, e.g. NewBarkTown")
    ap.add_argument("--rom", default=DEFAULT_ROM)
    ap.add_argument("--sym", default=DEFAULT_SYM)
    ap.add_argument("--verify", action="store_true")
    args = ap.parse_args()

    for p in (args.rom, args.sym):
        if not os.path.exists(p):
            sys.exit(f"missing {p}\n  cd pokecrystal-master && make")

    rom = Rom(args.rom, args.sym)
    id_to_name, name_to_id = M.build_map_table(rom)
    print(f"map table: {len(name_to_id)} maps across "
          f"{len(M.group_pointers(rom))} groups")

    if args.map not in name_to_id:
        near = [n for n in name_to_id if n.lower().startswith(args.map.lower()[:5])]
        sys.exit(f"unknown map {args.map!r}; did you mean {near[:6]}?")
    group, mapno = name_to_id[args.map]
    print(f"\n=== {args.map}  (group {group}, map {mapno}) ===")

    hdr = M.read_map_header(rom, group, mapno)
    attr = M.read_attributes(rom, *hdr["attributes"], id_to_name)
    print(f"tileset {hdr['tileset']}  environment {hdr['environment']}  "
          f"landmark {hdr['landmark']}  music {hdr['music']}  "
          f"tod {hdr['time_of_day']}  fishgroup {hdr['fishing_group']}")
    print(f"size {attr['width']}x{attr['height']} blocks   "
          f"border ${attr['border_block']:02X}   "
          f"connections ${attr['connection_mask']:02X}")
    for c in attr["connections"]:
        print(f"  {c['dir']:5s} -> {c['dest'] or '?'} "
              f"(group {c['group']}, map {c['map']})  "
              f"strip {c['strip_len']} of width {c['dest_width']}  "
              f"arrive y={c['y']} x={c['x']}")

    blocks = M.read_blocks(rom, *attr["blocks"], attr["width"], attr["height"])
    print(f"\nblockdata: {len(blocks)} bytes, "
          f"{len(set(blocks))} distinct block ids")

    ev = M.read_events(rom, *attr["events"])
    print(f"\nevents: {len(ev['warps'])} warps, {len(ev['coord_events'])} coord, "
          f"{len(ev['bg_events'])} bg, {len(ev['object_events'])} objects")
    for w in ev["warps"]:
        print(f"  warp   ({w['x']:2d},{w['y']:2d}) -> "
              f"{id_to_name.get((w['group'], w['map']), '?')} #{w['dest_warp']}")
    for b in ev["bg_events"]:
        print(f"  sign   ({b['x']:2d},{b['y']:2d}) function {b['function']} "
              f"script ${b['script']:04X}")
    for o in ev["object_events"]:
        print(f"  object ({o['x']:2d},{o['y']:2d}) sprite {o['sprite']:3d} "
              f"move {o['movement']:2d} radius {o['radius_x']}x{o['radius_y']} "
              f"type {o['type']} sight {o['sight_range']} "
              f"flag ${o['event_flag']:04X}")

    ts = M.read_tileset_entry(rom, hdr["tileset"])
    nmeta = M.metatile_count(rom, ts)
    meta = M.read_metatiles(rom, *ts["meta"], count=nmeta)
    coll = M.read_collision(rom, *ts["coll"], count=nmeta)
    gfx = M.read_tileset_gfx(rom, *ts["gfx"])
    palmap = M.read_palette_map(rom, *ts["palmap"], count=nmeta)
    tsname = M.tileset_name(rom, ts)
    print(f"\ntileset {tsname or '<UNRESOLVED>'} (id {hdr['tileset']}): "
          f"{len(gfx)} tiles, {len(meta)} metatiles, "
          f"{len(coll)} collision entries, {len(palmap)} palette entries")

    if not args.verify:
        return 0

    print("\n--- verify against the disassembly (oracle only) ---")
    ok = True

    o = oracle_blocks(args.map)
    if o is None:
        print("  blocks: no oracle file"); ok = False
    else:
        same = bytes(blocks) == o
        print(f"  blocks: {'MATCH' if same else 'DIFFER'} "
              f"({len(blocks)} vs {len(o)} bytes)")
        ok &= same

    if tsname is None:

        print("  tileset: name UNRESOLVED -- metatiles and collision unverified")
        ok = False
    om = oracle_metatiles(snake(tsname)) if tsname else None
    if om is None:
        if tsname:
            print(f"  metatiles: NO ORACLE for {snake(tsname)}")
            ok = False
    else:
        n = min(len(om), len(meta))
        bad = [i for i in range(n) if bytes(meta[i]) != om[i]]
        print(f"  metatiles: {n - len(bad)}/{n} match"
              + (f"  first bad: {bad[:5]}" if bad else ""))
        ok &= not bad

    oc = oracle_collision(snake(tsname)) if tsname else None
    if oc is None:
        if tsname:
            print(f"  collision: NO ORACLE for {snake(tsname)}")
            ok = False
    elif isinstance(oc, tuple) and oc and oc[0] == "unresolved":
        print(f"  collision: oracle has an unknown constant {oc[1]}")
        ok = False
    else:
        n = min(len(oc), len(coll))
        bad = [i for i in range(n) if coll[i] != oc[i]]
        print(f"  collision: {n - len(bad)}/{n} match"
              + (f"  first bad: {[(i, coll[i], oc[i]) for i in bad[:3]]}" if bad else ""))
        ok &= not bad

    oe = oracle_events(args.map)
    if oe is None:
        print("  events: no oracle file"); ok = False
    else:
        got = [(w["x"], w["y"], w["dest_warp"]) for w in ev["warps"]]
        print(f"  warps: {'MATCH' if got == oe['warps'] else 'DIFFER'}"
              + ("" if got == oe["warps"] else f"\n    rom={got}\n    asm={oe['warps']}"))
        ok &= got == oe["warps"]

        got = [(b["x"], b["y"]) for b in ev["bg_events"]]
        want = [(x, y) for (x, y, _f) in oe["bg_events"]]
        print(f"  bg events: {'MATCH' if got == want else 'DIFFER'}"
              + ("" if got == want else f"\n    rom={got}\n    asm={want}"))
        ok &= got == want

        got = [(o_["x"], o_["y"]) for o_ in ev["object_events"]]
        want = [(x, y) for (x, y, _s, _m) in oe["object_events"]]
        print(f"  object events: {'MATCH' if got == want else 'DIFFER'}"
              + ("" if got == want else f"\n    rom={got}\n    asm={want}"))
        ok &= got == want

    print("\nVERIFY " + ("PASSED" if ok else "FAILED"))
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
