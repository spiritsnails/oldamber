
import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build_pak as B

REPO = Path(__file__).resolve().parent.parent.parent

ACCOUNTED = {
    "src/data/gbc_palettes.c:*":
        "colour, not artwork -- RGB555 values and which palette a thing draws "
        "with. Hand-maintained on purpose; nothing generates it.",
    "gMoveAnimSpecialEffectPointers":
        "maps a ROM id onto a ROUTINE NAME. The ROM holds the address the "
        "assembler resolved, which says nothing about which routine this port "
        "dispatches to -- the name is ours.",
    "gMoveAnimTilesetAsmLabels":
        "same: ROM tileset index -> the label this port looks the tiles up by.",
    "gMoveAnimTilesetTileCounts":
        "three counts that describe the port's own tile budget.",
    "kSlots":
        "NOT AN ASSET -- a function-local static in party_icon_data.c listing "
        "which OAM slots the icon loader writes to. It is the port's own tile "
        "allocation, has no ROM counterpart, and only appears here because a "
        "regex over `static const` cannot tell a local from a data table.",
    "src/data/npc_script_bindings.c:*":
        "maps a ROM key (map id, text id, sprite) onto a C FUNCTION. The "
        "function is the port's; the ROM has no such thing. Generated once "
        "from event_data.c before it was removed, hand-maintained since.",
    "src/data/hidden_events.c:*":
        "hidden interaction points -- PCs, gym trash cans, Cinnabar's quiz "
        "switches. Each is a position plus a POINTER TO HAND-WRITTEN C, and a "
        "pack cannot hold a function pointer. The ROM has the positions; "
        "extracting only those would split one small table across two "
        "sources and still leave every pointer to write by hand.",
}

EXPECT_EMPTY = {
    "move_anim_basecoords.c", "move_anim_frameblocks.c", "move_anim_subanims.c",
    "slots_gfx.c", "font_data.c",
}

ARRAY_DEF = re.compile(
    r"(?:^|\n)[ \t]*(?:static[ \t]+)?const[ \t]+[^;={}]*?"
    r"\b(\w+)[ \t]*(?:\[[^\]]*\][ \t]*)+=", re.M)

def defined_arrays(path):
    txt = re.sub(r"/\*.*?\*/", "", path.read_text(encoding="utf-8", errors="replace"),
                 flags=re.S)
    txt = re.sub(r"//[^\n]*", "", txt)
    return {m.group(1) for m in ARRAY_DEF.finditer(txt)}

def reason_for(sym, relpath):
    return ACCOUNTED.get(sym) or ACCOUNTED.get(f"{relpath}:*")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--verbose", action="store_true", help="name every symbol")
    args = ap.parse_args()

    packaged = {a["name"] for a in B.ASSETS}
    pkg_of = {a["name"]: a["package"] for a in B.ASSETS}

    committed, authored, outstanding_gen = [], [], []

    for p in sorted((REPO / "src" / "data").glob("*.c")):
        rel = f"src/data/{p.name}"
        syms = defined_arrays(p)
        if p.name in EXPECT_EMPTY and syms:
            print(f"  ! {rel} was expected to define nothing, but defines "
                  f"{len(syms)}: {', '.join(sorted(syms))}")
        for s in sorted(syms):
            why = reason_for(s, rel)
            if why:
                authored.append((rel, s, why))
            elif s not in packaged:
                committed.append((rel, s, p.stat().st_size))

    gen = REPO / "generated"
    by_game = {}
    for p in sorted(gen.glob("*.c")):
        if p.name in ("assetpack_bind.c",):
            continue
        syms = defined_arrays(p)
        unpacked = {s for s in syms if s not in packaged}
        if not unpacked:
            continue
        game = ("crystal" if p.name.startswith(("crystal", "johto", "gen2"))
                else "other")
        by_game.setdefault(game, [0, 0, []])
        by_game[game][0] += len(unpacked)
        by_game[game][1] += p.stat().st_size
        by_game[game][2].append(p.name)
        outstanding_gen += [(p.name, s) for s in sorted(unpacked)]

    pkgs = {}
    for n in packaged:
        pkgs[pkg_of[n]] = pkgs.get(pkg_of[n], 0) + 1
    print("PACKAGED")
    for k in sorted(pkgs):
        print(f"  {k:<10} {pkgs[k]:5d} assets")
    print(f"  {'total':<10} {len(packaged):5d}")

    print(f"\nAUTHORED (the port's own, never to be packaged) -- {len(authored)}")
    if args.verbose:
        for rel, s, why in authored:
            print(f"  {s}\n      {why}")
    else:
        seen = set()
        for rel, s, why in authored:
            if rel not in seen:
                seen.add(rel)
                n = sum(1 for r, _, _ in authored if r == rel)
                print(f"  {rel:<34} {n:3d} symbol(s)")

    print(f"\nCOMMITTED -- ROM data still in src/data -- {len(committed)}")
    files = {}
    for rel, s, size in committed:
        files.setdefault(rel, [0, size])[0] += 1
    for rel in sorted(files, key=lambda r: -files[r][1]):
        n, size = files[rel]
        print(f"  {size // 1024:5d} KB  {rel:<38} {n:4d} symbol(s)")
        if args.verbose:
            for r, s, _ in committed:
                if r == rel:
                    print(f"            {s}")

    print(f"\nCOMPILED IN -- generated/, linked but in no package -- "
          f"{len(outstanding_gen)}")
    for game in sorted(by_game):
        n, size, fs = by_game[game]
        print(f"  {size // 1024:5d} KB  {game:<10} {n:5d} symbol(s) "
              f"in {len(fs)} file(s)")
        if args.verbose:
            for f in sorted(fs):
                print(f"            {f}")

    total = len(committed) + len(outstanding_gen)
    print(f"\n{'=' * 60}")
    if total == 0:
        print("DONE -- every data symbol is packaged or accounted for.")
        return 0
    print(f"OUTSTANDING: {total} symbol(s) "
          f"({len(committed)} committed, {len(outstanding_gen)} compiled in)")
    print("Run with --verbose to name them.")
    if not committed and outstanding_gen:

        print("\nNote: 0 committed -- nothing ROM-derived is in the repo.")
        print("These are linked from generated/ (gitignored) in a FULL build.")
        print("A Red standalone excludes them: cmake -DRED_ONLY=ON, or")
        print("  bash tools/setup_red.sh <your-red-rom.gbc>")
    return 1

if __name__ == "__main__":
    sys.exit(main())
