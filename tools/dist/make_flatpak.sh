#!/usr/bin/env bash
# make_flatpak.sh: build the Flatpak from the tested Linux tarball.
#
# Why a Flatpak at all. The plain tarball links against the host's SDL2 and
# needs glibc 2.38 or newer, so it runs on distributions close to the one that
# built it and fails elsewhere with a version error that tells a player nothing.
# A Flatpak pins the runtime, so the same build runs on an old Debian and a
# rolling Arch alike, and it is the native install path on a Steam Deck.
#
# It packages the tarball that make_release_linux.sh already produced and
# tested, so what ships here is the same payload, not a second build.
#
#   bash tools/dist/make_release_linux.sh      # first, produces the tarball
#   bash tools/dist/make_flatpak.sh
#
# Needs flatpak and flatpak-builder, and the freedesktop runtime, which the
# script offers to install.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HERE="$REPO/tools/dist/flatpak"
APPID="com.spiritsnails.OldAmber"
RUNTIME_VER="24.08"
VERSION="${VERSION:-0.0.2}"
TARBALL="$REPO/build/OldAmber-$VERSION-linux-x64.tar.gz"
BUILDDIR="$REPO/build/flatpak-build"
REPODIR="$REPO/build/flatpak-repo"
BUNDLE="$REPO/build/OldAmber-$VERSION-linux-x86_64.flatpak"

say() { printf '[flatpak] %s\n' "$*"; }
die() { printf '[flatpak] ERROR: %s\n' "$*" >&2; exit 1; }

command -v flatpak >/dev/null 2>&1 || die "flatpak is not installed"
command -v flatpak-builder >/dev/null 2>&1 || die "flatpak-builder is not installed"
[ -f "$TARBALL" ] || die "no $TARBALL
    Run tools/dist/make_release_linux.sh first: this packages that output
    rather than building a second time."

# The runtime and SDK, from Flathub. Installed per user so this needs no root.
#
# Both are checked, not just the Platform. An interrupted install can leave the
# Platform present and the Sdk missing, and testing only the first then skips
# straight to a build that fails with "Unable to find sdk".
need=""
flatpak info "org.freedesktop.Platform//$RUNTIME_VER" >/dev/null 2>&1 || \
    need="$need org.freedesktop.Platform//$RUNTIME_VER"
flatpak info "org.freedesktop.Sdk//$RUNTIME_VER" >/dev/null 2>&1 || \
    need="$need org.freedesktop.Sdk//$RUNTIME_VER"
if [ -n "$need" ]; then
    say "installing:$need"
    say "this is a large download the first time"
    flatpak remote-add --if-not-exists --user flathub \
        https://dl.flathub.org/repo/flathub.flatpakrepo
    # shellcheck disable=SC2086
    flatpak install --user -y flathub $need
fi

# Icons, from the one 16x16 source. Every size wanted is a power-of-two
# multiple of 16, so nearest neighbour reproduces the pixel art exactly rather
# than blurring it.
say "generating icons"
python3 - "$REPO/oldambericon.png" "$HERE" <<'PY'
import sys
from PIL import Image
src, out = sys.argv[1], sys.argv[2]
im = Image.open(src).convert("RGBA")
if im.size != (16, 16):
    raise SystemExit("expected a 16x16 source icon, got %s" % (im.size,))
for px in (128, 256):
    im.resize((px, px), Image.NEAREST).save("%s/icon_%d.png" % (out, px))
print("wrote icon_128.png and icon_256.png")
PY

say "building"
rm -rf "$BUILDDIR" "$REPODIR"
flatpak-builder --user --force-clean --repo="$REPODIR" \
    "$BUILDDIR" "$HERE/$APPID.yml"

say "bundling"
rm -f "$BUNDLE"
flatpak build-bundle "$REPODIR" "$BUNDLE" "$APPID"

say "ready: $BUNDLE  ($(du -h "$BUNDLE" | cut -f1))"
if command -v sha256sum >/dev/null 2>&1; then
    (cd "$(dirname "$BUNDLE")" &&
     sha256sum "$(basename "$BUNDLE")" | tee "$(basename "$BUNDLE").sha256")
fi

cat <<NOTE

[flatpak] To install and run it:

    flatpak install --user $BUNDLE
    flatpak run $APPID

[flatpak] On a Steam Deck this is the native route: install it in Desktop Mode,
    then add it to Steam so it appears in Game Mode.
NOTE
