---
id: 005
title: Menu/title screen runs at ~52fps on a 60Hz display
status: open
severity: low
area: perf
opened: 2026-08-13
---

## Symptom

"The menu is kind of laggy." Title screen delivers ~52fps against displays
running at 59.96Hz / 59.91Hz — roughly one refresh in seven is missed.

## Measurements

Steady state (400 frames, 3 reps each, init subtracted):

    branch, vsync ON    51.9 / 52.1 / 51.8 fps
    branch, vsync OFF   51.8 / 51.9 / 52.0 fps
    master              ~52.6 fps

## Ruled Out

- **vsync** — the original hypothesis (Tier 0's `SDL_SetRenderVSync` layered
  on the Timer's busy-wait = two pacers) is WRONG. A/B via an env probe shows
  no measurable difference. A `Timer::set_external_pacing` fix was written,
  measured, found to change nothing, and reverted unshipped.
- **Not a v3 regression** — master is ~52.6fps, the branch ~52.0. An earlier
  reading of 49.6 vs 52.6 was noise, not a regression; see below.
- The GPU renderer (`--classic-renderer` identical).
- Bloom (`bloom.enabled=false` identical).
- The Timer's target rate (`--fps 120` and `--fps 240` identical).

## Measurement hazard (read before re-testing)

For roughly 20 minutes this machine produced 8-20x slower readings for the
BRANCH only, while master stayed fast: 100 frames took 46s where it now takes
2.3s. The committed HEAD reproduced it with no local changes, so it was not
code. It cleared on its own and never recurred. Cause unknown — suspect
compositor state (cosmic-comp).

Any single timing run here is untrustworthy. Take 3+ reps and sanity-check
master in the same batch before concluding anything.

## Not Yet Tried

- Where the ~8ms/frame actually goes: no profiler has been run. Everything
  above is elimination, not measurement of the cost itself.
- Whether gameplay (as opposed to the title screen) shows the same shortfall.
