#!/usr/bin/env bash
# The only supported way to build this project.
#
# Do not call mingw32-make or gcc directly. A repository-local guard stops that,
# because getting it wrong does not fail in a
# way that points at itself: with the ambient Windows PATH, make and gcc are
# both found, but gcc loads its backend (cc1) and the DLLs it depends on from
# whatever else is on PATH, and the result is
#
#       src/game/debug_cli.c: internal compiler error: Segmentation fault
#
# which reads like a compiler bug in a large source file. The file is fine; the
# PATH is wrong.
#
# Usage:
#   bash tools/build.sh                # the game
#   bash tools/build.sh battle_harness # a specific target
#   BUILD_DIR=build-red bash tools/build.sh   # a different configured tree
#
# BUILD_DIR exists for the RED_ONLY tree (a Red standalone with no Crystal
# data), which is configured separately:
#   cmake -S . -B build-red -G "MinGW Makefiles" -DRED_ONLY=ON
# It goes through this script like everything else: the PATH problem above is a
# property of the toolchain, not of which directory is being built.
set -euo pipefail

case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) IS_WIN=1 ;; *) IS_WIN=0 ;; esac

if [ "$IS_WIN" = 1 ]; then
    export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"
    MAKE_BIN="/c/msys64/mingw64/bin/mingw32-make.exe"
    PY_BIN="C:/Program Files/Python311/python.exe"
else
    MAKE_BIN="$(command -v make || echo make)"
    PY_BIN="$(command -v python3 || command -v python || echo python3)"
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET="${1:-}"

# The game holds its own .exe open, so a rebuild while it is running fails at
# the link step with "permission denied". Kill it first.
#
# Absolute path on purpose: bare `taskkill` is not on the PATH this script
# exports (mingw64 and msys usr/bin, no System32), so it would fail with
# "command not found", swallowed by the redirect and `|| true`, and the kill
# would be a silent no-op. The fallback keeps a missing System32 or an
# already-dead game from aborting the build.
#
# Two names: a normal dev build's CMake target is `pokered` and outputs
# pokered.exe, while a RED_ONLY checkout renames OUTPUT_NAME to oldamber.exe
# under the same target. Kill both; this script is shared by both checkouts.
#
# Rebuild-progress handoff: if the game is running, quicksave to a well-known
# slot ("prebuild") first. That uses the full save-state system
# (Save_StateWrite), so it captures live runtime state a regular save file does
# not (mid-battle, vmap bindings, scene state). tools/launch_game.sh restores
# and deletes it on the next launch, so the round trip needs no manual step.
# Best-effort throughout: a rebuild must never be blocked on it.
#
# tasklist.exe, not `ps`: MSYS2's ps only enumerates processes reachable from
# this bash's own session tree, so it misses a game launched from a different
# shell, while taskkill.exe below kills system-wide regardless of session. A
# ps-based check could report "not running" for a game taskkill is about to
# kill, skipping the quicksave.
#
# Clear any stale handoff before deciding whether to take a new one: the file
# must only ever exist because this build just wrote it. A leftover
# qs_prebuild.state from an earlier build would be restored over the save the
# game just loaded, and the next in-game save would write that rewound state
# back to pokered.sav.
#
# Which bugs/ directory the running game uses: game_cli.py resolves it by
# looking for the exe next to it, so mirror that exactly and the two can never
# disagree about where the handoff lives.
_handoff_dir() {
    if   [ -f "$ROOT/build-red/oldamber.exe" ]; then echo "build-red/bugs"
    elif [ -f "$ROOT/build/pokered.exe" ];     then echo "build/bugs"
    elif [ -f "$ROOT/build-blue/oldamber.exe" ]; then echo "build-blue/bugs"
    else echo "${BUILD_DIR:-build}/bugs"; fi
}

# Clear every candidate, not just ${BUILD_DIR}. The file is written by the game,
# into whichever bugs/ directory game_cli.py resolves, and that can differ from
# ${BUILD_DIR:-build} whenever the env var is unset in a checkout whose live
# game is build-red/oldamber.exe. Clearing all of them is free and cannot be got
# wrong by a missing env var.
for _bd in build build-red build-blue "${BUILD_DIR:-build}"; do
    rm -f "$ROOT/$_bd/bugs/qs_prebuild.state"
done

