
import argparse
import os
import subprocess
import sys
from collections import deque

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import check_connections
import check_field
import check_warps
import crystal_maps as M
import crystal_script
from crystal_rom import Rom

PC = os.path.join(ROOT, "pokecrystal-master")

def neighbours(rom, id_to_name, name):
    g, m = id_to_name[1][name]
    hdr = M.read_map_header(rom, g, m)
    attr = M.read_attributes(rom, *hdr["attributes"], id_to_name[0])
    out = set()
    for c in attr["connections"]:
        if c["dest"]:
            out.add(c["dest"])
    ev = M.read_events(rom, *attr["events"])
    for w in ev["warps"]:
        d = id_to_name[0].get((w["group"], w["map"]))
        if d:
            out.add(d)
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("seed", help="map to start from, e.g. NewBarkTown")
    ap.add_argument("--limit", type=int, default=0,
                    help="stop after this many maps (0 = the whole component)")
    ap.add_argument("--rom", default=os.path.join(PC, "pokecrystal.gbc"))
    ap.add_argument("--sym", default=os.path.join(PC, "pokecrystal.sym"))
    ap.add_argument("--allow-missing-tiles", action="store_true")
    args = ap.parse_args()

    rom = Rom(args.rom, args.sym)
    table = M.build_map_table(rom)
    if args.seed not in table[1]:
        sys.exit(f"unknown map {args.seed!r}")

    order, seen, q = [], {args.seed}, deque([args.seed])
    while q:
        cur = q.popleft()
        order.append(cur)
        if args.limit and len(order) >= args.limit:
            break
        for n in sorted(neighbours(rom, table, cur)):
            if n not in seen:
                seen.add(n)
                q.append(n)
    print(f"reachable from {args.seed}: {len(order)} maps")

    ok, failed = [], []
    for i, name in enumerate(order, 1):
        cmd = [sys.executable, os.path.join(HERE, "emit_map.py"), name]
        if args.allow_missing_tiles:
            cmd.append("--allow-missing-tiles")
        r = subprocess.run(cmd, capture_output=True, encoding="utf-8",
                           errors="replace", cwd=ROOT)
        if r.returncode == 0:
            ok.append(name)
        else:
            why = (r.stdout + r.stderr).strip().splitlines()
            failed.append((name, why[-1] if why else "?"))
        if i % 20 == 0 or i == len(order):
            print(f"  {i}/{len(order)}  ok={len(ok)} failed={len(failed)}",
                  flush=True)

    print(f"\nimported {len(ok)}, failed {len(failed)}")

    for name, why in failed:
        print(f"  !! {name}: {why[:150]}")

    print("\n--- connections ---")
    conns_ok = check_connections.check(check_connections.DEFAULT_BLOCKS)

    print("\n--- dialogue ---")
    text_ok = crystal_script.verify_texts()

    print("\n--- warps and field coverage ---")
    warps_ok = check_warps.check()

    print("\n--- field properties ---")
    field_ok = check_field.check()

    return 0 if (not failed and conns_ok and text_ok and warps_ok
                 and field_ok) else 1

if __name__ == "__main__":
    sys.exit(main())
