#!/usr/bin/env bash
# make_release_linux.sh: the Linux / Steam Deck bundle.
#
#     bash tools/dist/make_release_linux.sh
#     (then run build/OldAmber-linux/OldAmber.sh)
#
# Produces build/OldAmber-linux/. A player needs their ROM, the setup binary and
# the game, so nothing else goes in: no loose importer scripts and no
# disassembly checkout. Every .py the import needs, and the symbol table it
# reads, are frozen inside setup, which works because build_pak.py resolves its
# repo root as `sys._MEIPASS or <build_pak.py>/../../..`, so PyInstaller's
# unpack directory behaves like a checkout. The Windows bundle does the same.
#
# Nothing ROM-derived is here either. assets.pak, generatedmaps/ and
# custom_art/ are built on the player's machine from the ROM they supply.
#
# The game is built AMBER_EMBED_PYTHON=OFF, which keeps libpython out of the
# ELF's NEEDED list: a binary linked against one minor version will not start on
# a host carrying another, and a SteamOS player cannot install the missing one.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${OUT:-$REPO/build/OldAmber-linux}"
GAME_DIR="${GAME_DIR:-$REPO/build-nopy}"

say() { printf '[linux] %s\n' "$*"; }
die() { printf '[linux] ERROR: %s\n' "$*" >&2; exit 1; }

case "$(uname -s)" in
    Linux) ;;
    *) die "this packages a Linux build and must run on Linux (uname -s = $(uname -s))" ;;
esac

