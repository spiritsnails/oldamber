#!/usr/bin/env bash
# make_test_install.sh: build a fresh-install sandbox for testing the first run.
#
#     bash tools/dist/make_test_install.sh
#     (then run build/OldAmberTest/OldAmber.exe)
#
# Produces build/OldAmberTest/: the game as a player receives it, with no game
# data at all, no packages/, no generated/, no assets.pak and no generated maps.
# The launcher comes up on its ROM-picker screen and the import runs for real,
# in-process, as it would on the player's machine.
#
# Separate from make_release.sh, which builds the shipping bundle whose first run
# is a frozen PyInstaller setup.exe. This exercises the other first-run path, the
# launcher built into the binary (HAVE_ROM_LAUNCHER, src/platform/launcher.c),
# which runs the same importers through an embedded interpreter.
#
# A separate directory rather than --workdir, because the importers resolve
# their output root from their own __file__ rather than the working directory.
# Pointing the game at an empty --workdir would show the empty first-run screen
# and then write the import back into the real checkout. A directory containing
# its own copy of tools/ is the only arrangement where their REPO lands inside
# the sandbox, which is also the arrangement a player has.
#
# Nothing ROM-derived is copied, and the check at the bottom enforces it. The
# .gbc files in particular are not copied, only the .sym symbol tables, which
# carry no ROM bytes.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$REPO/build/OldAmberTest"
GAME_DIR="${GAME_DIR:-$REPO/build-red}"
# Defined up here because the extractor-source scan below needs it, and it is
# also what supplies python311.dll near the end.
PY="${PY:-C:/Program Files/Python311/python.exe}"

# The RED_ONLY tree builds oldamber.exe; older trees produced pokered.exe.
GAME_EXE=""
for cand in "$GAME_DIR/oldamber.exe" "$GAME_DIR/pokered.exe"; do
    [ -f "$cand" ] && { GAME_EXE="$cand"; break; }
done
[ -n "$GAME_EXE" ] || {
    echo "no game binary in $GAME_DIR -- run: BUILD_DIR=${GAME_DIR##*/} bash tools/build.sh"
    exit 1
}

echo "==> assembling $OUT"
mkdir -p "$OUT"
find "$OUT" -mindepth 1 -maxdepth 1 -exec rm -rf {} + 2>/dev/null || true
mkdir -p "$OUT/mod_runtime" "$OUT/tools" "$OUT/pokered-master" \
         "$OUT/src/data" "$OUT/src/game"

cp "$GAME_EXE" "$OUT/OldAmber.exe"
cp "$REPO/THIRD_PARTY.md" "$OUT/" 2>/dev/null || true

# The DLLs the binary actually loads, resolved rather than listed.
#
# The dev build finds SDL2.dll and the MinGW runtime through the MSYS PATH, so
# build-red/ may contain no DLLs at all and a hardcoded `cp SDL2.dll` fails. A
# player has no MSYS PATH, so the sandbox has to carry them, and which ones is a
# property of how the binary was linked.
echo "==> runtime DLLs"
ldd "$GAME_EXE" 2>/dev/null \
  | awk '{print $3}' \
  | grep -iE '^/(mingw64|c/msys64/mingw64)/' \
  | sort -u \
  | while read -r dll; do cp -n "$dll" "$OUT/" 2>/dev/null || true; done
ls "$OUT"/*.dll >/dev/null 2>&1 || {
    echo "REFUSING: no DLLs resolved -- the sandbox would not start on a clean machine"
    exit 1
}
cp -r "$REPO/shaders" "$OUT/"

# Runtime content that is authored, not extracted. Same list make_release.sh
# ships, and for the same reasons, custom_art/ and generatedmaps/ are absent
# because the import is what produces them.
for d in blocks scenes config map_edits; do
    cp -r "$REPO/mod_runtime/$d" "$OUT/mod_runtime/"
done
cp "$REPO/mod_runtime/pks_flag_registry.txt" "$OUT/mod_runtime/"

# The importers themselves. The launcher runs these in-process through the
# embedded interpreter, and finds them by looking for tools/assetpack and
# tools/romimport relative to the working directory (main.c's boot_launcher).
echo "==> importers"
mkdir -p "$OUT/tools/assetpack" "$OUT/tools/romimport"
cp "$REPO"/tools/assetpack/*.py "$OUT/tools/assetpack/"
cp "$REPO"/tools/romimport/*.py "$OUT/tools/romimport/"
# Every top-level tools/*.py, not a hand-picked list. build_pak.py imports
# several of these sideways (extract_audio, extract_map_objects), and a curated
# list goes stale silently: the failure is a ModuleNotFoundError minutes into an
# import the player started. They come to about 1 MB in total.
cp "$REPO"/tools/*.py "$OUT/tools/"

cp "$REPO"/pokered-master/*.sym "$OUT/pokered-master/"

# Repo-relative source files the extractors read, found rather than listed.
#
# Shared with make_release.sh through one scanner, so the sandbox and the
# shipping bundle cannot disagree about what an import needs.
echo "==> repo files the extractors read"
"$PY" "$REPO/tools/dist/scan_extractor_sources.py" "$REPO" | tr -d '
' | while IFS= read -r rel; do
    [ -n "$rel" ] || continue
    mkdir -p "$OUT/$(dirname "$rel")"
    cp "$REPO/$rel" "$OUT/$rel"
    echo "   $rel"
done

# The embedded interpreter needs its DLL beside the exe. The stdlib still comes
# from the system Python install, so this sandbox is not a self-contained
# distribution; it only has to be a faithful first run.
[ -f "$(dirname "$PY")/python311.dll" ] && cp "$(dirname "$PY")/python311.dll" "$OUT/"

# Logs and IPC from a previous test run. Windows can keep a handle on a log
# after the process is gone, refusing a delete while allowing a truncate, so
# emptying is the fallback. Either outcome leaves a zero-length log.
for f in "$OUT"/pokered_log.txt "$OUT"/oldamber_log.txt "$OUT"/*.log; do
    [ -e "$f" ] || continue
    rm -f "$f" 2>/dev/null || : > "$f" 2>/dev/null || true
done
rm -rf "$OUT"/bugs "$OUT"/saves_backup 2>/dev/null || true

# Prove there is no game data in here.
echo "==> checking the sandbox really is empty"
fail=0
for p in assets.pak packages generated mod_runtime/generatedmaps \
         mod_runtime/custom_art pokered.sav pokeblue.sav; do
    if [ -e "$OUT/$p" ]; then echo "  LEFTOVER: $p"; fail=1; fi
done
# A .gbc here would mean the sandbox ships ROM bytes.
roms="$(find "$OUT" -iname '*.gbc' -o -iname '*.gb' | head)"
[ -n "$roms" ] && { echo "  ROM DATA PRESENT:"; echo "$roms"; fail=1; }
[ "$fail" -eq 0 ] || { echo "REFUSING: sandbox is not clean"; exit 1; }

echo
echo "clean. $(du -sh "$OUT" | cut -f1) at $OUT"
echo "run:  (cd '$OUT' && ./OldAmber.exe)"
