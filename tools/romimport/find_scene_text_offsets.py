
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "tools", "assetpack"))

import gen1_script as G
from gen1_rom import Gen1Rom, RomError, default_paths
import emit_scene_text as EST

SCENES = os.path.join(REPO, "mod_runtime", "scenes")

def norm(s):
    s = s.replace("\\n", "\n").replace("\\f", "\n").replace("\f", "\n")
    return re.sub(r"\s+", " ", s.rstrip("@")).strip()

TEMPLATE_TOKEN_RE = re.compile(
    r"\b(ABRA|CLEFAIRY|DRATINI|SCYTHER|S\.S\.TICKET|TM\d+|EXP\.ALL|ITEMFINDER|"
    r"# FLUTE|[A-Z]+BALL)\b")

def check_templated(rows):
    by_norm = {}
    for name, txt in rows:
        by_norm.setdefault(norm(txt), name)

    scenes = os.path.join(REPO, "mod_runtime", "scenes")
    for root, _d, files in os.walk(scenes):
        for f in files:
            if not f.endswith(".scene"):
                continue
            p = os.path.join(root, f)
            with open(p, encoding="utf-8", errors="replace") as fh:
                for m in EST.SAY.finditer(fh.read()):
                    lit = m.group(1)
                    if norm(lit) in by_norm:
                        continue
                    for tok_re, placeholder in (
                        (TEMPLATE_TOKEN_RE, "{name}"),
                    ):
                        templated = tok_re.sub(placeholder, lit, count=1)
                        if templated == lit:
                            continue
                        key = norm(templated)
                        if key in by_norm:
                            noun = tok_re.search(lit).group(0)
                            print(f"  TEMPLATED: {os.path.relpath(p, scenes):45s} "
                                  f"rom:{by_norm[key]} name={noun!r}")
                            break

def find_literals():
    lits = []
    for root, _d, files in os.walk(SCENES):
        for f in files:
            if not f.endswith(".scene"):
                continue
            p = os.path.join(root, f)
            with open(p, encoding="utf-8", errors="replace") as fh:
                for m in EST.SAY.finditer(fh.read()):
                    lits.append((os.path.relpath(p, SCENES), m.group(1)))
    return lits

def main():
    dr, ds = default_paths(REPO)
    try:
        rom = Gen1Rom(dr, ds)
    except RomError as e:
        sys.exit("error: %s" % e)

    rows = EST.build(rom)
    by_norm = {norm(txt): name for name, txt in rows}

    lits = find_literals()
    unresolved = [(p, s) for p, s in lits if norm(s) not in by_norm]

    print("whole-symbol matches (already fine via emit_scene_text.py): "
          f"{len(lits) - len(unresolved)}")
    print(f"checking {len(unresolved)} remaining literal(s) for a "
          "page/offset match...\n")

    page_hits, offset_hits, misses = [], [], []
    for path, lit in unresolved:
        target = norm(lit)
        target_esc = lit
        found = None
        for name, full in rows:
            full_esc = EST.escape(full)
            pages = full_esc.split("\\f")
            page_start = None
            acc = ""
            matched_range = None

            for s in range(len(pages)):
                acc = pages[s]
                if acc == target_esc:
                    matched_range = (s, s)
                    break
                for e in range(s + 1, len(pages)):
                    acc = acc + "\\f" + pages[e]
                    if acc == target_esc:
                        matched_range = (s, e)
                        break
                    if len(acc) > len(target_esc):
                        break
                if matched_range:
                    break
            if matched_range:
                found = ("page", name, matched_range)
                break
            idx = full_esc.find(target_esc)
            if idx != -1:
                found = ("offset", name, (idx, len(target_esc)))
                break
        if found is None:

            for name, full in rows:
                if target and target in norm(full):
                    found = ("fuzzy", name, None)
                    break
        if found is None:
            misses.append((path, lit))
        elif found[0] == "page":
            page_hits.append((path, lit, found[1], found[2]))
        elif found[0] == "offset":
            offset_hits.append((path, lit, found[1], found[2]))
        else:
            misses.append((path, lit + "  [FUZZY MATCH ONLY in " + found[1] + "]"))

    print(f"PAGE match ({len(page_hits)}) -- exact escaped-text match, safe for `say rom:<sym> page=N[-M]`:")
    for path, lit, sym, (s, e) in page_hits:
        page_arg = str(s) if s == e else f"{s}-{e}"
        print(f"  {path:45s} say rom:{sym} page={page_arg}")
        print(f"      {lit[:70]}")

    print(f"\nOFFSET match ({len(offset_hits)}) -- exact escaped-text match, safe for `say rom:<sym> at=N len=M`:")
    for path, lit, sym, (off, ln) in offset_hits:
        print(f"  {path:45s} say rom:{sym} at={off} len={ln}")
        print(f"      {lit[:70]}")

    print(f"\nUNRESOLVED, no exact escaped-text match anywhere ({len(misses)}):")
    for path, lit in misses:
        print(f"  {path:45s} {lit[:70]}")

    print("\nOf those, checking for a TEMPLATED match (would resolve via the "
          "EXISTING `say rom:<sym> name=X` splice, no new feature needed):")
    check_templated(rows)

    return 0

if __name__ == "__main__":
    sys.exit(main())
