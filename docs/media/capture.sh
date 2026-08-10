#!/bin/bash
# Capture gameplay media for docs/features.html.
#
# Runs the game headless (SDL dummy driver renders fine) with scripted input:
#   - SPACE injected every frame = continuous fire + periodic dash
#   - --hover rotates the aim point around the spawn so fire sweeps the arena
#   - --screenshot dumps BMP frames into logs/<timestamp>/
#
# For capture sessions, temporarily buff the player in assets/GameData.json
# (weapon damage/fire_rate/spread, start_health, currency values) so scripted
# runs actually progress through waves — then `git checkout assets/GameData.json`
# to restore. The buff values used for the shipped media are recorded in
# docs/features-page-design.md.
#
# Usage:
#   capture.sh STOPFRAME "SHOT_FRAMES_PYEXPR"   # generic probe run
#   capture.sh scenario                          # the full scripted session used
#                                                # for the shipped page media
# Timeline of `scenario` (seed 42, capture buffs on): waves 1-5 clear by ~7500,
# Core->Foundry crossfade ~4700-4950, WAVE CLEARED prompt from ~7600 (it waits
# for a click), OPEN SHOP at 8000, buys, TAB to gear, B closes at 8600, then a
# kitted run toward the wave-10 boss.
set -e
cd "$(dirname "$0")/../.."
GAME=CPP/build/game/game
ARGS=$(python3 - "$@" <<'EOF'
import sys, math
def hover(stop):
    return ["--hover"] + [
        f"{f}:{int(1600 + 420*math.cos(f/80.0*6.283))},{int(1600 + 420*math.sin(f/80.0*6.283))}"
        for f in range(20, stop, 5)]
if sys.argv[1] == "scenario":
    stop = 30000
    keys = ["5:SPACE"]
    keys += [f"{f}:SPACE" for f in range(20, 7500)]        # fight waves 1-5
    buys = [(8100,"2"),(8140,"2"),(8180,"1"),(8220,"3"),(8260,"4"),
            (8300,"5"),(8340,"7"),(8380,"8")]              # shield first: field ring
    keys += [f"{f}:{k}" for f, k in buys]
    keys += ["8450:TAB", "8520:1", "8560:5", "8600:B"]     # gear page, buy, close
    keys += [f"{f}:SPACE" for f in range(8650, stop)]      # kitted, toward the boss
    shots  = list(range(2000, 2150))                       # hero clip
    shots += list(range(4700, 4950))                       # crossfade clip
    shots += [7600, 7610, 7620]                            # WAVE CLEARED prompt
    shots += [8030, 8060, 8090, 8420, 8480, 8560]          # shop pages
    shots += list(range(8700, 12000, 50))                  # kitted action
    shots += list(range(12000, stop, 100))                 # boss hunt
    out = ["--seed", "42", "--stopframe", str(stop),
           "--clicks", "8000:377,369",                     # OPEN SHOP
           "--keys"] + keys + hover(stop) + ["--screenshot"] + [str(f) for f in shots]
else:
    stop = int(sys.argv[1])
    out = ["--seed", "42", "--stopframe", str(stop), "--keys", "5:SPACE"]
    out += [f"{f}:SPACE" for f in range(20, stop)]
    out += hover(stop) + ["--screenshot"] + [str(f) for f in eval(sys.argv[2])]
print(" ".join(out))
EOF
)
SDL_VIDEODRIVER=dummy $GAME $ARGS 2>&1 | grep -v Screenshot | tail -3
