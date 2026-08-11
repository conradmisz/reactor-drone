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

# curl for the game's async HTTP client (Task 6). Pin checked against
# https://curl.se/windows/ as of 2026-08-11.
CURL_VER=8.21.0_6
if [ ! -d "curl-mingw" ]; then
  curl -fLO "https://curl.se/windows/dl-$CURL_VER/curl-$CURL_VER-win64-mingw.zip"
  unzip -q "curl-$CURL_VER-win64-mingw.zip" -d curl-tmp
  mv curl-tmp/curl-* curl-mingw && rm -r curl-tmp
  cp -rn curl-mingw/include/* include/ && cp -rn curl-mingw/lib/* lib/ && cp -n curl-mingw/bin/*.dll bin/
fi

echo "win-deps ready"
