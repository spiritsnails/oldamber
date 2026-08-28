
import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from gen1_rom import Gen1Rom, RomError

REPO = Path(__file__).resolve().parent.parent.parent
BASELINE = Path(__file__).resolve().parent / "audit_baseline.json"

ROMS = {
    "red":     ("pokered-master/pokered.gbc",         "pokered-master/pokered.sym"),
    "crystal": ("pokecrystal-master/pokecrystal.gbc", "pokecrystal-master/pokecrystal.sym"),
}

DECL = re.compile(
    r"(?:static\s+)?const\s+(?:unsigned\s+char|uint8_t)\s+"
    r"(\w+)\s*(\[[^=;]*\])\s*=\s*\{", re.M)

MIN_LEN = 16
MIN_EVIDENCE_LEN = 24
MIN_DISTINCT = 4

VERDICTS = """
  copied        unique in a ROM, long enough and varied enough to be evidence
  likely        unique in a ROM, but short, plausible, not proof
  inconclusive  found, but occurs more than once: proves nothing either way
  unmatched     no verbatim match in any ROM available (see the blind spots)
"""

def load_roms(which):
    out, missing = {}, []
    for name, (rom_path, sym_path) in ROMS.items():
        if which and name not in which:
            continue
        p = REPO / rom_path
        if not p.is_file():
            missing.append(name)
            continue
        try:

            out[name] = Gen1Rom(p, REPO / sym_path, allow_unknown=True).data
        except RomError as e:
            missing.append(f"{name} ({e})")
    return out, missing

def link_surface(build_dir):
    root = REPO / build_dir / "CMakeFiles"
    if not root.is_dir():
        sys.exit(f"no build at {build_dir}/ -- build it first:\n"
                 f"  BUILD_DIR={build_dir} bash tools/build.sh pokered")
    srcs = set()
    for obj in root.rglob("*.obj"):

        parts = obj.as_posix().split(".dir/")
        if len(parts) < 2:
            continue
        p = REPO / parts[-1][:-4]
        if p.suffix == ".c" and p.exists():
            srcs.add(p)
    return sorted(srcs)

def _source(path):
    txt = path.read_text(encoding="utf-8", errors="replace")
    return re.sub(r"/\*.*?\*/|//[^\n]*",
                  lambda m: re.sub(r"[^\n]", " ", m.group(0)),
                  txt, flags=re.S)

def arrays_in(path):
    txt = _source(path)
    for m in DECL.finditer(txt):
        body = txt[m.end():]
        depth, i = 1, 0
        while i < len(body) and depth:
            if body[i] == "{":
                depth += 1
            elif body[i] == "}":
                depth -= 1
            i += 1
        blob = bytes(int(x, 16)
                     for x in re.findall(r"0x([0-9A-Fa-f]{2})", body[:i]))
        if len(blob) >= MIN_LEN:
            yield m.group(1), blob

def scan_targets(build_dir):
    seen, out = set(), []
    for c in link_surface(build_dir):
        files = [c]
        txt = c.read_text(encoding="utf-8", errors="replace")
        for inc in re.findall(r'#include\s+"([^"]+\.h)"', txt):
            h = (c.parent / inc).resolve()
            if h.exists() and str(h).startswith(str(REPO / "src")):
                files.append(h)
        for f in files:
            for sym, blob in arrays_in(f):
                key = (sym, f)
                if key in seen:
                    continue
                seen.add(key)
                out.append((sym, f.relative_to(REPO).as_posix(), blob))
    return out

def sizeof_misuse(build_dir):
    import build_pak
    bound = {a["name"] for a in build_pak.ASSETS if a["bind"]}
    pat = re.compile(r"sizeof\s*\(\s*(\w+)")
    out = []
    for c in link_surface(build_dir):
        txt = _source(c)
        for m in pat.finditer(txt):
            if m.group(1) in bound:
                line = txt.count("\n", 0, m.start()) + 1
                out.append((c.relative_to(REPO).as_posix(), line, m.group(1)))
    return out

