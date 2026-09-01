#!/usr/bin/env bash
# make_release.sh: build the folder that gets sent to a playtester.
#
#     bash tools/dist/make_release.sh
#
# Produces build/OldAmber/ containing the game, its runtime data, and a frozen
# first-run setup tool. Nothing ROM-derived is included: the tester supplies
# their own Red ROM and setup.exe builds assets.pak and the map tiles on their
# machine. See src/platform/assetpack.h.
#
# Deliberately left out of mod_runtime:
#   custom_art/     ROM-derived tile art. setup.exe regenerates it through
#                   emit_kanto.py --art-all.
#   generatedmaps/  Johto, imported from Crystal. Not part of a Kanto build.
#   map_export/     debug_cli only.
#   python/         debug_cli only.
#   _archive/       what it says.
#
# The frozen tool bundles a few repo files under their normal relative paths, so
# _MEIPASS behaves like a checkout root and each module resolves REPO through
# sys._MEIPASS when frozen. None of them is ROM data. pokered.sym is among them:
# it is `bank:addr Name` lines and contains no ROM bytes.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
WIN_REPO="$(cd "$REPO" && pwd -W 2>/dev/null || echo "$REPO")"
# Product naming, deliberately not the original's. The engine is "Amber Engine"
# and this build of Red is "OldAmber". Trademark is a separate exposure from the
# asset policy this script enforces below: a bundle can ship zero ROM-derived
# bytes and still infringe a mark by what the download is called. So the
# distributed folder, the binary and the window caption all carry the product
# name, while the dev-side names (the CMake target, pokered.exe, this repo's
# directory) stay internal.
OUT="$REPO/build/OldAmber"
PY="${PY:-C:/Program Files/Python311/python.exe}"

echo "==> game"
# Must be the RED_ONLY tree. The plain build/ tree is the developer build and
# configures RED_ONLY=OFF, which links every generated/crystal_*.c, about
# 4.51 MiB of Crystal ROM data, straight into the .exe.
GAME_DIR="${GAME_DIR:-$REPO/build-red}"
# The RED_ONLY target links as oldamber.exe; the full checkout's is pokered.exe.
# Take whichever exists.
GAME_EXE=""
for _cand in oldamber.exe pokered.exe; do
    [ -f "$GAME_DIR/$_cand" ] && { GAME_EXE="$_cand"; break; }
done
[ -n "$GAME_EXE" ] || {
    echo "no oldamber.exe or pokered.exe in $GAME_DIR -- run: bash tools/setup_red.sh <your-red-rom.gbc>"
    exit 1
}
if ! grep -q '^RED_ONLY:BOOL=ON$' "$GAME_DIR/CMakeCache.txt" 2>/dev/null; then
    echo "REFUSING: $GAME_DIR is not a RED_ONLY build."
    echo "Its .exe would embed ~4.51 MiB of Crystal ROM data."
    echo "Reconfigure with: bash tools/setup_red.sh <your-red-rom.gbc>"
    exit 1
fi
# And it must be a Release build. The RED_ONLY check above is not enough: the
# two settings are independent, and rerunning configure by hand leaves
# CMAKE_BUILD_TYPE empty while RED_ONLY stays cached ON. That ships a 5.1 MB
# unoptimised -O0 binary instead of a 3.0 MB one. Read from the cache, because
# empty is the dangerous value and it is invisible from this script otherwise.
BUILD_TYPE="$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$GAME_DIR/CMakeCache.txt" 2>/dev/null)"
if [ "$BUILD_TYPE" != "Release" ]; then
    echo "REFUSING: $GAME_DIR is not a Release build."
    echo "  CMAKE_BUILD_TYPE = '${BUILD_TYPE:-<empty>}', expected 'Release'."
    echo "An unoptimised -O0 binary is ~5.1 MB instead of ~3.0 MB and runs slower."
    echo "Reconfigure with: bash tools/setup_red.sh <your-red-rom.gbc>"
    echo "  (or: cmake -S . -B ${GAME_DIR##*/} -G \"MinGW Makefiles\" \\"
    echo "         -DRED_ONLY=ON -DCMAKE_BUILD_TYPE=Release)"
    exit 1
fi

