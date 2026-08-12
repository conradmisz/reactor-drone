#!/usr/bin/env bash
# Stage a portable Linux build: exe + bundled SDL3 libs + assets, launched via
# run.sh (LD_LIBRARY_PATH beats patchelf: no extra tool, same effect).
# Build first: cmake -B CPP/build-portable -S CPP -DRD_PORTABLE=ON -DBUILD_TESTING=OFF
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="$ROOT/installer/stage-linux"
BIN="$ROOT/CPP/build-portable/game/game"
[ -x "$BIN" ] || { echo "build first: cmake -B CPP/build-portable -S CPP -DRD_PORTABLE=ON"; exit 1; }
rm -rf "$STAGE" && mkdir -p "$STAGE/lib"
cp "$BIN" "$STAGE/ReactorDrone"
# Bundle the SDL3 stack (not on any distro's default install) and its direct
# non-system deps. libcurl and the rest of the OpenSSL/glibc world stay system:
# bundling TLS means shipping its CA story too, and every distro has libcurl.
for lib in libSDL3.so.0 libSDL3_image.so.0 libSDL3_ttf.so.0; do
    src=$(ldd "$BIN" | awk -v l="$lib" '$1 == l {print $3}')
    [ -n "$src" ] || { echo "missing $lib in ldd output"; exit 1; }
    cp -L "$src" "$STAGE/lib/"
done
cp -r "$ROOT/assets" "$STAGE/assets"
cp "$ROOT/PRIVACY.md" "$STAGE/"
cat > "$STAGE/run.sh" <<'EOF'
#!/bin/sh
HERE="$(cd "$(dirname "$0")" && pwd)"
LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" exec "$HERE/ReactorDrone" "$@"
EOF
chmod +x "$STAGE/run.sh" "$STAGE/ReactorDrone"
echo "staged: $(du -sh "$STAGE" | cut -f1) at $STAGE"
