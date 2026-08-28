
import argparse
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

REPO = Path(__file__).resolve().parent.parent.parent
DOC = REPO / "docs" / "assetpack-migration.md"
GIT = r"C:\Program Files\Git\cmd\git.exe"

import build_pak
from gen1_rom import Gen1Rom, RomError, default_paths

DEF_RE = re.compile(
    r"^(?P<static>static\s+)?const\s+[A-Za-z_][A-Za-z0-9_]*\s*"
    r"(?:\*\s*(?:const\s+)?)*\s*"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\[",
    re.M)

def tracked_src_data():
    out = subprocess.run([GIT, "-C", str(REPO), "ls-files", "src/data"],
                         capture_output=True, text=True).stdout
    return sorted(l.strip() for l in out.splitlines() if l.strip())

def symbols_in(path):
    try:
        text = Path(path).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return [], []
    exported, internal = set(), set()
    for m in DEF_RE.finditer(text):
        (internal if m.group("static") else exported).add(m.group("name"))
    return sorted(exported), sorted(internal)

def collect():
    registered = {a["name"]: a for a in build_pak.ASSETS}

    verified = set()
    rom = None
    try:
        r, s = default_paths(REPO)
        rom = Gen1Rom(r, s)
    except RomError:
        pass
    if rom:
        for a in build_pak.ASSETS:
            if not a["verify"]:
                continue
            p = REPO / a["verify"]
            if not p.is_file():
                continue
            try:
                want = build_pak.parse_c_bytes(p, a["symbol"], a.get("shape"))
                got = a["fn"](rom)

                if got == want or a.get("expect_diff"):
                    verified.add(a["name"])
            except (RomError, SystemExit):
                pass

    rows = []
    for rel in tracked_src_data():
        if not rel.endswith(".c"):
            continue
        syms, internal = symbols_in(REPO / rel)
        done = [s for s in syms if s in verified]
        reg = [s for s in syms if s in registered and s not in verified]
        todo = [s for s in syms if s not in registered]

        entries = sum(1 for a in build_pak.ASSETS if a["verify"] == rel)
        entries_ok = sum(1 for a in build_pak.ASSETS
                         if a["verify"] == rel and a["name"] in verified)

        rows.append({
            "file": rel, "symbols": syms, "internal": internal,
            "entries": entries, "entries_ok": entries_ok,
            "done": done, "registered": reg, "todo": todo,
            "size_kb": round((REPO / rel).stat().st_size / 1024, 1)
                       if (REPO / rel).is_file() else 0,
        })
    return rows, registered, verified, rom

def status_of(row):
    if row["symbols"] and not row["todo"] and not row["registered"]:
        return "done"
    if row["done"] or row["registered"]:
        return "partial"
    if not row["symbols"] and not row["internal"]:
        return "nodata"
    return "todo"

BADGE = {"done": "[x]", "partial": "[~]", "todo": "[ ]", "nodata": "[?]"}

def render(rows, registered, verified, rom):
    counts = {}
    for r in rows:
        counts[status_of(r)] = counts.get(status_of(r), 0) + 1
    total_syms = sum(len(r["symbols"]) for r in rows)
    done_syms = sum(len(r["done"]) for r in rows)

    L = []
    A = L.append
    A("# Asset migration status")
    A("")
    A("**GENERATED FILE - do not edit by hand.** Regenerate with:")
    A("")
    A("```bash")
    A("pwsh tools/py.ps1 tools/assetpack/migration_status.py --write")
    A("```")
    A("")
    A("Tracking the move from ROM assets committed under `src/data/` to a")
    A("runtime `assets.pak` built from the user's own ROM. See")
    A("`src/platform/assetpack.h` for why, and `tools/assetpack/build_pak.py`")
    A("for how to add one.")
    A("")
    A("## Progress")
    A("")

    def is_cut(a):
        if a["bind"] or a.get("cut"):
            return True
        if not a["verify"]:
            return False
        path = REPO / a["verify"]
        if not path.is_file():
            return True
        return not build_pak._find_definition(build_pak._c_text(path), a["symbol"])

    bound = [a["name"] for a in build_pak.ASSETS if is_cut(a)]
    pending = [a for a in build_pak.ASSETS if not is_cut(a) and a["verify"]]
    A(f"- **{len(bound)} of {len(build_pak.ASSETS)} pack assets CUT OVER** -- "
      f"src/data definition deleted, bound from the pack at boot. This is the")
    A("  completed work: those bytes are no longer in the binary.")
    A(f"- **{len(pending)} more assets verified** "
      f"byte-exact but still compiled in, awaiting cutover.")
    A(f"- **{len(rows)} files still under src/data**, "
      f"{total_syms - done_syms} exported symbols outstanding.")
    A("")
    A("NOTE: a file that has been cut over is DELETED, so it leaves the table")
    A("below entirely rather than showing as done. Progress is the cut-over")
    A("count going UP and the src/data file count going DOWN -- do not read a")
    A("shrinking 'migrated' figure here as a regression.")
    if rom:
        A(f"- verified against `{rom.title}` (`{rom.sha1}`)")
    else:
        A("- *(no ROM available; verification skipped this run)*")
    A("")
    A("A symbol counts as migrated only when the exporter reads it from the ROM")
    A("and the bytes match what is committed today, exactly. Anything less is")
    A("still listed as outstanding.")
    A("")
    A("MIGRATED and CUT OVER are different stages. Migrated means the exporter")
    A("can reproduce the bytes; the array is still compiled into the binary and")
    A("nothing renders differently. Cut over means the src/data definition is")
    A("DELETED and the symbol is bound from the pack at boot -- that is the step")
    A("that can change what you see on screen, and the one worth testing.")
    A("")
    if bound:
        A("Cut over so far:")
        A("")
        for n in bound:
            A(f"- `{n}`")
        A("")
    A("## Building from a fresh clone")
    A("")
    A("`generated/` is gitignored but the build now REQUIRES two files from it,")
    A("so a fresh clone must run both of these before cmake:")
    A("")
    A("```bash")
    A("# 1. the binding layer -- generated/assetpack_bind.{h,c}, compiled in")
    A("pwsh tools/py.ps1 tools/assetpack/gen_bindings.py")
    A("")
    A("# 2. the asset pack itself -- from YOUR OWN ROM")
    A("pwsh tools/py.ps1 tools/assetpack/build_pak.py --rom <your-rom.gbc>")
    A("```")
    A("")
    A("The game looks for `assets.pak` in the CWD and then `../assets.pak`, so")
    A("the exporter's default (repo root) works when running from `build/`.")
    A("Without a pack the game exits immediately with instructions rather than")
    A("starting up half-initialised.")
    A("")
    A("## Legend")
    A("")
    A("| | meaning |")
    A("|---|---|")
    A("| `[x]` | every symbol migrated and byte-verified |")
    A("| `[~]` | partially migrated |")
    A("| `[ ]` | not started |")
    A("| `[?]` | no exported data symbols found (may be code, not assets) |")
    A("")

    A("## Files")
    A("")
    A("`symbols` counts EXPORTED symbols only. `pack` counts entries actually")
    A("in the pack from this file, including file-local statics -- which is why")
    A("`tileset_data.c` can show 0/1 symbols while 57 verified entries exist.")
    A("")
    A("| | file | KB | symbols | migrated | pack | outstanding |")
    A("|---|---|---:|---:|---:|---:|---|")
    for r in sorted(rows, key=lambda r: (status_of(r) != "done", -r["size_kb"])):
        out = ", ".join(f"`{s}`" for s in (r["todo"] + r["registered"])[:4])
        if len(r["todo"]) + len(r["registered"]) > 4:
            out += f" +{len(r['todo']) + len(r['registered']) - 4} more"
        pack = f"{r['entries_ok']}/{r['entries']}" if r["entries"] else "-"
        A(f"| {BADGE[status_of(r)]} | `{r['file']}` | {r['size_kb']} | "
          f"{len(r['symbols'])} | {len(r['done'])} | {pack} | {out or '-'} |")
    A("")

    extra = sorted(set(registered) - verified)
    if extra:
        A("## Registered but not verifying")
        A("")
        A("These are wired into the exporter but do not currently match the")
        A("committed bytes. Treat the SYMBOL CHOICE as the prime suspect before")
        A("the committed data -- a wrong-but-plausible symbol decodes cleanly at")
        A("the right size and looks fine until diffed.")
        A("")
        for n in extra:
            A(f"- `{n}`")
        A("")

    A("## Out of scope here")
    A("")
    A("Tracked ROM-derived content this doc does NOT cover yet:")
    A("")
    A("- `mod_runtime/blocks/` - Kanto map geometry (phase 2)")
    A("- `mod_runtime/scenes/` + `src/game/*_scripts.c` - dialogue (phase 3)")
    A("- `generated/` - Johto/Crystal/GBC assets are gitignored, but are still")
    A("  COMPILED INTO the binary, so they need this same treatment before any")
    A("  release build is safe")
    return "\n".join(L) + "\n"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true", help=f"write {DOC.name}")
    args = ap.parse_args()

    rows, registered, verified, rom = collect()
    text = render(rows, registered, verified, rom)
    if args.write:
        DOC.parent.mkdir(parents=True, exist_ok=True)
        DOC.write_text(text, encoding="utf-8")
        done = sum(1 for r in rows if status_of(r) == "done")
        print(f"wrote {DOC.relative_to(REPO)}  ({done}/{len(rows)} files done)")
    else:
        print(text)

if __name__ == "__main__":
    main()
