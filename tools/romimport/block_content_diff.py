
import argparse
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

TRACKED = REPO / "mod_runtime" / "blocks"

COMPARED = ["warpspot", "npc", "trainer", "item_ball", "hidden_event",
            "hidden_item", "hidden_coin", "slot_machine", "wild_encounter",
            "wild_rate", "music", "connect", "mapsize", "indoor",
            "static_encounter", "tile_ledge", "tile_sign", "grass",
            "warp_walk_into", "warp_stair", "text_if", "badge_if"]

MODIFIER = {"text_if", "badge_if", "hide_if", "show_if", "after_battle",
            "text_random", "no_face_until"}

SKIPPED = {"subtile", "block", "source", "passable", "end", "border",
           "gbc_tileset", "scene_trigger", "scene_npc"}

SPRITE_TOKEN = re.compile(r"\s+sprite:\w+")

TRAINER_CLASS = re.compile(r"^(\S+)")

EVENT_TOKEN = re.compile(r"\bEVENT_\w+")

def canon_trainer(line, id_by_name, flag_by_name):
    line = EVENT_TOKEN.sub(
        lambda m: str(flag_by_name.get(m.group(0), m.group(0))), line)
    m = TRAINER_CLASS.match(line)
    if not m:
        return line
    cid = id_by_name.get(m.group(1))
    return ("%d%s" % (cid, line[m.end():])) if cid else line

def lines_by_directive(text):
    out = {}
    for ln in text.splitlines():
        s = ln.strip()
        if not s or s.startswith("#"):
            continue
        f = s.split()
        d = f[0]
        if d in SKIPPED or d not in COMPARED:
            continue

        if d == "grass" and len(f) == 2 and f[1] in ("yes", "no"):
            continue

        rest = " ".join(f[1:] if d in MODIFIER else f[2:]) if len(f) > 1 else ""
        rest = re.sub(r"\s+", " ", rest).strip()
        out.setdefault(d, Counter())[rest] += 1
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom")
    ap.add_argument("--sym")
    ap.add_argument("--map", help="one map by name")
    ap.add_argument("--directive", help="only this directive")
    ap.add_argument("--show", type=int, default=2,
                    help="example differing lines to print per map/directive")
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

    id_by_name = {v: k for k, v in emit_kanto.trainer_class_names(rom).items()}
    flag_by_name = {v: k for k, v in emit_kanto.event_flag_names().items()}

    agree = Counter()
    only_e = Counter()
    only_t = Counter()
    maps_bad = Counter()
    examples = {}
    sprite_added = 0
    compared = 0

    for mid, name in ids:
        tf = TRACKED / f"{name}.block"
        if not tf.is_file():
            continue
        try:
            em = emit_kanto.emit(rom, mid, name)
        except Exception:
            continue
        compared += 1
        E = lines_by_directive(em)
        T = lines_by_directive(tf.read_text(encoding="utf-8", errors="replace"))

        for d in set(E) | set(T):
            if args.directive and d != args.directive:
                continue
            e, t = Counter(E.get(d, {})), Counter(T.get(d, {}))
            if d == "trainer":

                def strip(c):
                    out = Counter()
                    for k, v in c.items():
                        out[canon_trainer(SPRITE_TOKEN.sub("", k).strip(),
                                          id_by_name, flag_by_name)] += v
                    return out
                sprite_added += sum(1 for k in e if SPRITE_TOKEN.search(k))
                e, t = strip(e), strip(t)
            agree[d] += sum((e & t).values())
            eo, to = e - t, t - e
            if eo or to:
                maps_bad[d] += 1
                only_e[d] += sum(eo.values())
                only_t[d] += sum(to.values())
                ex = examples.setdefault(d, [])
                if len(ex) < args.show:
                    ex.append((name, list(eo)[:1], list(to)[:1]))

    print(f"{compared} maps compared, entity layer only "
          f"(geometry excluded -- see the docstring)\n")
    print(f"{'directive':<18}{'identical':>10}{'emitted-only':>14}"
          f"{'tracked-only':>14}{'maps differing':>16}")
    total_ok = total_bad = 0
    for d in sorted(set(list(agree) + list(only_e) + list(only_t))):
        print(f"{d:<18}{agree[d]:>10}{only_e[d]:>14}"
              f"{only_t[d]:>14}{maps_bad[d]:>16}")
        total_ok += agree[d]
        total_bad += only_e[d] + only_t[d]
    print(f"\n{total_ok} lines identical, {total_bad} differing")
    if sprite_added:
        print(f"({sprite_added} trainer lines carry an emitter-added "
              f"sprite: token, compared without it -- see the docstring)")

    for d, ex in sorted(examples.items()):
        print(f"\n--- {d}")
        for name, eo, to in ex:
            if eo:
                print(f"  {name}  emitted only: {eo[0][:96]}")
            if to:
                print(f"  {name}  tracked only: {to[0][:96]}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
