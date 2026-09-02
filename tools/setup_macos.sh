#!/usr/bin/env bash
# setup_macos.sh: one shot: universal SDL2, then a universal game build.
#
#     bash tools/setup_macos.sh
#     bash tools/dist/make_release_macos.sh      # then package it
#
# The release ships Intel as well as Apple Silicon, so the binary has to be
# universal, and a universal binary needs a universal SDL2. Homebrew installs
# for the host arch only, so it cannot supply one.
#
# Building SDL2 once, universal, into deps/ avoids both a second Intel Homebrew
# under Rosetta and two builds on two machines. It mirrors what the repo already
# does for MSVC in deps/SDL2-VC.
#
# Everything here is one-time. Re-running reuses what is already built.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDL_VER="${SDL_VER:-2.32.10}"          # matches deps/SDL2-VC, so the two agree
DEPS="$REPO/deps/SDL2-macOS"
PREFIX="$DEPS/install"
BUILD_DIR="${BUILD_DIR:-build-macos}"
ARCHS="${ARCHS:-arm64;x86_64}"
DEPLOY="${DEPLOY:-10.15}"

say() { printf '[macos-setup] %s\n' "$*"; }
die() { printf '[macos-setup] ERROR: %s\n' "$*" >&2; exit 1; }

[ "$(uname -s)" = "Darwin" ] || die "this is the macOS setup and must run on macOS (uname -s = $(uname -s))"
command -v cmake >/dev/null 2>&1 || die "cmake not found -- brew install cmake"
command -v python3 >/dev/null 2>&1 || die "python3 not found -- brew install python@3.11"

# Universal SDL2
if [ -f "$PREFIX/lib/libSDL2.dylib" ] || [ -f "$PREFIX/lib/libSDL2-2.0.0.dylib" ]; then
    say "universal SDL2 already built at $PREFIX"
else
    mkdir -p "$DEPS"
    TAR="$DEPS/SDL2-$SDL_VER.tar.gz"
    SRC="$DEPS/SDL2-$SDL_VER"
    if [ ! -d "$SRC" ]; then
        if [ ! -f "$TAR" ]; then
            URL="https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL2-$SDL_VER.tar.gz"
            say "fetching SDL2 $SDL_VER"
            curl -fL --retry 3 -o "$TAR" "$URL" || die "could not download $URL"
        fi
        # The hash is printed, not asserted. Record this value by hand and turn
        # it into a check; an unverified checksum would look like a
        # supply-chain guarantee while asserting nothing.
        say "sha256: $(shasum -a 256 "$TAR" | cut -d' ' -f1)"
        tar -xzf "$TAR" -C "$DEPS"
    fi
    say "building SDL2 universal ($ARCHS) -- one time, a few minutes"
    cmake -S "$SRC" -B "$DEPS/build" \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
          -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOY" \
          -DCMAKE_INSTALL_PREFIX="$PREFIX" \
          -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF >/dev/null
    cmake --build "$DEPS/build" --config Release --parallel >/dev/null
    cmake --install "$DEPS/build" >/dev/null
fi

LIB="$(ls "$PREFIX"/lib/libSDL2-2.0.0.dylib "$PREFIX"/lib/libSDL2.dylib 2>/dev/null | head -1)"
[ -n "$LIB" ] || die "SDL2 built but no dylib landed in $PREFIX/lib"
sdl_archs="$(lipo -archs "$LIB")"
case "$sdl_archs" in
    *arm64*x86_64*|*x86_64*arm64*) say "SDL2 is universal: $sdl_archs" ;;
    *) die "SDL2 came out single-arch ($sdl_archs) -- nothing built against it can be universal" ;;
esac

# The game. The same cache values the release bundle asserts, so a build made by
# this script is one make_release_macos.sh accepts.
say "configuring $BUILD_DIR"
cmake -S "$REPO" -B "$REPO/$BUILD_DIR" \
      -DRED_ONLY=ON -DCMAKE_BUILD_TYPE=Release \
      -DAMBER_DEBUG_PRINTS=OFF -DAMBER_EMBED_PYTHON=OFF \
      -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOY" \
      -DCMAKE_PREFIX_PATH="$PREFIX" >/dev/null

say "building the game"
cmake --build "$REPO/$BUILD_DIR" --target pokered oldamber_bootstrap --parallel

EXE="$REPO/$BUILD_DIR/oldamber"
[ -f "$EXE" ] || die "build finished but no $EXE"
game_archs="$(lipo -archs "$EXE")"
case "$game_archs" in
    *arm64*x86_64*|*x86_64*arm64*) say "game is universal: $game_archs" ;;
    *) die "game came out single-arch ($game_archs)" ;;
esac

say "done. package it with:  bash tools/dist/make_release_macos.sh"
