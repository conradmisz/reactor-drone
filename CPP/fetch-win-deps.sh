#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p win-deps && cd win-deps
SDL_VER=3.4.14; IMG_VER=3.4.4; TTF_VER=3.2.2   # pin: newest release-* tag per repo as of 2026-08-11
fetch() { # $1 repo, $2 name, $3 ver
  [ -d "$2-$3" ] && return 0
  curl -fLO "https://github.com/libsdl-org/$1/releases/download/release-$3/$2-devel-$3-mingw.tar.gz"
  tar xf "$2-devel-$3-mingw.tar.gz"
}
fetch SDL SDL3 "$SDL_VER"
fetch SDL_image SDL3_image "$IMG_VER"
fetch SDL_ttf SDL3_ttf "$TTF_VER"
# Flatten the x86_64 sysroots so one CMAKE_PREFIX_PATH covers all three.
for d in SDL3-$SDL_VER SDL3_image-$IMG_VER SDL3_ttf-$TTF_VER; do
  cp -rn "$d/x86_64-w64-mingw32/"* . 2>/dev/null || true
done
echo "win-deps ready"