# The binary, and the four cache values it has to have been built with.
GAME_EXE="$GAME_DIR/oldamber"
[ -f "$GAME_EXE" ] || die "no oldamber in $GAME_DIR -- build it first:
    cmake -S . -B ${GAME_DIR##*/} -G 'Unix Makefiles' \\
          -DRED_ONLY=ON -DCMAKE_BUILD_TYPE=Release \\
          -DAMBER_DEBUG_PRINTS=OFF -DAMBER_EMBED_PYTHON=OFF
    BUILD_DIR=${GAME_DIR##*/} bash tools/build.sh"

cache="$GAME_DIR/CMakeCache.txt"
want() {
    local key="$1" expect="$2" why="$3" got
    got="$(sed -n "s/^${key}:[A-Z]*=//p" "$cache" 2>/dev/null)"
    [ "$got" = "$expect" ] || die "$GAME_DIR has $key='${got:-<unset>}', expected '$expect'
    $why
    Reconfigure with -D$key=$expect"
}
want RED_ONLY ON \
     "A non-RED_ONLY binary embeds Crystal ROM data, which must never ship."
want CMAKE_BUILD_TYPE Release \
     "A -O0 binary is far larger and slower than the player needs."
want AMBER_DEBUG_PRINTS OFF \
     "The bracketed [E4DBG]/[PALDBG] traces would print to the player's terminal."
want AMBER_EMBED_PYTHON OFF \
     "Embedding puts libpython3.x.so in NEEDED; a container-built binary then
    fails to start on a SteamOS host with a different minor version."

# Assemble.
say "assembling $OUT"
mkdir -p "$OUT"
find "$OUT" -mindepth 1 -maxdepth 1 -exec rm -rf {} + 2>/dev/null || true
mkdir -p "$OUT/mod_runtime"

cp "$GAME_EXE" "$OUT/OldAmber"
chmod +x "$OUT/OldAmber"
cp -r "$REPO/shaders" "$OUT/"
# The Steam library icon. The launcher's "add to Steam library" offer points the
# .desktop it writes at this exact name beside the binary (icon_path() in
# steam_shortcut.c), so renaming it here drops the shortcut back to Steam's
# blank placeholder.
cp "$REPO/oldambericon.png" "$OUT/icon.png"
cp "$REPO/LICENSE" "$REPO/THIRD_PARTY.md" "$OUT/" 2>/dev/null || true

# The player's instructions. The Windows bundle has always carried these and
# this one did not, so a Linux player unpacked the tarball to a game with no
# explanation of what to do with it.
cp "$REPO/tools/dist/README.txt" "$OUT/"

# The PSF licence, which has to travel with the bundle because PyInstaller
# embeds CPython inside setup. Distributions put it in different places, so
# look in the ones that exist rather than assuming one.
PY_LICENSE=""
for cand in \
    "$(python3 -c 'import sysconfig,os;print(os.path.join(sysconfig.get_paths()["stdlib"],"LICENSE.txt"))' 2>/dev/null)" \
    "/usr/lib/python3/LICENSE.txt" \
    "$(ls -d /usr/share/doc/python3.* 2>/dev/null | head -1)/copyright"
do
    [ -n "$cand" ] && [ -f "$cand" ] && { PY_LICENSE="$cand"; break; }
done
if [ -n "$PY_LICENSE" ]; then
    cp "$PY_LICENSE" "$OUT/LICENSE-Python.txt"
    say "PSF licence from $PY_LICENSE"
else
    say "WARNING: no CPython licence found. setup embeds the interpreter, so
    the PSF notice has to ship with it. Find it and copy it in by hand."
fi

# Authored content only. generatedmaps/ and custom_art/ are ROM-derived and are
# produced on the player's machine by the first run.
for d in blocks scenes config map_edits; do
    cp -r "$REPO/mod_runtime/$d" "$OUT/mod_runtime/"
done
cp "$REPO/mod_runtime/pks_flag_registry.txt" "$OUT/mod_runtime/"

# Development scaffolding, see the same removal in make_release.sh: no scene
# binds a Test*.block, and they name `subtile` art under mod_runtime/custom_art
# which no bundle carries, so each one costs the player a "PNG not found" line
# on first boot and gives nothing back.
rm -f "$OUT/mod_runtime/blocks"/Test*.block

# No loose tools/ in the bundle. Every importer .py, and the symbol table they
# read, is frozen into setup below: PyInstaller's --paths pulls them in and
# build_pak.py's _MEIPASS branch makes the unpack directory behave as the repo
# root, so an --add-data'd pokered.sym lands at the path it already looks for.
#
# The --add-data separator is ':' on POSIX where Windows uses ';'.
say "freezing setup (embeds the symbol table)"
SYM="$(ls "$REPO"/pokered-master/*.sym 2>/dev/null | head -1 || true)"
[ -n "$SYM" ] || die "no pokered-master/*.sym -- the frozen tool has nothing to
    embed and first-run import could not start. Run this from a tree that has
    the disassembly checkout."
command -v pyinstaller >/dev/null 2>&1 || python3 -m PyInstaller --version >/dev/null 2>&1 \
    || die "PyInstaller not available -- pip install pyinstaller"
# Which repo files to embed is computed, not listed. The extractors open a
# handful of repo files by path at run time, and a hardcoded --add-data list
# drifts from that set silently: the frozen tool builds fine and then dies part
# way through an import on the player's machine. scan_extractor_sources.py
# answers the question directly.
#
# Every symbol file, not just the first: a tree holding both pokered.sym and
# pokeblue.sym must embed both, or importing a Blue ROM fails on the player's
# machine long after packaging reported success.
ADD_DATA=()
for _sym in "$REPO"/pokered-master/*.sym; do
    [ -f "$_sym" ] && ADD_DATA+=( --add-data "$_sym:pokered-master" )
done
while IFS= read -r rel; do
    [ -n "$rel" ] || continue
    [ -f "$REPO/$rel" ] || die "scan_extractor_sources names $rel, which is missing"
    ADD_DATA+=( --add-data "$REPO/$rel:$(dirname "$rel")" )
done < <(python3 "$REPO/tools/dist/scan_extractor_sources.py" "$REPO" | tr -d '\r')
say "embedding $(( ${#ADD_DATA[@]} / 2 )) file(s) into setup"

python3 -m PyInstaller --noconfirm --clean --onefile \
    --name setup \
    --distpath "$REPO/build/dist_tmp_linux" \
    --workpath "$REPO/build/pyi_work_linux" \
    --specpath "$REPO/build/pyi_work_linux" \
    --paths "$REPO/tools/assetpack" \
    --paths "$REPO/tools/romimport" \
    --paths "$REPO/tools" \
    --hidden-import build_pak --hidden-import emit_kanto \
    --hidden-import emit_scene_text \
    --hidden-import gen1_audio --hidden-import gen1_pic \
    --hidden-import port_overrides --hidden-import pak \
    --hidden-import extract_audio \
    "${ADD_DATA[@]}" \
    "$REPO/tools/dist/setup_assets.py" >/dev/null
# internal/, not the top level. Next to the game, setup reads as the installer
# a player is meant to run first, and it is not one: the launcher runs it. One
# executable in the folder a player opens.
mkdir -p "$OUT/internal"
cp "$REPO/build/dist_tmp_linux/setup" "$OUT/internal/setup"
chmod +x "$OUT/internal/setup"

# The repo files the extractors open by path are --add-data'd into setup above,
# not copied loose. The same list make_release.sh freezes into setup.exe.

# The launcher shim. The binary anchors its own working directory
# (anchor_cwd_to_exe_dir in main.c), so this exists for the double-click case
# and to give Steam a target with a stable name.
cat > "$OUT/OldAmber.sh" <<'LAUNCH'
#!/usr/bin/env bash
cd "$(dirname "$(readlink -f "$0")")"
exec ./OldAmber "$@"
LAUNCH
chmod +x "$OUT/OldAmber.sh"

# Refuse to ship something broken, or something ROM-derived.
say "checking nothing ROM-derived got in"
# pokered-master is in this list because the disassembly checkout never ships in
# any form. The symbol table the importers need is embedded inside the frozen
# setup binary instead, see the freeze above.
strays="$(find "$OUT" \( -name '*.pak' -o -name custom_art -o -name generatedmaps \
                        -o -iname '*.gb' -o -iname '*.gbc' -o -name generated \
                        -o -name '*.sav' -o -name 'pokered-master' \
                        -o -name '*.sym' \) -print)"
[ -z "$strays" ] || { printf '%s\n' "$strays" >&2; die "content that must never ship is in the bundle"; }

missing=0
for f in OldAmber OldAmber.sh internal/setup LICENSE THIRD_PARTY.md README.txt \
         LICENSE-Python.txt icon.png shaders/MasterShader.fsh; do
    [ -e "$OUT/$f" ] || { echo "MISSING FROM BUNDLE: $f" >&2; missing=1; }
done
[ "$missing" -eq 0 ] || die "refusing to call this bundle ready"

# Same idea as the Windows bundle's DLL check: ask the binary what it needs
# rather than maintaining a list. Anything unresolved here is a library the
# player's machine does not have, and the game would die at exec with a linker
# message and no log.
if command -v ldd >/dev/null 2>&1; then
    unresolved="$(ldd "$OUT/OldAmber" 2>/dev/null | grep -i "not found" || true)"
    if [ -n "$unresolved" ]; then
        printf '%s\n' "$unresolved" >&2
        die "unresolved shared libraries -- this bundle will not start here"
    fi
    if ldd "$OUT/OldAmber" 2>/dev/null | grep -qi "libpython"; then
        die "the binary links libpython -- rebuild with -DAMBER_EMBED_PYTHON=OFF"
    fi
    say "shared libraries all resolve, and libpython is absent"
fi

say "ready: $OUT  ($(du -sh "$OUT" | cut -f1))"
say "run:   $OUT/OldAmber.sh"

# ---- the archive that actually gets uploaded --------------------------------
# Packaging used to stop at a folder, so every upload was archived by hand and
# nothing afterwards could say what a given download contained.
#
# tar, not zip: OldAmber, OldAmber.sh and setup all need their executable bit,
# and zip does not carry it. A player unpacking a zip would get files that will
# not run, on the platform least likely to forgive that.
VERSION="${VERSION:-0.0.2}"
ARCHIVE="$REPO/build/OldAmber-$VERSION-linux-x64.tar.gz"
say "archiving"
rm -f "$ARCHIVE" "$ARCHIVE.sha256"
if tar -czf "$ARCHIVE" -C "$(dirname "$OUT")" "$(basename "$OUT")"; then
    say "archive: $ARCHIVE  ($(du -h "$ARCHIVE" | cut -f1))"
    if command -v sha256sum >/dev/null 2>&1; then
        (cd "$(dirname "$ARCHIVE")" &&
         sha256sum "$(basename "$ARCHIVE")" | tee "$(basename "$ARCHIVE").sha256")
    fi
else
    say "WARNING: could not create $ARCHIVE. Upload the folder by hand."
fi