if [ "$IS_WIN" = 1 ]; then
    for exe in pokered.exe oldamber.exe; do
        if /c/Windows/System32/tasklist.exe //FI "IMAGENAME eq $exe" 2>/dev/null \
                | grep -qi "$exe"; then
            echo "[build] $exe is running -- quicksaving before rebuild..."
            "$PY_BIN" "$ROOT/tools/game_cli.py" \
                "quicksave prebuild" >/dev/null 2>&1 || true
            # Wait for the file to stop growing before killing the game.
            #
            # game_cli.py returns as soon as the game consumes cli_cmd.txt and
            # its live.json heartbeat ticks. That is delivery, not completion:
            # Save_StateWrite is still running. The taskkill below is /F, an
            # immediate unblockable termination, so without this it would land
            # mid-write and leave a truncated qs_prebuild.state. Windows-only,
            # because the POSIX branch uses pkill (SIGTERM), which lets the
            # write finish.
            #
            # Poll the size until it holds steady, then allow the kill.
            _qs="$ROOT/$(_handoff_dir)/qs_prebuild.state"
            _last=-1; _same=0
            for _i in $(seq 1 40); do
                _sz=$(wc -c < "$_qs" 2>/dev/null || echo -1)
                if [ "$_sz" != "-1" ] && [ "$_sz" = "$_last" ]; then
                    _same=$((_same + 1))
                    [ "$_same" -ge 3 ] && break
                else
                    _same=0
                fi
                _last="$_sz"
                sleep 0.1
            done
            if [ ! -s "$_qs" ]; then
                echo "[build] WARNING: pre-rebuild quicksave did not appear."
                echo "[build]   Unsaved progress since your last in-game SAVE will be lost."
            fi
        fi
    done

    for exe in pokered.exe oldamber.exe; do
        /c/Windows/System32/taskkill.exe //F //IM "$exe" >/dev/null 2>&1 \
            || taskkill //F //IM "$exe" >/dev/null 2>&1 || true
    done
else
    for exe in pokered oldamber; do
        if pgrep -x "$exe" >/dev/null 2>&1; then
            "$PY_BIN" "$ROOT/tools/game_cli.py" \
                "quicksave prebuild" >/dev/null 2>&1 || true
            pkill -x "$exe" >/dev/null 2>&1 || true
        fi
    done
fi

"$MAKE_BIN" -C "$ROOT/${BUILD_DIR:-build}" ${TARGET:+"$TARGET"}

# Does every .block reference art an extractor can rebuild?
#
# custom_art/kanto is gitignored and rebuilt from the player's ROM, so a block
# pointing at a file no extractor produces looks fine here and is missing for
# everyone else. Advisory only.
"$PY_BIN" "$ROOT/tools/check_art_regenerable.py" \
    --quiet >/dev/null 2>&1 || {
    echo ""
    echo "WARNING: a .block references tile art no extractor regenerates."
    echo "  A tester building from their own ROM will be missing it."
    echo "  details: \"C:/Program Files/Python311/python.exe\" tools/check_art_regenerable.py"
    echo ""
}

# Are the RED_ONLY stubs still in step with the real headers?
#
# stubs/crystal/*.h are committed mirrors of the gitignored generated/*.h. If an
# extractor changes a header, adding a symbol or changing a dimension, the
# mirror goes stale and a RED_ONLY build compiles against a declaration that no
# longer matches. Advisory only: on a fresh checkout generated/ is empty and
# there is nothing to compare, which is normal and must not fail.
if [ -d "$ROOT/generated" ] && [ -f "$ROOT/generated/crystal_font.h" ]; then
    "$PY_BIN" "$ROOT/tools/gen_crystal_stubs.py" \
        --check >/dev/null 2>&1 || {
        echo ""
        echo "WARNING: stubs/crystal is STALE -- a generated Crystal header changed."
        echo "  RED_ONLY builds would compile against out-of-date declarations."
        echo "  refresh: \"C:/Program Files/Python311/python.exe\" tools/gen_crystal_stubs.py"
        echo ""
    }
fi

# Is the asset pack still current for this binary?
#
# Adding a bound asset and forgetting to rebuild the pack produces a game that
# refuses to boot with "asset X is missing from every mounted package". That
# fires at launch, so without this check the first person to see it is whoever
# tries to play, not whoever built.
#
# Compare what assetpack_bind.c requires against what the pack actually holds.
# Advisory only: a missing ROM or pack is normal for a fresh checkout and must
# not fail the build.
PAK="$ROOT/packages/red.pak"
BIND="$ROOT/generated/assetpack_bind.c"
if [ -f "$PAK" ] && [ -f "$BIND" ]; then
    missing="$("$PY_BIN" - "$PAK" "$BIND" <<'PY' 2>/dev/null || true
import re, sys, struct
pak, bind = sys.argv[1], sys.argv[2]
want = set(re.findall(r'AssetPack_Require\("([^"]+)"', open(bind, encoding='utf-8').read()))
data = open(pak, 'rb').read()
have = set(re.findall(rb'[ -~]{3,}', data))
have = {h.decode() for h in have}
gone = sorted(w for w in want if w not in have)
print(" ".join(gone[:4]) + (" ..." if len(gone) > 4 else "") if gone else "")
PY
)"
    if [ -n "$missing" ]; then
        echo ""
        echo "WARNING: the asset pack is STALE for this binary."
        echo "  missing: $missing"
        echo "  rebuild: \"C:/Program Files/Python311/python.exe\" tools/assetpack/build_pak.py --rom <your-rom.gbc>"
        echo ""
    fi
fi