def classify(blob, roms):
    best = None
    for name, data in roms.items():
        n, start = 0, 0
        while n < 100:
            k = data.find(blob, start)
            if k < 0:
                break
            n += 1
            start = k + 1
        if n and (best is None or n < best[1]):
            best = (name, n)
    if best is None:
        return "unmatched", None, 0
    rom, hits = best
    distinct = len(set(blob))
    if hits > 1:
        return "inconclusive", rom, hits
    if len(blob) >= MIN_EVIDENCE_LEN and distinct >= MIN_DISTINCT:
        return "copied", rom, hits
    return "likely", rom, hits

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build", default="build-red",
                    help="build directory to audit (default: build-red, the "
                         "RED_ONLY standalone -- the one a player runs)")
    ap.add_argument("--rom", action="append", dest="roms",
                    help="restrict to these ROMs (default: all available)")
    ap.add_argument("--update-baseline", action="store_true",
                    help="record the current findings as accepted")
    ap.add_argument("--all", action="store_true",
                    help="list every array, including baselined ones")
    args = ap.parse_args()

    roms, missing = load_roms(args.roms)
    if not roms:
        sys.exit("no ROMs available to search -- nothing can be verified.\n"
                 "  cd pokered-master && make")
    print(f"ROMs searched: {', '.join(sorted(roms))}")
    if missing:
        print(f"NOT searched (findings against these are UNCHECKED, "
              f"not clean): {', '.join(missing)}")

    targets = scan_targets(args.build)
    print(f"link surface: {len(link_surface(args.build))} .c files in "
          f"{args.build}/, {len(targets)} literal byte arrays >= {MIN_LEN} B\n")

    base = {}
    if BASELINE.is_file():
        base = {f"{e['symbol']}@{e['file']}": e
                for e in json.loads(BASELINE.read_text())["accepted"]}

    findings, new = [], []
    for sym, path, blob in targets:
        verdict, rom, hits = classify(blob, roms)
        f = {"symbol": sym, "file": path, "bytes": len(blob),
             "verdict": verdict, "rom": rom, "hits": hits}
        findings.append(f)
        key = f"{sym}@{path}"
        if verdict in ("copied", "likely") and key not in base:
            new.append(f)

    order = {"copied": 0, "likely": 1, "inconclusive": 2, "unmatched": 3}
    findings.sort(key=lambda f: (order[f["verdict"]], -f["bytes"]))

    shown = findings if args.all else [f for f in findings
                                       if f["verdict"] in ("copied", "likely")]
    if shown:
        print(f"{'bytes':>6} {'verdict':<13} {'rom':<8} symbol / file")
        for f in shown:
            print(f"{f['bytes']:>6} {f['verdict']:<13} {(f['rom'] or '-'):<8} "
                  f"{f['symbol']}  {f['file']}"
                  + ("   [baselined]"
                     if f"{f['symbol']}@{f['file']}" in base else ""))
        print(VERDICTS)

    tot = {}
    for f in findings:
        t = tot.setdefault(f["verdict"], [0, 0])
        t[0] += 1
        t[1] += f["bytes"]
    for v in ("copied", "likely", "inconclusive", "unmatched"):
        n, b = tot.get(v, (0, 0))
        print(f"  {v:<13} {n:>3} arrays {b:>6} B")

    if args.update_baseline:
        BASELINE.write_text(json.dumps({
            "_comment": "Accepted compiled-in literals. Each needs a reason. "
                        "Regenerate with --update-baseline, but write the "
                        "reasons by hand -- an unexplained entry is how a real "
                        "finding gets silenced.",
            "accepted": [{**f, "reason": base.get(
                f"{f['symbol']}@{f['file']}", {}).get("reason", "TODO")}
                for f in findings if f["verdict"] in ("copied", "likely")],
        }, indent=2) + "\n")
        print(f"\nwrote {BASELINE.relative_to(REPO)} "
              f"-- fill in every \"reason\": \"TODO\"")
        return 0

    bad_sizeof = sizeof_misuse(args.build)
    if bad_sizeof:
        print(f"\nFAIL: sizeof() on {len(bad_sizeof)} BOUND asset(s) -- these "
              f"are pointers, so sizeof is the pointer size, not the data:")
        for path, line, sym in bad_sizeof:
            print(f"  {path}:{line}  sizeof({sym})   use {sym}_count")

    if new:
        print(f"\nFAIL: {len(new)} literal(s) not in the baseline:")
        for f in new:
            print(f"  {f['symbol']}  {f['file']}  ({f['bytes']} B, "
                  f"{f['verdict']} in {f['rom']})")
        print("\nMigrate them (a provider in build_pak.py), or accept them with "
              "a reason:\n  audit_linked.py --update-baseline")

    if new or bad_sizeof:
        return 1

    print("\nOK: no un-baselined ROM data linked into the binary, and no "
          "sizeof() on a bound asset.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
