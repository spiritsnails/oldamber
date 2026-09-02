#!/usr/bin/env bash
# One command from a fresh clone plus a Red ROM to a playable build.
#
#   bash tools/setup_red.sh /path/to/your/pokered.gbc
#
# Nothing ROM-derived is committed, so a fresh clone has an empty generated/ and
# an empty packages/. Several tools have to run in order before CMake can even
# configure, and this script is where that order lives.
#
# It does not need a Crystal ROM: it configures RED_ONLY=ON, which swaps the
# generated Crystal data files for stubs/crystal/crystal_stubs.c.
#
# Everything it writes goes to generated/, packages/ and build-red/, all of
# which are gitignored. Re-running it is safe.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROM="${1:-}"
BUILD_DIR="${BUILD_DIR:-build-red}"

case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) IS_WIN=1 ;; *) IS_WIN=0 ;; esac

# Whatever this machine actually has. The Windows path is a fallback for the
# layout the project was developed on, not an assumption about yours.
PY="$(command -v python3 || command -v python || true)"
if [ -z "$PY" ] && [ -x "C:/Program Files/Python311/python.exe" ]; then
    PY="C:/Program Files/Python311/python.exe"
fi
if [ -z "$PY" ]; then
    echo "no python3 found on PATH -- install Python 3.8 or newer" >&2
    exit 2
fi

if [ "$IS_WIN" = 1 ]; then
    CMAKE_GEN="MinGW Makefiles"
    EXE_SUFFIX=".exe"
else
    CMAKE_GEN="Unix Makefiles"
    EXE_SUFFIX=""
fi

if [ -z "$ROM" ]; then
    echo "usage: bash tools/setup_red.sh <path-to-your-pokemon-red.gbc>" >&2
    echo "" >&2
    echo "This project ships extractors, not assets. You supply the ROM;" >&2
    echo "everything else is built from it on your machine." >&2
    exit 2
fi
if [ ! -f "$ROM" ]; then
    echo "no such file: $ROM" >&2
    exit 2
fi

cd "$ROOT"

echo "==> 1/6  asset pack (this is the big one)"
"$PY" tools/assetpack/build_pak.py --rom "$ROM"

# Must follow build_pak: the bindings are generated from the pack's contents,
# and a stale assetpack_bind.c fails as a missing symbol at compile time rather
# than as a missing asset at boot.
echo "==> 2/6  asset bindings"
"$PY" tools/assetpack/gen_bindings.py

# The full import, which has to stay in step with RomImport_EmitKantoMaps in
# src/platform/rom_import*.c. Those are the only two definitions of what
# importing a ROM means, and the verification block at the end is what catches
# them drifting.
#
# Order matters: --all reads the tiles --art-all wrote, so running it first
# produces blocks referencing art that does not exist yet.
echo "==> 3/6  map tile art"
"$PY" tools/romimport/emit_kanto.py --rom "$ROM" --art-all

echo "==> 4/6  maps + ROM text"
"$PY" tools/romimport/emit_kanto.py --rom "$ROM" --all
"$PY" tools/romimport/emit_scene_text.py --rom "$ROM"

# CMakeLists defaults CMAKE_BUILD_TYPE to Debug, which is right for day-to-day
# work and wrong for anything a player runs: -g -O0 makes the binary 5.09 MB
# instead of 3.00 MB and leaves it unoptimised. Asking for Release here rather
# than changing that default keeps debug info in developer builds.
#
# Do not add -ffunction-sections -fdata-sections -Wl,--gc-sections here. On this
# MinGW/PE toolchain every small data section is padded to section alignment and
# the binary goes from 3 MB to 121 MB.
echo "==> 5/6  configure (RED_ONLY: no Crystal ROM needed)"
cmake -S . -B "$BUILD_DIR" -G "$CMAKE_GEN" -DRED_ONLY=ON -DCMAKE_BUILD_TYPE=Release

echo "==> 6/6  build"
BUILD_DIR="$BUILD_DIR" bash tools/build.sh pokered
BUILD_DIR="$BUILD_DIR" bash tools/build.sh oldamber_bootstrap

echo ""
echo "done -- $BUILD_DIR/oldamber$EXE_SUFFIX"

# Did the import actually produce everything the game needs?
#
# Every failure this catches otherwise presents as a running game that is subtly
# wrong: no map geometry reads as garbage maps, no text table reads as empty
# dialogue boxes. A build that cannot boot correctly must not report success.
missing=0
check_file() {
    if [ ! -s "$1" ]; then
        echo "MISSING: $1  ($2)" >&2
        missing=1
    fi
}
check_count() {
    n=$(ls $1 2>/dev/null | wc -l)
    if [ "$n" -lt "$2" ]; then
        echo "TOO FEW: $1 -- found $n, expected at least $2  ($3)" >&2
        missing=1
    fi
}

# These paths are keyed by package id: generated/red/, generatedmaps/blue/.
#
# Globbed rather than resolving the id, because this only has to answer whether
# the extraction produced its output, and the tools that write it already agree
# on where through build_pak's sha1 map.
check_file "assets.pak"                                          "asset pack"
check_count "generated/*/assetpack_bind.c"                  1    "asset bindings"
check_count "mod_runtime/generatedmaps/*/scene_text.tbl"    1    "ROM dialogue; without it every text box is blank"
check_count "mod_runtime/generatedmaps/*/blocks/*.block"  200    "map geometry; without it maps render as garbage"
check_count "mod_runtime/custom_art/kanto/*.bin"          100    "map tile art"

if [ "$missing" -ne 0 ]; then
    echo "" >&2
    echo "setup did NOT complete: the game would run but be broken." >&2
    exit 1
fi
echo "verified: pack, bindings, maps, tile art and text table all present"

echo "run it with: bash tools/launch_game.sh"