# The debug traces default on so a developer build keeps them; turning them off
# is this path's job. Read from the cache for the same reason as
# CMAKE_BUILD_TYPE above: the difference is invisible in the binary until a
# player sees "[E4DBG] tick curmap=..." scroll past every frame.
DEBUG_PRINTS="$(sed -n 's/^AMBER_DEBUG_PRINTS:BOOL=//p' "$GAME_DIR/CMakeCache.txt" 2>/dev/null)"
if [ "$DEBUG_PRINTS" != "OFF" ]; then
    echo "REFUSING: $GAME_DIR still has the debug traces compiled in."
    echo "  AMBER_DEBUG_PRINTS = '${DEBUG_PRINTS:-<unset>}', expected 'OFF'."
    echo "The bracketed [E4DBG]/[PALDBG]/[PCNPCDBG] traces would print to the"
    echo "player's console. See src/platform/debug_log.h."
    echo "Reconfigure with: cmake -S . -B ${GAME_DIR##*/} -G \"MinGW Makefiles\" \\"
    echo "         -DRED_ONLY=ON -DCMAKE_BUILD_TYPE=Release -DAMBER_DEBUG_PRINTS=OFF"
    exit 1
fi

echo "==> freezing setup.exe"
# Which repo files to embed is computed, not listed. A hardcoded list drifts,
# and a frozen setup built from a stale one gets a long way into an import
# before it fails on a file nobody remembered. scan_extractor_sources.py answers
# the question directly.
# Every symbol file, not just Red's. This line named pokered.sym alone, so a
# Windows bundle could not import Blue however many .sym files the tree held,
# and the failure landed on the player long after packaging reported success.
# The Linux and macOS packagers already globbed; this one did not.
ADD_DATA=()
for _sym in "$REPO"/pokered-master/*.sym; do
    [ -f "$_sym" ] || continue
    ADD_DATA+=( --add-data "$(cygpath -w "$_sym");pokered-master" )
done
[ ${#ADD_DATA[@]} -gt 0 ] || { echo "no pokered-master/*.sym to embed" >&2; exit 1; }
while IFS= read -r rel; do
    [ -n "$rel" ] || continue
    [ -f "$REPO/$rel" ] || { echo "scan_extractor_sources names $rel, which is missing" >&2; exit 1; }
    ADD_DATA+=( --add-data "$WIN_REPO/$rel;$(dirname "$rel")" )
done < <("$PY" "$REPO/tools/dist/scan_extractor_sources.py" "$REPO" | tr -d '\r')
echo "    embedding $(( ${#ADD_DATA[@]} / 2 )) file(s)"

"$PY" -m PyInstaller --noconfirm --clean --onefile \
  --name setup \
  --distpath "$REPO/build/dist_tmp" \
  --workpath "$REPO/build/pyi_work" \
  --specpath "$REPO/build/pyi_work" \
  --paths "$WIN_REPO/tools/assetpack" \
  --paths "$WIN_REPO/tools/romimport" \
  --paths "$WIN_REPO/tools" \
  --hidden-import build_pak --hidden-import emit_kanto \
  --hidden-import emit_scene_text \
  --hidden-import gen1_audio --hidden-import gen1_pic \
  --hidden-import port_overrides --hidden-import pak \
  --hidden-import extract_audio \
  "${ADD_DATA[@]}" \
  "$WIN_REPO/tools/dist/setup_assets.py" >/dev/null

echo "==> assembling $OUT"
# Clear the CONTENTS rather than the directory itself: on Windows an open
# Explorer window holds a handle on the folder, and `rm -rf $OUT` then fails
# with "Device or resource busy" even though everything inside deleted fine.
mkdir -p "$OUT"
find "$OUT" -mindepth 1 -maxdepth 1 -exec rm -rf {} + 2>/dev/null || true
mkdir -p "$OUT/mod_runtime"
cp "$GAME_DIR/$GAME_EXE" "$OUT/OldAmber.exe"
cp "$GAME_DIR/SDL2.dll"    "$OUT/"
# internal/, not the top level. Next to the game, setup.exe reads as the
# installer a player is meant to run first, and it is not one: the launcher runs
# it. One executable in the folder a player opens.
mkdir -p "$OUT/internal"
cp "$REPO/build/dist_tmp/setup.exe" "$OUT/internal/"
cp "$REPO/tools/dist/README.txt" "$OUT/"

# The project's own licence, by the same rule the block below states for
# everyone else's: MIT requires its notice in copies and substantial portions,
# and a bundle is a copy. THIRD_PARTY.md covers the code this project did not
# write; this covers the code it did.
cp "$REPO/LICENSE" "$OUT/"

# The third-party notices have to travel with the binary, which is the only
# artifact a player receives.
#
# SameBoy's is compelled: Expat requires its notice in binary distributions, and
# SameBoy-derived C is compiled into OldAmber.exe (display.c's MONO_PALETTES,
# temperature_tint and colour-correction LUT, and gb_apu.c), not just the
# shaders.
#
# The rest are given by choice. NTSC-CRT's licence makes attribution explicitly
# optional, and SDL2's zlib terms ask for acknowledgement without requiring it.
cp "$REPO/THIRD_PARTY.md" "$OUT/"

# Shaders are loaded from disk, so leaving them out does not fall back to
# anything, it removes a feature. DisplayGL_ShaderDir() looks for shaders/ next
# to the exe, then one level up, then in the CWD, and the CRT renderer reads
# shaders/crt/*.{vert,frag} through that same directory. Without the copy the
# player gets "[crt] no shaders/ directory found" on a stdout nobody reads, and
# the SGB and CRT presentation modes do nothing.
#
# The whole directory, not just crt/. try_shader_dir() probes for
# MasterShader.fsh specifically, so a bundle carrying only shaders/crt/ would
# fail that probe and disable the CRT renderer too.
#
# This ships SameBoy's .fsh files verbatim, which is why the THIRD_PARTY.md copy
# above is not optional.
cp -r "$REPO/shaders" "$OUT/"

# CPython ships inside setup.exe, since PyInstaller embeds the interpreter and
# the stdlib, so the PSF agreement travels with the bundle even though no .py
# file appears in it. Sourced from the interpreter that built setup.exe, so the
# text matches the version actually embedded.
PY_LICENSE="$(dirname "$PY")/LICENSE.txt"
if [ -f "$PY_LICENSE" ]; then
    cp "$PY_LICENSE" "$OUT/LICENSE-Python.txt"
else
    echo "WARNING: no LICENSE.txt beside $PY -- the PSF notice is NOT in the bundle" >&2
fi
# mod_runtime/generated/ is not in this list and must never be: it holds raw
# Crystal 2bpp tile dumps, which are ROM-derived. The sanity check below has a
# .bin rule so a copy of it cannot pass unnoticed.
for d in blocks scenes config map_edits; do
    cp -r "$REPO/mod_runtime/$d" "$OUT/mod_runtime/"
done
cp "$REPO/mod_runtime/pks_flag_registry.txt" "$OUT/mod_runtime/"

# The Test*.block fixtures are development scaffolding: no scene binds them, so
# a player cannot reach one. They ship declaring `subtile` art under
# mod_runtime/custom_art, which the cleanup below deliberately removes, so the
# tile loader hunted 160 PNGs that are not in the bundle and wrote a "PNG not
# found" line for each into the player's log on first boot. Nothing was broken
# by it, which is the problem: a log that cries wolf on a clean install is one
# nobody reads when something does go wrong.
rm -f "$OUT/mod_runtime/blocks"/Test*.block
# Presentation defaults are compile-time, not injected here: gbc_color.c and
# gen1color_battle.c default to colour off, Gen 1 HUD and Gen 1 sprites. Setting
# them in the binary rather than through the startup config means the package
# cannot boot into the wrong presentation because a config file was missed.

# A tester's tree must never contain ROM-derived data. Running setup here
# during a smoke test would leave exactly that behind, so refuse to ship it.
rm -rf "$OUT/assets.pak" "$OUT/mod_runtime/custom_art" "$OUT/bugs" \
       "$OUT/pokered.sav" "$OUT"/*.log "$OUT"/run_log.txt 2>/dev/null || true

echo
echo "ready: $OUT"
du -sh "$OUT"
echo
echo "Sanity check -- these MUST be absent from what you send:"
# Every shape ROM-derived data takes in this tree, so the guard cannot print
# "nothing listed = good" over a leak it has no rule for.
find "$OUT" \( -name '*.pak' -o -name custom_art -o -name '*.gb' \
              -o -name '*.gbc' -o -name '*.bin' -o -name '*_tiles.txt' \
              -o -name '*.2bpp' -o -name generated \) -print
echo "(nothing listed above = good)"

# The inverse guard: things that must be present. A missing shader is invisible
# at runtime, a printf on a stdout no player reads, so it has to be caught here
# or not at all.
missing=0
for f in "OldAmber.exe" "SDL2.dll" "internal/setup.exe" "README.txt" "THIRD_PARTY.md" "LICENSE" \
         "shaders/MasterShader.fsh" "shaders/crt/tube.frag" "shaders/crt/tube.vert" \
         "shaders/crt/blur.frag" "shaders/crt/blur.vert" "shaders/crt/final.frag"; do
    if [ ! -e "$OUT/$f" ]; then
        echo "MISSING FROM BUNDLE: $f" >&2
        missing=1
    fi
done
if [ "$missing" -ne 0 ]; then
    echo "refusing to call this bundle ready" >&2
    exit 1
fi
echo "(all required files present)"

# The list above cannot catch a dependency nobody remembered to add to it, and a
# missing runtime DLL fails in the Windows loader before main(), with an error
# box and no log.
#
# So ask the binary what it needs instead: every non-system DLL it imports must
# be sitting beside it in the bundle. A new link-time dependency then fails
# packaging on the day it appears.
DLL_DEPS_OK=1
OBJDUMP="${OBJDUMP:-/c/msys64/mingw64/bin/objdump.exe}"
if [ -x "$OBJDUMP" ]; then
    # Everything a stock Windows install already provides. Anything outside
    # this set has to travel with us.
    SYSTEM_DLL_RE='^(KERNEL32|USER32|GDI32|ADVAPI32|SHELL32|ole32|OLEAUT32|msvcrt|WS2_32|IMM32|WINMM|SETUPAPI|VERSION|dwmapi|UxTheme|OPENGL32|COMDLG32|SHLWAPI|bcrypt|CFGMGR32|RPCRT4|USERENV|CRYPT32|POWRPROF|HID|AVRT|DINPUT8|ntdll|api-ms-win-)'
    for dll in $("$OBJDUMP" -p "$OUT/OldAmber.exe" 2>/dev/null \
                 | sed -n 's/^[[:space:]]*DLL Name:[[:space:]]*//p' | sort -u); do
        echo "$dll" | grep -Eqi "$SYSTEM_DLL_RE" && continue
        if [ ! -e "$OUT/$dll" ]; then
            echo "MISSING RUNTIME DLL: OldAmber.exe imports $dll, not in the bundle" >&2
            echo "  the game will fail to start in the loader, with no log" >&2
            DLL_DEPS_OK=0
        fi
    done
    if [ "$DLL_DEPS_OK" -ne 1 ]; then
        echo "refusing to ship a bundle that cannot start" >&2
        exit 1
    fi
    echo "(every imported DLL is present in the bundle)"
