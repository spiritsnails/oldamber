#!/usr/bin/env bash
# make_appimage.sh: one file a Linux player downloads, marks executable, runs.
#
# The plain tarball links 51 shared libraries and ships none of them, so it only
# starts on a machine that already has SDL2, Wayland, xcb, PulseAudio and the
# rest. The Flatpak solves that by pinning a runtime, at the cost of a 3 GB
# download before the 8 MB app will start. An AppImage carries its own
# libraries in one file.
#
#   bash tools/dist/make_release_linux.sh   # first, produces the tarball
#   bash tools/dist/make_appimage.sh
#
# GLIBC IS THE ONE THING IT CANNOT CARRY. The C library is bound to the kernel
# and the loader, so it comes from the host and the binary keeps whatever floor
# the build machine set. Building on an older distribution lowers that floor;
# nothing done here can.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VERSION="${VERSION:-0.0.2}"
TARBALL="$REPO/build/OldAmber-$VERSION-linux-x64.tar.gz"
WORK="$REPO/build/appimage"
APPDIR="$WORK/OldAmber.AppDir"
OUT="$REPO/build/OldAmber-$VERSION-x86_64.AppImage"
TOOL="$WORK/appimagetool"

say() { printf '[appimage] %s\n' "$*"; }
die() { printf '[appimage] ERROR: %s\n' "$*" >&2; exit 1; }

[ -f "$TARBALL" ] || die "no $TARBALL
    Run tools/dist/make_release_linux.sh first."

rm -rf "$WORK"; mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib"

say "unpacking the release payload"
tmp="$WORK/payload"; mkdir -p "$tmp"
tar -xzf "$TARBALL" -C "$tmp"
PAY="$tmp/OldAmber-linux"
[ -d "$PAY" ] || die "unexpected tarball layout"
cp -r "$PAY"/. "$APPDIR/usr/bin/"
rm -f "$APPDIR/usr/bin/OldAmber.sh"     # AppRun replaces the shim

# ---- libraries -------------------------------------------------------------
# Everything the binary and SDL pull in, except the pieces that have to come
# from the host. Bundling those breaks more than it fixes: the C library is tied
# to the kernel and loader, and the graphics libraries are tied to the installed
# driver, so a copied one refuses to talk to the real GPU.
EXCLUDE='^(ld-linux|libc|libm|libdl|libpthread|librt|libresolv|libnsl|libutil|libBrokenLocale|libanl|libthread_db|libGL|libGLX|libGLdispatch|libEGL|libOpenGL|libGLESv|libdrm|libgbm|libglapi|libX11|libxcb|libXext|libXau|libXdmcp|libxshmfence|libgcc_s|libstdc\+\+)'

say "bundling libraries"
n=0
while read -r name arrow path rest; do
    [ "$arrow" = "=>" ] || continue
    [ -f "$path" ] || continue
    echo "$name" | grep -qE "$EXCLUDE" && continue
    cp -Ln "$path" "$APPDIR/usr/lib/" 2>/dev/null || true
    n=$((n + 1))
done < <(ldd "$APPDIR/usr/bin/OldAmber" 2>/dev/null)
say "bundled $n library(ies)"

# SDL loads its video, audio and input backends by dlopen at runtime, so ldd
# never names them and they would be missing on a host without SDL installed.
for extra in libdecor-0.so.0 libwayland-client.so.0 libwayland-cursor.so.0 \
             libwayland-egl.so.1 libxkbcommon.so.0 libpulse.so.0 libasound.so.2; do
    for d in /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib; do
        [ -f "$d/$extra" ] && cp -Ln "$d/$extra" "$APPDIR/usr/lib/" 2>/dev/null && break
    done
done

# ---- the pieces AppImage requires at the AppDir root ------------------------
cp "$REPO/oldambericon.png" "$APPDIR/com.spiritsnails.OldAmber.png"
cp "$REPO/oldambericon.png" "$APPDIR/.DirIcon"

cat > "$APPDIR/com.spiritsnails.OldAmber.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=OldAmber
Comment=The Poke RBY Generation 1 engine, rebuilt. Supply your own ROM.
Exec=OldAmber
Icon=com.spiritsnails.OldAmber
Terminal=false
Categories=Game;ActionGame;
DESKTOP

cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/sh
# Point the loader at the bundled libraries before anything else runs, and start
# the game from its own directory so it finds shaders and mod_runtime beside it.
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:${LD_LIBRARY_PATH}"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-}"
cd "$HERE/usr/bin"
exec ./OldAmber "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

# ---- build -----------------------------------------------------------------
if ! [ -x "$TOOL" ]; then
    say "fetching appimagetool"
    curl -fsSL -o "$TOOL" \
      https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage \
      || die "could not download appimagetool"
    chmod +x "$TOOL"
fi

say "building"
rm -f "$OUT"
# extract-and-run because FUSE is often unavailable, in a container or WSL.
ARCH=x86_64 "$TOOL" --appimage-extract-and-run "$APPDIR" "$OUT" >/dev/null 2>&1 \
    || ARCH=x86_64 "$TOOL" "$APPDIR" "$OUT"

[ -f "$OUT" ] || die "appimagetool produced nothing"
chmod +x "$OUT"
say "ready: $OUT  ($(du -h "$OUT" | cut -f1))"
if command -v sha256sum >/dev/null 2>&1; then
    (cd "$(dirname "$OUT")" &&
     sha256sum "$(basename "$OUT")" | tee "$(basename "$OUT").sha256")
fi

say "glibc floor of this build: $(objdump -T "$APPDIR/usr/bin/OldAmber" \
      | grep -o 'GLIBC_[0-9.]*' | sort -V -u | tail -1)"
say "that is set by the build machine and travels with the binary."
