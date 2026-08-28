
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

EMITTED = re.compile(r"custom_art/kanto/[a-z0-9]+_t\d{3}\.(png|bin)$")
SUBTILE = re.compile(r"^subtile\s+\S+\s+(\S+)", re.M)

def main():
    quiet = "--quiet" in sys.argv
    refs = {}
    for path in sorted(glob.glob(os.path.join(ROOT, "mod_runtime", "blocks", "*.block"))):
        text = open(path, encoding="utf-8", errors="replace").read()
        for m in SUBTILE.finditer(text):
            refs.setdefault(m.group(1).replace("\\", "/"), []).append(
                os.path.basename(path))

    kanto = {r: f for r, f in refs.items() if "custom_art/kanto/" in r}
    orphan = {r: f for r, f in kanto.items() if not EMITTED.search(r)}

    if not quiet:
        print("block subtile references into custom_art/kanto: %d" % len(kanto))
        print("not regenerable by emit_kanto.py:               %d" % len(orphan))

    if orphan:
        print()
        print("These reference art NO extractor produces. A tester building")
        print("from their own ROM will not have them:")
        for r in sorted(orphan):
            print("  %-64s  %s" % (r, ", ".join(sorted(set(orphan[r])))))
        print()
        print("Fix by pointing the block at the map's own <maplower>_tNNN tile")
        print("if the art is identical, or by adding a real extractor for it.")
        print("Do NOT just create the file -- it will vanish for everyone else.")
        return 1

    if not quiet:
        print("OK -- every referenced tile is regenerable from the ROM.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
