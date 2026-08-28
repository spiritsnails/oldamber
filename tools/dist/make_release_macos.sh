#!/usr/bin/env bash
# make_release_macos.sh: the macOS bundle, universal (arm64 + x86_64).
#
#     bash tools/dist/make_release_macos.sh
#     (then open build/OldAmber-macos/OldAmber.app)
#
# Produces build/OldAmber-macos/OldAmber.app. Same bottom line as every other
# bundle: a player needs their ROM, the setup binary and the game. No
# disassembly checkout in any form, and nothing ROM-derived. assets.pak,
# generatedmaps/ and custom_art/ are built on the player's machine from the ROM
# they supply.
#
# Not Homebrew's SDL2: brew installs for the host arch only, so linking a
# universal binary against it fails at the link step for the other slice. A
# universal build needs a universal SDL2, either SDL2.framework from libsdl.org's
# own macOS release or a source build with
# -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64". This script does not choose, it
# checks, because a single-arch SDL2 fails on a machine that is not this one.
#
# Deliberately not done here:
#   - Notarisation. Signing is ad-hoc (`codesign -s -`), enough to run locally
#     but not enough for a download: Gatekeeper refuses an unnotarised app, and
#     the player needs the right-click Open path this prints at the end. Real
#     notarisation needs a paid Developer ID and `xcrun notarytool`.
#   - Moving saves out of the bundle. main.c anchors its working directory to
#     the executable, which inside an .app is Contents/MacOS/, so first-run data
#     lands there rather than under ~/Library/Application Support. Moving it is
#     a code change with save migration attached, not a packaging one.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="${OUT:-$REPO/build/OldAmber-macos}"
APP="$OUT/OldAmber.app"
GAME_DIR="${GAME_DIR:-$REPO/build-macos}"
VERSION="${VERSION:-0.0.1}"

say() { printf '[macos] %s\n' "$*"; }
die() { printf '[macos] ERROR: %s\n' "$*" >&2; exit 1; }

[ "$(uname -s)" = "Darwin" ] || die "this packages a macOS build and must run on macOS (uname -s = $(uname -s))"

# The binary, and the properties it has to have.
GAME_EXE="$GAME_DIR/oldamber"
[ -f "$GAME_EXE" ] || die "no oldamber in $GAME_DIR -- build it first:
    cmake -S . -B ${GAME_DIR##*/} -G Ninja \\
          -DRED_ONLY=ON -DCMAKE_BUILD_TYPE=Release \\
          -DAMBER_DEBUG_PRINTS=OFF -DAMBER_EMBED_PYTHON=OFF \\
          -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' \\
          -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15
    cmake --build ${GAME_DIR##*/} --target pokered"

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
     "The bracketed [E4DBG]/[PALDBG] traces would print to the player's console."
want AMBER_EMBED_PYTHON OFF \
     "Embedding links a specific libpython; the binary then refuses to start on
    a Mac carrying a different one. Same reason the Linux bundle does this."

# Both slices, checked. A single-arch build is invisible on the machine that
# produced it.
archs="$(lipo -archs "$GAME_EXE" 2>/dev/null || true)"
case "$archs" in
    *arm64*) ;; *) die "no arm64 slice in $GAME_EXE (lipo says: ${archs:-nothing})" ;;
esac
case "$archs" in
    *x86_64*) say "binary is universal: $archs" ;;
    *)
        # ALLOW_SINGLE_ARCH EXISTS FOR CI, AND FOR NOTHING ELSE.
        #
        # The macOS runner has brew's SDL2, which is arm64-only, so a universal
        # build cannot link there, but everything AFTER this point (bundle
        # layout, Info.plist, dylib rebasing, the freeze, the stray check,
        # signing) is arch-independent and is exactly what wants exercising on
        # every push. Refusing outright would mean this script is only ever run
        # by hand, which is how packaging scripts rot.
        #
        # A RELEASE must not use it: 0.0.1 ships Intel, and an arm64-only
        # bundle does not fail on the machine that built it, it fails on
        # somebody else's Intel Mac.
        [ "${ALLOW_SINGLE_ARCH:-0}" = "1" ] || die "no x86_64 slice in $GAME_EXE (lipo says: $archs)
    0.0.1 ships Intel too. Reconfigure with
        -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64'
    and make sure SDL2 is universal -- brew's is host-arch only; see the note
    at the top of this file."
        say "WARNING: single-arch ($archs) -- ALLOW_SINGLE_ARCH is set."
        say "WARNING: this bundle is NOT shippable; it will not run on Intel."
        ;;
