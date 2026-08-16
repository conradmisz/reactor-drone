---
id: 006
title: The replay canary is not seed-deterministic — it reads saves/
status: resolved
severity: high
area: process
opened: 2026-08-13
resolved: 2026-08-13
---

## Symptom

The firing canary reported a hard divergence from master after the v3 Tier 7
laser fix:

    master:  Frames: 3000  Final score: 100  Units: 24  Wave: 1  Phase: 1
    branch:  Frames: 3000  Final score:  85  Units:  7  Wave: 1  Phase: 1

`Units` is `final_credits`, so both figures are real gameplay outcomes. It
looked like the change had broken the simulation.

## Cause

Not the code. The canary reads persistent state at boot and is therefore NOT
self-contained despite `--seed`:

- `saves/meta.json` — best_wave / lifetime_score / runs_played decide what the
  title menu offers.
- `saves/settings.json` — present or absent, and what it holds.

SPACE is BOTH the title-screen key and the fire key. A different menu means
the scripted `--keys` presses land on different widgets, the run starts
differently, and every downstream number diverges.

Between the last good canary and the failing one, this worktree's saves drifted:

    meta.json      best_wave 5,  runs_played 4   ->  best_wave 14, runs_played 20
    settings.json  absent                        ->  {"fullscreen": true, ...}

caused by a windowed playtest (waves 9-14) and fullscreen testing run in the
same worktree the canary reads from.

## Ruled Out

- The laser fix itself: bisected by restoring the `Color` component on shots
  while keeping every other change — still 85/7. The code was never implicated.
- Resetting `saves/` to the baseline state reproduced 100/24 exactly, on the
  unchanged build.

## Resolution

`CLAUDE.md` now requires clearing `saves/` before any canary run, with the
exact reset commands and the reason. A canary result from a worktree with a
played-in `saves/` is explicitly not evidence.

## Lesson

Every tier from 6a to 8 passed this canary honestly, but only because nobody
had played in the worktree yet. The gate had a silent precondition that was
true by luck. Verification that depends on ambient state is not verification.

---

## Follow-up (2026-08-16): the gate itself runs the WEAK canary — still open

Two scripts run the replay canary, and both used `--keys 5:SPACE`:

- `gate.sh` (arrived with the engine-suite merge) — **fixed this session** to
  the firing form, and it now resets `saves/` between the two runs. Its
  `.canary-baseline.txt` was also holding the idle summary line
  (`score 0 / units 0 / phase 2`) and now holds the firing one
  (`score 100 / units 24 / wave 1 / phase 1`).
- `scripts/verify_branch.sh` **section 3 — STILL THE IDLE FORM, NOT FIXED.**

SPACE is both the title-screen start key and the fire key, so a single
`--keys 5:SPACE` presses start and nothing else: the run ends `score 0 /
units 0`, never reaches hit-stop, and passes without exercising the one path
that could break. The branch gate has therefore reported **30/30 across the
whole v2.x line while never once firing a shot.**

Left unfixed deliberately — verify_branch is the shipping gate and changing it
is a release-risk call, not a wrap-up edit. It is the highest-value cleanup
before the gameplay.md rewrite, because that rewrite touches firing directly.
The replacement is the form CLAUDE.md already documents:

    SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 \
      --keys $(seq -f '%g:SPACE' 10 4 2990) --stopframe 3000

Note sections 6 and 7 (portable Linux, wine smoke) also use `5:SPACE`, but
those only assert "reaches frame N", so the weak form is adequate there.