else
    echo "WARNING: no objdump at $OBJDUMP -- could NOT verify runtime DLLs." >&2
    echo "         Set OBJDUMP=... to check that the bundle can actually start." >&2
fi

# ---- the archive that actually gets uploaded --------------------------------
# Packaging used to stop at a folder, so every upload was zipped by hand and
# nothing afterwards could say what a given download contained. Building the
# archive here makes the thing that was tested and the thing that is published
# the same file, and the checksum beside it identifies which build it came from.
#
# Windows ships bsdtar as System32\tar.exe, which writes zip directly. MSYS's
# own tar is GNU tar and cannot, and neither powershell nor pwsh is on the PATH
# this script runs under, so both are called by absolute path if bsdtar is
# missing. Everything here takes Windows paths, hence cygpath.
VERSION="${VERSION:-0.0.2}"
ARCHIVE="$REPO/build/OldAmber-$VERSION-windows-x64.zip"
WIN_TAR="/c/Windows/System32/tar.exe"
WIN_PS="/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe"
echo
echo "==> archiving"
rm -f "$ARCHIVE" "$ARCHIVE.sha256"
if [ -x "$WIN_TAR" ]; then
    # Archive the FOLDER, not its contents. `-C "$OUT" .` stamped ./ on every
    # entry and unpacked flat, so a player got nine loose items wherever they
    # extracted it. This matches the Linux tarball, which has always had a
    # OldAmber-linux/ root.
    "$WIN_TAR" -a -c -f "$(cygpath -w "$ARCHIVE")" \
        -C "$(cygpath -w "$(dirname "$OUT")")" "$(basename "$OUT")" \
        >/dev/null 2>&1 || true
elif [ -x "$WIN_PS" ]; then
    "$WIN_PS" -NoProfile -NonInteractive -Command \
      "Compress-Archive -Path '$(cygpath -w "$OUT")' -DestinationPath '$(cygpath -w "$ARCHIVE")' -Force" \
      >/dev/null 2>&1 || true
fi
if [ -f "$ARCHIVE" ]; then
    echo "archive: $ARCHIVE  ($(du -h "$ARCHIVE" | cut -f1))"
    # Prove the archive is readable rather than assume it.
    if [ -x "$WIN_TAR" ] && ! "$WIN_TAR" -tf "$(cygpath -w "$ARCHIVE")" >/dev/null 2>&1; then
        echo "WARNING: $ARCHIVE does not list. Do not publish it." >&2
    fi
    if command -v sha256sum >/dev/null 2>&1; then
        (cd "$(dirname "$ARCHIVE")" &&
         sha256sum "$(basename "$ARCHIVE")" | tee "$(basename "$ARCHIVE").sha256")
    fi
else
    echo "WARNING: could not create $ARCHIVE. Upload the folder by hand." >&2
fi