esac

# Assemble the bundle.
say "assembling $APP"
rm -rf "$OUT"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" "$APP/Contents/Frameworks"

cp "$GAME_EXE" "$APP/Contents/MacOS/OldAmber"
chmod +x "$APP/Contents/MacOS/OldAmber"

# Data sits beside the executable, not in Resources/, because main.c anchors the
# working directory to Contents/MacOS/ and every path below it is relative.
BASE="$APP/Contents/MacOS"
cp -R "$REPO/shaders" "$BASE/"
cp "$REPO/LICENSE" "$REPO/THIRD_PARTY.md" "$BASE/" 2>/dev/null || true
mkdir -p "$BASE/mod_runtime"
for d in blocks scenes config map_edits; do
    cp -R "$REPO/mod_runtime/$d" "$BASE/mod_runtime/"
done
cp "$REPO/mod_runtime/pks_flag_registry.txt" "$BASE/mod_runtime/"

# Development scaffolding, see the same removal in make_release.sh: no scene
# binds a Test*.block, and they name `subtile` art under mod_runtime/custom_art
# which no bundle carries, so each one costs the player a "PNG not found" line
# on first boot and gives nothing back.
rm -f "$BASE/mod_runtime/blocks"/Test*.block

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>              <string>OldAmber</string>
    <key>CFBundleDisplayName</key>       <string>OldAmber</string>
    <key>CFBundleExecutable</key>        <string>OldAmber</string>
    <key>CFBundleIdentifier</key>        <string>com.spiritsnails.oldamber</string>
    <key>CFBundlePackageType</key>       <string>APPL</string>
    <key>CFBundleShortVersionString</key><string>$VERSION</string>
    <key>CFBundleVersion</key>           <string>$VERSION</string>
    <key>LSMinimumSystemVersion</key>    <string>10.15</string>
    <key>LSApplicationCategoryType</key> <string>public.app-category.games</string>
    <!-- Without this macOS magnifies the whole window on a Retina panel and it
         reads as blurry. The OTHER half is SDL_WINDOW_ALLOW_HIGHDPI on the
         window itself, which is NOT set yet: launcher_nav's win_to_logical
         maps mouse coordinates assuming window points and output pixels are
         the same, and that stops being true the moment the flag goes on. Both
         have to land together. -->
    <key>NSHighResolutionCapable</key>   <true/>
</dict>
</plist>
PLIST

if [ -f "$REPO/oldambericon.icns" ]; then
    cp "$REPO/oldambericon.icns" "$APP/Contents/Resources/OldAmber.icns"
    /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string OldAmber" \
        "$APP/Contents/Info.plist" >/dev/null 2>&1 || true
else
    say "no oldambericon.icns -- the app will use the generic icon"
fi

# SDL2 travels with the app. otool says what the binary actually asks for, so
# nothing is guessed. Two things that makes tricky:
#
#   otool -L on a universal binary prints one header per architecture, not one
#   for the file, and a header line like "...OldAmber (architecture arm64):"
#   would be read as a dependency whose path exists. Header lines end in ':',
#   which is what to filter on.
#
#   A dependency may already be @rpath, and skipping it ships nothing. CMake
#   gives libSDL2 an @rpath install name, so passing it over leaves the app
#   resolving it through the build rpath, an absolute path on the packaging
#   machine. It then launches there and nowhere else.
#
# So: resolve @rpath deps against the binary's own LC_RPATH list, bundle them,
# and then strip every rpath that is not inside the app.
say "bundling shared libraries"
install_name_tool -add_rpath "@executable_path/../Frameworks" \
    "$BASE/OldAmber" 2>/dev/null || true

# The rpath list as the linker left it, used to find @rpath deps on disk.
BUILD_RPATHS="$(otool -l "$BASE/OldAmber" | awk '/LC_RPATH/{f=1} f&&/ path /{print $2; f=0}' | sort -u)"

