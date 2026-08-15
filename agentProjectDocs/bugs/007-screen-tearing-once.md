---
id: 007
title: Screen tearing observed once during playtest
status: open
severity: low
area: render
opened: 2026-08-14
---

## Symptom

One visible tear during the 2026-08-13/14 windowed playtest of `visual-overhaul`.
Seen once, not reproduced, no capture.

## Known before investigating

- **VSync is already on.** `main.cpp:180` calls `SDL_SetRenderVSync(renderer, 1)`
  (v3 Tier 0). So this is not a missing-vsync bug.
- The same playtest toggled **fullscreen** (v3 Tier 8) — `saves/settings.json`
  came back `"fullscreen": true`. Swapchain recreation across a fullscreen
  transition is the leading suspect and should be tested FIRST.
- This machine's compositor (cosmic-comp) already produced ~20 minutes of
  phantom 8-20x timing results for bugs/005. Treat single observations here as
  untrustworthy.

## Decision

**Filed, not chased** (user's call, 2026-08-14). One unreproduced tear on a
known-flaky compositor does not justify a hunt. Reopen with a repro: note
whether the game was fullscreen or windowed, and whether it had just switched.

## Ruled Out

- Missing vsync (see above).
