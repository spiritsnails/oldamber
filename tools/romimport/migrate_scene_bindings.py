
import argparse
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
BLOCKS = os.path.join(REPO, "mod_runtime", "blocks")
OUT = os.path.join(REPO, "mod_runtime", "scenes", "bindings.txt")

BIND = re.compile(r"^(scene_npc|scene_trigger|scene_tile)\s+(\S+)\s+(.*)$")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()

    rows = []
    for path in sorted(glob.glob(os.path.join(BLOCKS, "*.block"))):
        with open(path, encoding="utf-8", errors="replace") as fh:
            for ln in fh:
                m = BIND.match(ln.strip())
                if m:
                    rows.append((m.group(2), m.group(1), m.group(3)))
    rows.sort()

    print("%d bindings from %d authored .block files"
          % (len(rows), len(set(r[0] for r in rows))))
    if not a.apply:
        for r in rows[:5]:
            print("   %s %s %s" % (r[1], r[0], r[2]))
        print("(dry run -- pass --apply to write %s)" % OUT)
        return 0

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("# Scene bindings: which .scene attaches to which map cell.\n"
                 "# THIS PROJECT'S OWN DATA -- not in the ROM, not derivable\n"
                 "# from it. Committed. tools/romimport/emit_kanto.py injects\n"
                 "# these into every generated map, so a clean checkout needs\n"
                 "# only scenes/ and this file for scenes to work.\n"
                 "#   <directive> <Map> <scene path> <x> <y>\n")
        for mapname, directive, rest in rows:
            fh.write("%s %s %s\n" % (directive, mapname, rest))
    print("wrote %s" % OUT)
    return 0

if __name__ == "__main__":
    sys.exit(main())