otool -L "$BASE/OldAmber" \
    | grep -v ':$' \
    | awk '{print $1}' \
    | sort -u \
    | while read -r lib; do
    case "$lib" in
        ''|/usr/lib/*|/System/*|@executable_path/*|@loader_path/*) continue ;;
    esac
    base="$(basename "$lib")"
    src=""
    case "$lib" in
        @rpath/*)
            for rp in $BUILD_RPATHS; do
                case "$rp" in @*) continue ;; esac      # only real directories
                [ -f "$rp/$base" ] && { src="$rp/$base"; break; }
            done
            ;;
        *) [ -f "$lib" ] && src="$lib" ;;
    esac
    if [ -z "$src" ]; then
        die "cannot find $lib on disk to bundle it.
    Searched the binary's own rpaths: ${BUILD_RPATHS:-<none>}
    Shipping without it would produce an app that launches here and nowhere
    else."
    fi
    cp -f "$src" "$APP/Contents/Frameworks/$base"
    chmod u+w "$APP/Contents/Frameworks/$base"
    # Already @rpath/NAME needs no -change; an absolute path does.
    case "$lib" in
        @rpath/*) ;;
        *) install_name_tool -change "$lib" "@rpath/$base" "$BASE/OldAmber" ;;
    esac
    install_name_tool -id "@rpath/$base" "$APP/Contents/Frameworks/$base" 2>/dev/null || true
    say "  bundled $base"
done

# Every rpath that is not the bundle's own has to go. Leaving the build one in
# means the app prefers a library from a directory only this machine has, so it
# keeps working here while being broken everywhere else.
for rp in $BUILD_RPATHS; do
    [ "$rp" = "@executable_path/../Frameworks" ] && continue
    install_name_tool -delete_rpath "$rp" "$BASE/OldAmber" 2>/dev/null \
        && say "  dropped build rpath $rp"
done

# The frozen setup. Identical rules to the Linux bundle: every importer .py and
# the symbol table go inside the frozen binary, computed rather than listed,
# because a hardcoded --add-data list drifts silently and dies on the player's
# machine partway through their first import. scan_extractor_sources.py answers
# the question.
say "freezing setup (embeds the symbol table)"
command -v pyinstaller >/dev/null 2>&1 || python3 -m PyInstaller --version >/dev/null 2>&1 \
    || die "PyInstaller not available -- pip3 install pyinstaller"

ADD_DATA=()
for _sym in "$REPO"/pokered-master/*.sym; do
    [ -f "$_sym" ] && ADD_DATA+=( --add-data "$_sym:pokered-master" )
done
[ ${#ADD_DATA[@]} -gt 0 ] || die "no pokered-master/*.sym, the frozen tool would have
    nothing to embed and the player's first import could not start."
while IFS= read -r rel; do
    [ -n "$rel" ] || continue
    [ -f "$REPO/$rel" ] || die "scan_extractor_sources names $rel, which is missing"
    ADD_DATA+=( --add-data "$REPO/$rel:$(dirname "$rel")" )
done < <(python3 "$REPO/tools/dist/scan_extractor_sources.py" "$REPO" | tr -d '\r')
say "embedding $(( ${#ADD_DATA[@]} / 2 )) file(s) into setup"

python3 -m PyInstaller --noconfirm --clean --onefile \
    --name setup \
    --distpath "$REPO/build/dist_tmp_macos" \
    --workpath "$REPO/build/pyi_work_macos" \
    --specpath "$REPO/build/pyi_work_macos" \
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
cp "$REPO/build/dist_tmp_macos/setup" "$BASE/setup"
chmod +x "$BASE/setup"

# Refuse to ship something broken, or something ROM-derived.
say "checking nothing ROM-derived got in"
strays="$(find "$OUT" \( -name '*.pak' -o -name custom_art -o -name generatedmaps \
                        -o -iname '*.gb' -o -iname '*.gbc' -o -name generated \
                        -o -name '*.sav' -o -name 'pokered-master' \
                        -o -name '*.sym' \) -print)"
[ -z "$strays" ] || { printf '%s\n' "$strays" >&2; die "content that must never ship is in the bundle"; }

missing=0
for f in Contents/MacOS/OldAmber Contents/MacOS/setup Contents/Info.plist \
         Contents/MacOS/shaders/MasterShader.fsh; do
    [ -e "$APP/$f" ] || { echo "MISSING FROM BUNDLE: $f" >&2; missing=1; }
done
[ "$missing" -eq 0 ] || die "refusing to call this bundle ready"

# Ask the binary what it still needs. Any absolute path outside the app is
# host-only, whatever directory it names, and the app would die at launch with a
# dyld message and no log.
outside="$(otool -L "$BASE/OldAmber" | grep -v ':$' | awk '{print $1}' \
            | grep -E '^/' | grep -vE '^(/usr/lib|/System)/' || true)"
[ -z "$outside" ] || { printf '%s\n' "$outside" >&2
    die "the binary still references libraries from this machine only"; }

# And every @rpath dependency must actually be present in the bundle.
for dep in $(otool -L "$BASE/OldAmber" | grep -v ':$' | awk '{print $1}' \
             | grep '^@rpath/' | sort -u); do
    [ -f "$APP/Contents/Frameworks/$(basename "$dep")" ] \
        || die "$dep is not in Contents/Frameworks -- the app would not launch
    anywhere but this machine."
done

# Nothing may point outside the bundle for libraries either.
badrp="$(otool -l "$BASE/OldAmber" | awk '/LC_RPATH/{f=1} f&&/ path /{print $2; f=0}' \
          | sort -u | grep -v '^@executable_path/\.\./Frameworks$' || true)"
[ -z "$badrp" ] || { printf '%s\n' "$badrp" >&2
    die "an rpath still points outside the app bundle"; }
say "every library is inside the bundle, and no rpath leaves it"

# Sign. Ad-hoc: enough to run, not enough to distribute without the quarantine
# step. Deep, and the executable last, because a nested signature invalidates its
# container's.
say "signing (ad-hoc)"
codesign --force --deep --sign - "$APP" >/dev/null 2>&1 \
    || say "WARNING: codesign failed -- the app may not launch on this machine"
codesign --verify --deep --strict "$APP" >/dev/null 2>&1 \
    && say "signature verifies" || say "WARNING: signature does not verify"

say "ready: $APP  ($(du -sh "$OUT" | cut -f1))"

# ---- the archive that actually gets uploaded --------------------------------
# Packaging used to stop at a folder, so every upload was archived by hand and
# nothing afterwards could say what a given download contained.
#
# ditto, not zip. A .app is a directory carrying symlinks, executable bits and a
# code signature, and a plain zip drops enough of that to produce a bundle that
# will not launch or whose signature no longer verifies. ditto -c -k --keepParent
# is the archiver Apple's own tooling uses, and it preserves all three.
ARCHIVE="$OUT/../OldAmber-$VERSION-macos-universal.zip"
say "archiving"
rm -f "$ARCHIVE" "$ARCHIVE.sha256"
if ditto -c -k --keepParent "$APP" "$ARCHIVE"; then
    say "archive: $ARCHIVE  ($(du -h "$ARCHIVE" | cut -f1))"
    # Prove the round trip rather than assume it: unpack to a temp directory and
    # re-verify the signature. A bundle that arrives unlaunchable is the one
    # failure a player cannot work around.
    _t="$(mktemp -d)"
    if ditto -x -k "$ARCHIVE" "$_t" 2>/dev/null &&
       codesign --verify --deep --strict "$_t/$(basename "$APP")" >/dev/null 2>&1; then
        say "archive round trip verifies: unpacked signature is intact"
    else
        say "WARNING: the unpacked archive does not verify. Do not publish it."
    fi
    rm -rf "$_t"
    shasum -a 256 "$ARCHIVE" | tee "$ARCHIVE.sha256"
else
    say "WARNING: could not create $ARCHIVE. Upload the app by hand."
fi
# Quoted delimiter. Unquoted, the shell would command-substitute the backticked
# `xattr -dr com.apple.quarantine` below and print its usage error into the
# middle of this advice. Nothing in this block needs expanding.
cat <<'NOTE'

[macos] FIRST RUN, FOR THE README -- ALL MOUSE, NO TERMINAL:
    This build is ad-hoc signed, not notarised, so macOS refuses to open it
    from a download the first time. The player does this ONCE:

        Right-click (or Control-click) OldAmber.app  ->  Open  ->  Open

    If macOS offers no Open button, it is the newer wording:

        System Settings -> Privacy & Security -> scroll down -> Open Anyway

    Both are GUI. Do NOT put `xattr -dr com.apple.quarantine` in front of a
    player -- it works, and it is a terminal command aimed at someone who
    wanted to double-click a game. Keep it for a troubleshooting footnote.

    Notarising removes the step entirely and needs a paid Developer ID tied to
    a legal identity, which is a deliberate no for this project.
NOTE
