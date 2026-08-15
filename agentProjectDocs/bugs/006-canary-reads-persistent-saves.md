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
