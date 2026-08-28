
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SCENES = os.path.join(REPO, "mod_runtime", "scenes")
TBL = os.path.join(REPO, "mod_runtime", "generatedmaps", "kanto",
                   "scene_text.tbl")

SAY = re.compile(r'^(\s*say(?:_auto)?\s+)"((?:[^"\\]|\\.)*)"(\s*)$', re.M)

def norm(s):
    s = s.replace("\\n", "\n").replace("\\f", "\n").replace("\f", "\n")
    return re.sub(r"\s+", " ", s.rstrip("@")).strip()

def load_table():
    if not os.path.isfile(TBL):
        sys.exit("no %s -- run tools/romimport/emit_scene_text.py first" % TBL)
    exact, byn, every = {}, {}, []
    with open(TBL, encoding="utf-8") as fh:
        for ln in fh:
            if ln.startswith("#") or "\t" not in ln:
                continue
            sym, txt = ln.rstrip("\n").split("\t", 1)
            exact.setdefault(txt, sym)
            byn.setdefault(norm(txt), (sym, txt))

            every.append((sym, txt))
    return exact, byn, every

def item_display_names():
    sys.path.insert(0, os.path.join(REPO, "tools", "assetpack"))
    import emit_kanto as E
    from gen1_rom import Gen1Rom, default_paths
    rom = Gen1Rom(*default_paths(REPO))
    out = {}
    for iid, const in E.item_constant_names(rom).items():
        if iid >= E.TM01_ITEM_ID:
            disp = "TM%02d" % (iid - E.TM01_ITEM_ID + 1)
        elif iid >= E.HM01_ITEM_ID:
            disp = "HM%02d" % (iid - E.HM01_ITEM_ID + 1)
        else:
            disp = None
        out[const] = (iid, disp)
    names = {}
    for const, (iid, disp) in out.items():
        if disp is None:

            disp = const.replace("_", " ")
        names[const] = disp
    return names

GIVE = re.compile(r"^\s*give_item\s+(\S+)")

GIVE_MON = re.compile(r"^\s*give_pokemon\s+(\S+)")
PLACEHOLDER = re.compile(r"\{(name|mon|city|leader)\}")

def template_pass(src, templates, disp, stats, rel, samples,
                  allow_shared=False):
    out, last_item, last_mon = [], None, None
    for ln in src.split("\n"):
        g = GIVE.match(ln)
        if g:
            last_item = disp.get(g.group(1))
        gm = GIVE_MON.match(ln)
        if gm:

            last_item = last_mon = gm.group(1).upper()
        m = SAY.match(ln)
        if not m or not last_item:
            out.append(ln)
            continue
        head, lit, tail = m.group(1), m.group(2), m.group(3)

        mapdir = rel.replace("\\", "/").split("/")[-2].lower() if "/" in \
            rel.replace("\\", "/") else ""
        item_key = last_item.replace(" ", "").replace(".", "").lower()
        best, best_score = None, -1
        for sym, tmpl in templates:
            pat = "^" + "(.+)".join(re.escape(p) for p in
                                    PLACEHOLDER.split(norm(tmpl))[::2]) + "$"
            mm = re.match(pat, norm(lit))
            if not mm:
                continue

            slots = PLACEHOLDER.findall(norm(tmpl))
            ok = True
            for slot, got in zip(slots, mm.groups()):
                got = got.strip()
                if slot == "mon":
                    ok = ok and got == (last_mon or last_item)
                elif slot == "name":

                    ok = ok and (got == last_item or got == "{box_num}")
                else:
                    ok = False
            if not ok:
                continue
            low = sym.lower()
            score = (2 if mapdir and mapdir in low else 0) + \
                    (1 if item_key and item_key in low else 0)
            if score > best_score:
                best, best_score = sym, score

        peers = set()
        if best:
            btxt = dict(templates).get(best)
            for sym2, t2 in templates:
                if t2 == btxt:
                    peers.add(sym2.lstrip("_").split(".")[0].lower())

        hit = best if (best_score > 0 or len(peers) <= 1 or allow_shared)             else None
        if hit:
            stats["template"] += 1
            if len(samples) < 5:
                samples.append((rel, lit, hit, last_item))
            out.append("%srom:%s%s" % (head, hit, tail))
        else:
            out.append(ln)
    return "\n".join(out)

REF = re.compile(r'^(\s*say(?:_auto)?\s+rom:)(\S+)(\s*)$', re.M)

