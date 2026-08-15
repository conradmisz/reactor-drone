#!/usr/bin/env bash
# Build the SDL3 stack from source into CPP/linux-deps for the Linux release
# job — SDL3 is not in ubuntu-24.04's apt. Same version pins as fetch-win-deps.sh
# (release-* source tags match the -devel mingw tarballs). libcurl comes from
# apt (libcurl4-openssl-dev): bundling TLS means shipping a CA story too.
set -euo pipefail
cd "$(dirname "$0")"
PREFIX="$PWD/linux-deps"
mkdir -p linux-deps-src && cd linux-deps-src
SDL_VER=3.4.14; IMG_VER=3.4.4; TTF_VER=3.2.2   # keep in lockstep with fetch-win-deps.sh
# Every cmake step is redirected to keep CI output readable, which also hides
# the error when one fails. Dump the tail before dying so a CI break is
# diagnosable in one cycle instead of one blind re-run per guess.
run() { local log="$1"; shift; "$@" >> "$log" 2>&1 || { echo "FAILED: $*"; tail -40 "$log"; exit 1; }; }
build() { # $1 repo, $2 name, $3 ver
  [ -f "$PREFIX/lib/lib$2.so" ] && return 0
  [ -d "$2-$3" ] || { curl -fL "https://github.com/libsdl-org/$1/releases/download/release-$3/$2-$3.tar.gz" | tar xz; }
  # SDLTTF_VENDORED=OFF: the release tarball ships no harfbuzz sources, so
  # SDL_ttf links the system freetype/harfbuzz (apt: libfreetype-dev
  # libharfbuzz-dev). Every distro ships both — same linkage as the dev build.
  : > "$2-build.log"
  run "$2-build.log" cmake -S "$2-$3" -B "$2-$3/build" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_PREFIX_PATH="$PREFIX" \
        -DBUILD_SHARED_LIBS=ON -DSDLTTF_VENDORED=OFF
  run "$2-build.log" cmake --build "$2-$3/build" -j"$(nproc)"
  run "$2-build.log" cmake --install "$2-$3/build"
}
build SDL SDL3 "$SDL_VER"
build SDL_image SDL3_image "$IMG_VER"
build SDL_ttf SDL3_ttf "$TTF_VER"
echo "linux-deps ready at $PREFIX"
