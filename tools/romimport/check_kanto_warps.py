
import argparse
import io
import os
import re
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

WARPSPOT = re.compile(r"^warpspot\s+(\S+)\s+(\d+)\s+(\d+)\s+(\d+)\s*$")
WARP = re.compile(r"^warp\s+(\S+)\s+(\d+)\s*$")

DYNAMIC_DEST = "last"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom")
    ap.add_argument("--sym")
    args = ap.parse_args()

    dr, ds = default_paths(REPO)
    try:
        rom = Gen1Rom(args.rom or dr, args.sym or ds)
    except RomError as e:
        sys.exit("error: %s" % e)

    spots, trigs = {}, []

    landing, returns_to = {}, {}
    failed = 0
    for mid, name in enumerate(BP.MAP_NAMES):
        if (not name or mid >= BP.NUM_REAL_MAPS or mid in BP.MAP_FILLER
                or emit_kanto.is_duplicate_label(mid)):
            continue
        try:
            ob = emit_kanto.parse_objects(rom, mid)
            for i, (wx, wy, wdm, _wdw) in enumerate(
                    emit_kanto.PO.warps_for_map(mid, ob["warps"])):
                landing[(name, i)] = (wx, wy)
                dn = (BP.MAP_NAMES[wdm]
                      if wdm < len(BP.MAP_NAMES) and BP.MAP_NAMES[wdm] else None)
                returns_to[(name, (wx, wy))] = dn if wdm != 255 else "last"
        except Exception:
            pass
        try:
            text = emit_kanto.emit(rom, mid, name)
        except Exception as e:
            failed += 1
            print("  emit failed: %s (%s: %s)" % (name, type(e).__name__, e))
            continue
        for ln in text.splitlines():
            s = ln.strip()
            m = WARPSPOT.match(s)
            if m:
                spots.setdefault(m.group(1), set()).add(int(m.group(2)))
                continue
            m = WARP.match(s)
            if m:
                trigs.append((name, m.group(1), int(m.group(2))))

    bad = []
    dynamic = 0
    for src, dest, idx in trigs:
        if dest == DYNAMIC_DEST:
            dynamic += 1
            continue
        have = spots.get(dest)
        if have is None:
            bad.append((src, dest, idx, "destination defines no warpspot at all"))
        elif idx not in have:
            bad.append((src, dest, idx, "destination defines only %s"
                        % ",".join(str(i) for i in sorted(have))))

    for (osrc, ox, oy), val in sorted(emit_kanto.WARP_LAST_OVERRIDE.items()):
        odest, oidx = val.split()
        oidx = int(oidx)
        land = landing.get((odest, oidx))
        if land is None:
            bad.append((osrc, odest, oidx,
                        "override at (%d,%d): destination has no such warpspot"
                        % (ox, oy)))
            continue
        back = returns_to.get((odest, land))

        if back not in (osrc, "last"):
            want = sorted(i for (d, i), xy in landing.items()
                          if d == odest and returns_to.get((odest, xy)) == osrc)
            bad.append((osrc, odest, oidx,
                        "override at (%d,%d) lands on %s, which warps to %s, not "
                        "back here -- did a 1-based ROM literal get copied in? "
                        "correct index would be %s"
                        % (ox, oy, land, back or "nothing", want or "unknown")))

    import glob as _glob
    GB = os.path.join(REPO, "mod_runtime", "generatedmaps", "kanto", "blocks")
    GE = os.path.join(REPO, "mod_runtime", "generatedmaps", "kanto", "map_edits")
    for gpath in sorted(_glob.glob(os.path.join(GB, "*.block"))):
        mapname = os.path.basename(gpath)[:-len(".block")]
        text = io.open(gpath, encoding="utf-8", errors="replace").read()
        defs, cur = {}, None
        for line in text.splitlines():
            mm = re.match(r"\s*block\s+(\S+)", line)
            if mm:
                cur = mm.group(1); defs[cur] = {}; continue
            if not cur:
                continue
            mm = re.match(r"\s+passable\s+(\w+)", line)
            if mm: defs[cur]["pass"] = mm.group(1)
            mm = re.match(r"\s+warp\s+(\S+\s+\d+)", line)
            if mm: defs[cur]["warp"] = mm.group(1)
        walk_into = {(int(a), int(b)) for a, b in
                     re.findall(r"warp_walk_into\s+\S+\s+(\d+)\s+(\d+)", text)}
        ed = os.path.join(GE, "vmap_%s.txt" % mapname)
        if not os.path.exists(ed):
            continue
        for line in io.open(ed, encoding="utf-8", errors="replace"):
            t = line.split()
            if len(t) < 4:
                continue
            xy = (int(t[0]), int(t[1]))
            d = defs.get(t[3])
            if d and d.get("warp") and d.get("pass") == "no" and xy not in walk_into:
                bad.append((mapname, "(unreachable cell)", 0,
                            "(%d,%d) carries `warp %s` but the cell is impassable "
                            "and is not a walk-into door -- the player can never "
                            "trigger it" % (xy[0], xy[1], d["warp"])))

    print("%d warp triggers over %d maps with warpspots "
          "(%d dynamic `warp last`, skipped); %d hand-written overrides checked "
          "for reciprocity"
          % (len(trigs), len(spots), dynamic, len(emit_kanto.WARP_LAST_OVERRIDE)))
    if failed:
        print("%d map(s) FAILED TO EMIT -- not checked" % failed)
    if not bad:
        print("OK: every trigger resolves to a warpspot on its destination.")
        return 1 if failed else 0

    print("\n%d DANGLING TRIGGER(S) -- doors that would do nothing:" % len(bad))
    for src, dest, idx, why in bad:
        print("  %-26s -> %-26s idx %-3d  %s" % (src, dest, idx, why))
    print("\nworst destinations: %s"
          % ", ".join("%s(%d)" % kv for kv in Counter(b[1] for b in bad).most_common(6)))
    return 1

if __name__ == "__main__":
    sys.exit(main())
