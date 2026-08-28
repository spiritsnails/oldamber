
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
GAME = os.path.join(REPO, "src", "game")
TBL = os.path.join(REPO, "mod_runtime", "generatedmaps", "kanto",
                   "scene_text.tbl")

DECL = re.compile(
    r'static const char (\w+)\[\]\s*=\s*'
    r'((?:"(?:[^"\\]|\\.)*"\s*)+);',
    re.S)
SEG = re.compile(r'"((?:[^"\\]|\\.)*)"')

PREFIX = re.compile(r'^(\{RIVAL\}|[A-Z][A-Z0-9\'. ]{1,18}): ')

INLINE_CALL = re.compile(
    r'\b(Text_ShowASCII|YesNo_Show)\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)')
INLINE_RETURN = re.compile(r'\breturn\s+((?:"(?:[^"\\]|\\.)*"\s*)+);')

TABLE_LITERAL = re.compile(r'((?:"(?:[^"\\]|\\.)*"\s*)+)(?=[,}])')

def concat_literal(segs_blob):
    return "".join(m.group(1) for m in SEG.finditer(segs_blob))

def norm(s):
    s = s.replace("\\n", "\n").replace("\\f", "\n").replace("\f", "\n")

    s = s.upper()

    s = s.replace("#", "POKE")
    return re.sub(r"\s+", " ", s.rstrip("@")).strip()

def load_table():
    if not os.path.isfile(TBL):
        sys.exit("no %s -- run tools/romimport/emit_scene_text.py first" % TBL)
    exact, byn = {}, {}
    with open(TBL, encoding="utf-8") as fh:
        for ln in fh:
            if ln.startswith("#") or "\t" not in ln:
                continue
            sym, txt = ln.rstrip("\n").split("\t", 1)
            exact.setdefault(txt, sym)
            byn.setdefault(norm(txt), (sym, txt))
    return exact, byn

def match(lit, exact, byn):
    if lit in exact:
        return ("exact", exact[lit], lit)
    if norm(lit) in byn:
        sym, romtxt = byn[norm(lit)]
        return ("reflow", sym, romtxt)
    return None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--only", help="substring of the filename")
    ap.add_argument("--tables", action="store_true",
                    help="also report (never auto-apply) struct-field / "
                         "array-of-pointer literal table entries -- "
                         "kTrainers[]/kQuizQuestions[]-shaped. Converting "
                         "these needs a structural rewrite of the containing "
                         "declaration, done by hand using this report.")
    a = ap.parse_args()

    exact, byn = load_table()
    n_exact = n_reflow = n_prefix = n_miss = n_files = 0
    reflow_samples, prefix_samples, miss_samples = [], [], []
    table_hits = []

    files = sorted(f for f in os.listdir(GAME) if f.endswith("_scripts.c"))
    if a.only:
        files = [f for f in files if a.only in f]

    def resolve(lit, fn, name):
        nonlocal n_exact, n_reflow, n_prefix, n_miss
        hit = match(lit, exact, byn)
        if hit:
            kind, sym, romtxt = hit
            if kind == "exact":
                n_exact += 1
            else:
                n_reflow += 1
                if len(reflow_samples) < 6:
                    reflow_samples.append((fn, name, lit, romtxt))
            return 'RomText("%s")' % sym

        pm = PREFIX.match(lit)
        if pm:
            body_hit = match(lit[pm.end():], exact, byn)
            if body_hit:
                _kind, sym, romtxt = body_hit
                prefix = pm.group(1) + ": "
                n_prefix += 1
                if len(prefix_samples) < 6:
                    prefix_samples.append((fn, name, prefix, romtxt))
                return ('RomTextPrefixed("%s", "%s")'
                        % (prefix.replace('"', '\\"'), sym))

        n_miss += 1
        if len(miss_samples) < 999:
            miss_samples.append((fn, name, lit))
        return None

    for fn in files:
        path = os.path.join(GAME, fn)
        src = open(path, encoding="utf-8", errors="replace").read()
        changed = [0]

        def sub_decl(m):
            name, segs_blob = m.group(1), m.group(2)
            lit = concat_literal(segs_blob)
            expr = resolve(lit, fn, name)
            if expr is None:
                return m.group(0)
            changed[0] += 1
            return '#define %s (%s)' % (name, expr)

        out = DECL.sub(sub_decl, src)

        def sub_call(m):
            func, segs_blob = m.group(1), m.group(2)
            lit = concat_literal(segs_blob)
            expr = resolve(lit, fn, "<inline %s>" % func)
            if expr is None:
                return m.group(0)
            changed[0] += 1
            return '%s(%s)' % (func, expr)

        out = INLINE_CALL.sub(sub_call, out)

        def sub_return(m):
            segs_blob = m.group(1)
            lit = concat_literal(segs_blob)
            expr = resolve(lit, fn, "<inline return>")
            if expr is None:
                return m.group(0)
            changed[0] += 1
            return 'return %s;' % expr

        out = INLINE_RETURN.sub(sub_return, out)

        if a.tables:
            for tm in TABLE_LITERAL.finditer(out):
                lit = concat_literal(tm.group(1))
                if "\\n" not in lit and "\\f" not in lit:
                    continue
                hit = match(lit, exact, byn)
                if not hit:
                    continue
                _kind, sym, _romtxt = hit
                line = out.count("\n", 0, tm.start()) + 1
                table_hits.append((fn, line, lit, sym))

        if changed[0] and 'rom_text.h"' not in out:

            out = re.sub(r'(#include\s+"[^"]+\.h"\s*\n)',
                         r'\1#include "rom_text.h"  '
                         r'/* RomText/RomTextPrefixed -- ROM dialogue lookup */\n',
                         out, count=1)

        if changed[0]:
            n_files += 1
            if a.apply:
                open(path, "w", encoding="utf-8", newline="\n").write(out)

    print("%s: %d declarations -> RomText(...) across %d files"
          % ("REWROTE" if a.apply else "would rewrite",
             n_exact + n_reflow + n_prefix, n_files))
    print("   %4d exact      (byte-identical, no visible change)" % n_exact)
    print("   %4d REFLOWED   (same words, ROM's line breaks win)" % n_reflow)
    print("   %4d PREFIXED   (speaker prefix kept as a literal, body from ROM)" % n_prefix)
    print("   %4d left alone (no ROM match -- needs a look)" % n_miss)
    if reflow_samples:
        print("\nreflow examples:")
        for fn, name, lit, romtxt in reflow_samples:
            print("   %s :: %s" % (fn, name))
            print("      was: %s" % lit[:66])
            print("      rom: %s" % romtxt[:66])
    if prefix_samples:
        print("\nprefixed examples:")
        for fn, name, prefix, romtxt in prefix_samples:
            print("   %s :: %s  (prefix %r)" % (fn, name, prefix))
            print("      rom: %s" % romtxt[:66])
    if miss_samples:
        print("\nunmatched (left as literals):")
        for fn, name, lit in miss_samples:
            print("   %s :: %s" % (fn, name))
            print("      %s" % lit[:76])
    if a.tables:
        print("\ntable entries (%d found -- NOT applied, convert by hand):"
              % len(table_hits))
        for fn, line, lit, sym in table_hits:
            print("   %s:%d" % (fn, line))
            print("      was: %s" % lit[:66])
            print("      sym: %s" % sym)
    if not a.apply:
        print("\n(dry run -- pass --apply to write)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
