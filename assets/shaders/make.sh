#!/bin/sh
# Offline shader build (v3 Tier 4, D197) — run BY HAND, never from CMake.
# Output .spv files are committed; the game loads only the .spv.
# Works with either compiler:
#   glslc   (Ubuntu/Pop: sudo apt install glslc)
#   glslang (standalone release from github.com/KhronosGroup/glslang)
set -e
cd "$(dirname "$0")"
if command -v glslc >/dev/null 2>&1; then
    glslc -fshader-stage=frag postfx.frag.glsl -o postfx.frag.spv
elif command -v glslang >/dev/null 2>&1; then
    glslang -V -S frag postfx.frag.glsl -o postfx.frag.spv
else
    echo "need glslc or glslang on PATH" >&2; exit 1
fi
echo "wrote postfx.frag.spv ($(stat -c%s postfx.frag.spv) bytes)"
