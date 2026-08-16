#!/bin/bash
# engine-suite gate: build warning-free, 8/8 ctest, replay canary identical twice
# AND identical to a stored baseline.
#
#   bash gate.sh .canary-baseline.txt
#
# .canary-baseline.txt holds the summary line from pre-suite master (779455e), so
# a green run proves the suite is inert by default rather than merely
# self-consistent. Lives on this branch only — it is a merge-decision tool, not
# part of the game.
#
# Known noise: Game_Property_Tests fails ~1 run in 20 on a pre-existing master
# flake (agentProjectDocs/bugs/003-path-property-test-flake.md). Re-run before
# believing a red ctest line.
set -o pipefail
cd "$(dirname "$0")"
OUT=$(mktemp -d)
echo "=== build ==="
cmake --build CPP/build -j"$(nproc)" > "$OUT/build.log" 2>&1 || { tail -30 "$OUT/build.log"; exit 1; }
W=$(grep "warning:" "$OUT/build.log" | grep -v tmpnam | wc -l)
echo "warnings (ours): $W"
[ "$W" -eq 0 ] || grep "warning:" "$OUT/build.log" | grep -v tmpnam | head
echo "=== ctest ==="
ctest --test-dir CPP/build 2>&1 | grep -E "tests passed|tests failed|Failed" || true
echo "=== canary x2 ==="
for i in 1 2; do
  SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 --keys 5:SPACE --stopframe 3000 2>/dev/null | tail -1 > "$OUT/c$i.txt"
done
cat "$OUT/c1.txt"
if diff -q "$OUT/c1.txt" "$OUT/c2.txt" >/dev/null; then echo "canary: IDENTICAL across runs"; else echo "canary: DIVERGED"; diff "$OUT/c1.txt" "$OUT/c2.txt"; fi
if [ -n "$1" ] && [ -f "$1" ]; then
  if diff -q "$OUT/c1.txt" "$1" >/dev/null; then echo "canary: matches baseline $1"; else echo "canary: DIFFERS from baseline"; diff "$1" "$OUT/c1.txt"; fi
fi
