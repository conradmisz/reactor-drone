#!/bin/bash
# engine-suite gate: build warning-free, 8/8 ctest, replay canary identical twice
# AND identical to a stored baseline.
#
#   bash gate.sh .canary-baseline.txt
#
# .canary-baseline.txt holds the FIRING canary's summary line as documented in
# CLAUDE.md for pre-suite master (779455e), so a green run proves the suite is
# inert by default rather than merely self-consistent. Complements
# scripts/verify_branch.sh (the branch gate); this one only answers "did the
# suite move the sim".
#
# Known noise: Game_Property_Tests fails ~1 run in 20 on a pre-existing master
# flake (agentProjectDocs/bugs/010-path-property-test-flake.md). Re-run before
# believing a red ctest line.
set -o pipefail
cd "$(dirname "$0")"
OUT=$(mktemp -d)
# D230 (playtest #4 verification): the gate used to exit 0 no matter what it
# printed — "DIFFERS from baseline" was advisory. FAIL now propagates, so a
# scripted caller can trust the exit code. (Trap 8, bugs/003.)
FAIL=0
echo "=== build ==="
cmake --build CPP/build -j"$(nproc)" > "$OUT/build.log" 2>&1 || { tail -30 "$OUT/build.log"; exit 1; }
W=$(grep "warning:" "$OUT/build.log" | grep -v tmpnam | wc -l)
echo "warnings (ours): $W"
[ "$W" -eq 0 ] || { grep "warning:" "$OUT/build.log" | grep -v tmpnam | head; FAIL=1; }
echo "=== ctest ==="
ctest --test-dir CPP/build > "$OUT/ctest.log" 2>&1 || FAIL=1
grep -E "tests passed|tests failed|Failed" "$OUT/ctest.log" || true
echo "=== canary x2 ==="
# The FIRING canary, not the idle one. A single `--keys 5:SPACE` only presses
# start (SPACE is both the title key and the fire key), so the run ends
# score 0 / units 0 and never reaches hit-stop — it passes without exercising
# the one path that could break (CLAUDE.md, bugs/006). Saves are reset first
# for the same reason: meta.json is read at boot and steers the presses.
for i in 1 2; do
  rm -f saves/settings.json
  printf '{"best_wave":5,"lifetime_score":1305,"prestige":0,"runs_played":4}\n' > saves/meta.json
  SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 \
    --keys $(seq -f '%g:SPACE' 10 4 2990) --stopframe 3000 2>/dev/null | tail -1 > "$OUT/c$i.txt"
done
cat "$OUT/c1.txt"
if diff -q "$OUT/c1.txt" "$OUT/c2.txt" >/dev/null; then echo "canary: IDENTICAL across runs"; else echo "canary: DIVERGED"; diff "$OUT/c1.txt" "$OUT/c2.txt"; FAIL=1; fi
if [ -n "$1" ] && [ -f "$1" ]; then
  if diff -q "$OUT/c1.txt" "$1" >/dev/null; then echo "canary: matches baseline $1"; else echo "canary: DIFFERS from baseline"; diff "$1" "$OUT/c1.txt"; FAIL=1; fi
fi
exit $FAIL
