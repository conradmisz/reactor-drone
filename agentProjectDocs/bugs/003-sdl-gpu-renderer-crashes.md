---
id: 003
title: GPU renderer segfaults/hangs with bloom on the installed SDL prerelease
status: resolved
severity: high
area: build
opened: 2026-08-11
resolved: 2026-08-11
---

## Symptom

With the SDL GPU renderer (`SDL_CreateRendererWithProperties` name="gpu") on
the system SDL (3.5.0-prerelease built ~2026-05-10, /usr/local):

1. Long bloomed gameplay runs segfault mid-run (`--keys 5:SPACE --stopframe
   2000`, crash after ~wave 1; 900 frames clean, 2000 not).
2. `--screenshot` during a bloomed gameplay frame segfaults in the pixel
   readback (title-screen screenshots are fine).
3. Any run that ever CREATED a `SDL_GPURenderState` + `SDL_GPUShader` hangs at
   shutdown inside `SDL_WaitForGPUIdle` (also without it, inside the destroy) —
   even if the objects were never used in a single draw.

## Ruled Out

- PostFx code: symptom 1 reproduces with `postfx.enabled=false` (bloom only).
- Bloom code on the classic renderer: 2000-frame runs + screenshots clean.
- GPU renderer without bloom: 2000-frame runs + screenshots clean.
- Our teardown ordering: bisected — leaking state+shader at exit gives clean
  shutdown with the full pipeline live (symptom 3 is inside SDL, not us).
- The shader itself: symptom 3 reproduces with the state bound zero times.

## Resolution

Upstream SDL bugs in the installed prerelease. Verified by building SDL
`origin/main` (2026-08-11) in a scratch worktree and running with
`LD_LIBRARY_PATH`: the 2000-frame GPU+bloom run survives (slow under an
occluded window, but no crash).

Shipped mitigations (D197):

- GPU renderer is **opt-in** (`--gpu-renderer`); default is the classic
  renderer, which carries the complete Tier 0-3 look.
- `--screenshot` always forces classic (it is the verification baseline).
- `PostFxSystem`'s destructor deliberately leaks the render state + shader
  (documented in-code); the process is exiting anyway.

To use the GPU path day-to-day: update the system SDL
(`cd ~/SDL && git pull && cmake -B build && cmake --build build -j && sudo
cmake --install build`), then re-test symptoms 1-3 and consider flipping the
default in `main.cpp`.