def resymbol(every, apply):
    bysym, bytext = {}, {}
    for sym, txt in every:
        bysym[sym] = txt
        bytext.setdefault(txt, []).append(sym)
    n = 0
    for root, _d, files in os.walk(SCENES):
        for fn in sorted(files):
            if not fn.endswith(".scene"):
                continue
            path = os.path.join(root, fn)
            mapdir = os.path.basename(root).lower()
            src = open(path, encoding="utf-8", errors="replace").read()

            def fix(m):
                nonlocal n
                sym = m.group(2)
                txt = bysym.get(sym)
                if txt is None:
                    return m.group(0)
                peers = [p for p in bytext.get(txt, []) if mapdir and
                         mapdir in p.lower()]
                if peers and sym not in peers:
                    n += 1
                    print("   %-42s %s -> %s"
                          % (os.path.relpath(path, SCENES)[:42], sym, peers[0]))
                    return "%s%s%s" % (m.group(1), peers[0], m.group(3))
                return m.group(0)

            out = REF.sub(fix, src)
            if out != src and apply:
                open(path, "w", encoding="utf-8",
                     newline=chr(10)).write(out)
    print("%s %d reference(s)" % ("repointed" if apply else "would repoint", n))
    return 0

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--only", help="substring of the scene path")
    ap.add_argument("--resymbol", action="store_true",
                    help="repoint an existing `say rom:` at a better-matching "
                         "symbol when several share the same text")
    ap.add_argument("--shared", action="store_true",
                    help="allow a template match to a symbol that does not name "
                         "this scene's map, when the ROM has no better one")
    ap.add_argument("--templates", action="store_true",
                    help="also rewrite literals the ROM stores as a TEMPLATE "
                         "with a spliced item name")
    a = ap.parse_args()

    exact, byn, every = load_table()
    if a.resymbol:
        return resymbol(every, a.apply)
    n_exact = n_reflow = n_miss = n_files = 0
    reflow_samples = []
    tstats, tsamples, templates, disp = {"template": 0}, [], [], {}
    if a.templates:
        disp = item_display_names()
        templates = [(s, t) for s, t in every if PLACEHOLDER.search(t)]
        print("ROM templates with a spliced value: %d\n" % len(templates))

    for root, _d, files in os.walk(SCENES):
        for fn in sorted(files):
            if not fn.endswith(".scene"):
                continue
            path = os.path.join(root, fn)
            rel = os.path.relpath(path, SCENES)
            if a.only and a.only not in rel.replace("\\", "/"):
                continue
            src = open(path, encoding="utf-8", errors="replace").read()
            changed = [0]

            def sub(m):
                head, lit, tail = m.group(1), m.group(2), m.group(3)
                nonlocal n_exact, n_reflow, n_miss
                if lit in exact:
                    sym = exact[lit]
                    n_exact += 1
                elif norm(lit) in byn:
                    sym, romtxt = byn[norm(lit)]
                    n_reflow += 1
                    if len(reflow_samples) < 6:
                        reflow_samples.append((rel, lit, romtxt))
                else:
                    n_miss += 1
                    return m.group(0)
                changed[0] += 1
                return "%srom:%s%s" % (head, sym, tail)

            out = SAY.sub(sub, src)
            if a.templates:
                before = out
                out = template_pass(out, templates, disp, tstats, rel, tsamples,
                                    a.shared)
                if out != before:
                    changed[0] += 1
            if changed[0]:
                n_files += 1
                if a.apply:
                    open(path, "w", encoding="utf-8", newline="\n").write(out)

    print("%s: %d say-literals -> rom: references across %d files"
          % ("REWROTE" if a.apply else "would rewrite",
             n_exact + n_reflow, n_files))
    print("   %4d exact      (byte-identical, no visible change)" % n_exact)
    print("   %4d REFLOWED   (same words, ROM's line breaks win)" % n_reflow)
    if a.templates:
        print("   %4d TEMPLATE   (ROM splices the item name; value verified "
              "against the scene's own give_item)" % tstats["template"])
        n_miss -= tstats["template"]
    print("   %4d left alone (no ROM match)" % n_miss)
    for rel, lit, sym, item in tsamples:
        print("      %s\n         was: %s\n         rom: %s  ({name}=%s)"
              % (rel, lit[:56], sym, item))
    if reflow_samples:
        print("\nreflow examples:")
        for rel, lit, romtxt in reflow_samples:
            print("   %s" % rel)
            print("      was: %s" % lit[:66])
            print("      rom: %s" % romtxt[:66])
    if not a.apply:
        print("\n(dry run -- pass --apply to write)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
