#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="$ROOT/installer/stage"
EXE="$(find "$ROOT/CPP/build-win" -name ReactorDrone.exe | head -1)"
[ -n "$EXE" ] || { echo "build first (Task 1)"; exit 1; }
rm -rf "$STAGE" && mkdir -p "$STAGE"
cp "$EXE" "$STAGE/"
# Only CPP/win-deps/bin/*.dll — fetch-win-deps.sh flattens the x86_64 sysroots
# there deliberately. Globbing '*/bin/*' across all of win-deps also matches
# each dep's untouched i686-w64-mingw32/bin/, so a same-named 32-bit DLL can
# land in the stage dir and overwrite the correct x86_64 one (order from find
# is filesystem-dependent, not sorted) — breaking the x86-64 exe at load time.
find "$ROOT/CPP/win-deps/bin" -maxdepth 1 -name '*.dll' -exec cp {} "$STAGE/" \;
cp -r "$ROOT/assets" "$STAGE/assets"
echo "staged: $(du -sh "$STAGE" | cut -f1)"
