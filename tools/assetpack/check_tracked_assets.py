
import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
BASELINE = HERE / "tracked_assets_baseline.json"

def find_git():
    exe = shutil.which("git")
    if exe:
        return exe
    for candidate in (
        r"C:\Program Files\Git\cmd\git.exe",
        r"C:\Program Files\Git\bin\git.exe",
        r"C:\msys64\usr\bin\git.exe",
    ):
        if Path(candidate).exists():
            return candidate
    sys.exit("error: git executable not found (checked PATH and common install locations)")

GIT = find_git()

ASSET_EXT_RE = re.compile(
    r"\.(png|bmp|ppm|jpe?g|gif|bin|2bpp|pal|pcm|ogg|wav|mp3|mid)$", re.IGNORECASE
)

MUST_BE_EMPTY = [
    "mods/gfx/crystalsprites/",
    "mod_runtime/generated/",
    "mod_runtime/map_export/",
    "experiment/",
    "maps/",
]

def git_ls_files(repo):
    out = subprocess.run(
        [GIT, "ls-files"], cwd=repo, capture_output=True, text=True, check=True
    ).stdout
    return [ln for ln in out.splitlines() if ln]

def load_baseline():
    if not BASELINE.exists():
        return {"_comment": "", "accepted": []}
    return json.loads(BASELINE.read_text(encoding="utf-8"))

def write_baseline(paths_with_reasons):
    data = {
        "_comment": (
            "Tracked ROM-asset-shaped files accepted by "
            "tools/assetpack/check_tracked_assets.py. Every entry needs a "
            "written REASON -- an unexplained one is how a real regression "
            "gets silenced. Regenerate the path list with --update-baseline, "
            "but write reasons by hand."
        ),
        "accepted": paths_with_reasons,
    }
    BASELINE.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")

def run_baseline_sweep(update):
    tracked = git_ls_files(REPO)
    matched = sorted(p for p in tracked if ASSET_EXT_RE.search(p))

    baseline = load_baseline()
    known = {e["path"]: e for e in baseline.get("accepted", [])}

    new = [p for p in matched if p not in known]
    stale = [p for p in known if p not in matched]

    print(f"tracked ROM-asset-shaped files: {len(matched)}")
    for p in matched:
        tag = "[baselined]" if p in known else "[NEW]"
        print(f"  {tag:12s} {p}")

    if stale:
        print(f"\n{len(stale)} baseline entr{'y is' if len(stale)==1 else 'ies are'} "
              f"no longer tracked (fine -- update the baseline to drop them):")
        for p in stale:
            print(f"  - {p}")

    if update:
        kept = [known[p] for p in matched if p in known]
        for p in new:
            kept.append({"path": p, "reason": "TODO: write a reason"})
        write_baseline(kept)
        print(f"\nwrote {BASELINE} ({len(kept)} entries) -- fill in any TODO reasons")
        return 0

    if new:
        print(f"\nFAIL: {len(new)} newly tracked ROM-asset-shaped file(s) not in the baseline:")
        for p in new:
            print(f"  - {p}")
        print(
            "\nIf this is legitimate (e.g. hand-authored, non-ROM-derived "
            "test content), add it with a written reason:\n"
            "  check_tracked_assets.py --update-baseline"
        )
        return 1

    print("\nOK: no untracked-for ROM-asset-shaped file is tracked.")
    return 0

def run_empty_dir_sweep():
    tracked = git_ls_files(REPO)
    fail = False
    for d in MUST_BE_EMPTY:
        hits = [p for p in tracked if p.startswith(d)]
        if hits:
            fail = True
            print(f"\nFAIL: {d} must be untracked entirely, but {len(hits)} file(s) are still tracked:")
            for p in hits[:10]:
                print(f"  - {p}")
            if len(hits) > 10:
                print(f"  ... and {len(hits) - 10} more")
    if not fail:
        print(f"\nOK: all {len(MUST_BE_EMPTY)} ROM-only/debug-scratch directories are empty of tracked files.")
    return 1 if fail else 0

def run_full_checks():
    rc = 0

    print(f"\n{'='*60}\nrunning status.py\n{'='*60}")
    r = subprocess.run([sys.executable, str(HERE / "status.py")],
                        cwd=REPO, capture_output=True, text=True)
    print(r.stdout)
    if r.stderr:
        print(r.stderr, file=sys.stderr)

    m = re.search(r"COMMITTED -- ROM data still in src/data -- (\d+)", r.stdout)
    committed = int(m.group(1)) if m else None
    if committed is None:
        print("WARNING: could not parse status.py's COMMITTED count -- treating as a failure "
              "to be safe (its output format may have changed).")
        rc = 1
    elif committed != 0:
        print(f"FAIL: status.py reports {committed} ROM data symbol(s) actually COMMITTED "
              "to src/data (not just compiled-in-and-gitignored).")
        rc = 1
    else:
        print("OK: status.py reports 0 committed (outstanding COMPILED-IN Crystal "
              "symbols, if any, are the known deferred migration -- not a gate failure).")

    print(f"\n{'='*60}\nrunning audit_linked.py\n{'='*60}")
    r = subprocess.run([sys.executable, str(HERE / "audit_linked.py")], cwd=REPO)
    if r.returncode != 0:
        rc = 1
    return rc

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--update-baseline", action="store_true")
    ap.add_argument("--full", action="store_true",
                     help="also run status.py and audit_linked.py (needs a built build-red/)")
    a = ap.parse_args()

    rc = run_baseline_sweep(a.update_baseline)
    rc |= run_empty_dir_sweep()
    if a.full and not a.update_baseline:
        rc |= run_full_checks()
    return rc

if __name__ == "__main__":
    sys.exit(main())
