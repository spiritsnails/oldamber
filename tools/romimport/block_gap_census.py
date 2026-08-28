
import argparse
import os
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(REPO / "tools" / "assetpack"))

import build_pak as BP
import emit_kanto
from gen1_rom import Gen1Rom, RomError, default_paths

TRACKED = REPO / "mod_runtime" / "blocks"

ENTITY = ["mapsize", "border", "indoor", "music", "connect", "warp", "warpspot",
          "npc", "trainer", "item_ball", "hidden_event", "hidden_item",
          "hidden_coin", "slot_machine", "grass", "tile_ledge", "tile_sign",
          "scene_trigger", "scene_npc", "static_encounter",
          "wild_encounter", "wild_rate"]

GEOMETRY = ["block", "subtile", "source", "passable", "quad", "tileset"]

def directives(text):
    c = Counter()
    warps = set()
    for ln in text.splitlines():
        s = ln.strip()
        if not s or s.startswith("#"):
            continue
        f = s.split()
        if f[0] == "warp" and len(f) >= 3:
            warps.add((f[1], f[2]))
            continue
        c[f[0]] += 1
    c["warp"] = len(warps)
    return c

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom")
    ap.add_argument("--sym")
    ap.add_argument("--map", help="census one map by NAME, and show its lines")
    args = ap.parse_args()

    dr, ds = default_paths(REPO)
    try:
        rom = Gen1Rom(args.rom or dr, args.sym or ds)
    except RomError as e:
        sys.exit(f"error: {e}")

    ids = [(i, n) for i, n in enumerate(BP.MAP_NAMES)
           if n and i < BP.NUM_REAL_MAPS and i not in BP.MAP_FILLER
           and not emit_kanto.is_duplicate_label(i)]
    if args.map:
        ids = [(i, n) for i, n in ids if n == args.map]
        if not ids:
            sys.exit(f"no such map: {args.map}")

    missing = Counter()
    short = Counter()
    extra = Counter()
    total_tracked = Counter()
    seen = failed = no_tracked = 0
    per_map_gaps = {}

    for mid, name in ids:
        try:
            emitted = directives(emit_kanto.emit(rom, mid, name))
        except Exception as e:
            failed += 1
            print(f"  emit failed: {name} ({type(e).__name__}: {e})")
            continue
        tf = TRACKED / f"{name}.block"
        if not tf.is_file():
            no_tracked += 1
            continue
        seen += 1
        tracked = directives(tf.read_text(encoding="utf-8", errors="replace"))
        gaps = []
        for k in ENTITY:
            e, t = emitted.get(k, 0), tracked.get(k, 0)
            total_tracked[k] += t
            if t and not e:
                missing[k] += 1
                gaps.append(f"{k}(0/{t})")
            elif e < t:
                short[k] += 1
                gaps.append(f"{k}({e}/{t})")
            elif e > t:
                extra[k] += 1
        if gaps:
            per_map_gaps[name] = gaps

    print(f"\n{seen} maps compared"
          + (f", {failed} failed to emit" if failed else "")
          + (f", {no_tracked} with no tracked .block" if no_tracked else ""))

    print(f"\n{'directive':<16}{'ABSENT':>8}{'short':>8}{'extra':>8}"
          f"{'tracked total':>15}")
    print(f"{'':<16}{'(maps)':>8}{'(maps)':>8}{'(maps)':>8}{'(lines)':>15}")
    for k in ENTITY:
        if not (missing[k] or short[k] or extra[k] or total_tracked[k]):
            continue
        print(f"{k:<16}{missing[k]:>8}{short[k]:>8}{extra[k]:>8}"
              f"{total_tracked[k]:>15}")

    hard = [k for k in ENTITY if missing[k] and missing[k] == seen]
    if hard:
        print(f"\nNEVER EMITTED ON ANY MAP -- the emitter cannot say these at "
              f"all yet:\n  {', '.join(hard)}")
    part = [k for k in ENTITY if missing[k] and missing[k] != seen]
    if part:
        print(f"\nEMITTED SOMETIMES -- present on some maps, absent on others:"
              f"\n  {', '.join(f'{k} (absent on {missing[k]})' for k in part)}")

    if args.map and per_map_gaps:
        for name, gaps in per_map_gaps.items():
            print(f"\n{name}: {' '.join(gaps)}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
