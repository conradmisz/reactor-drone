---
id: 009
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

Upstream SDL bugs in the installed prerelease. First verified against SDL
`origin/main` in a scratch worktree; then the system SDL was updated
(2026-08-11, `sudo cmake --install`) and re-tested:

- Symptom 1 (mid-run segfault): **gone** — 2000-frame GPU+bloom soak clean.
- Symptom 2 (readback segfault): **gone** — `--gpu-renderer --screenshot`
  during bloomed gameplay captures correctly; the guard forcing classic now
  applies only to DEFAULT runs (explicit `--gpu-renderer` may capture).
- Symptom 3 (teardown wedge): **partially fixed.** The render state and frame
  target destroy cleanly, but `SDL_ReleaseGPUShader` (and `SDL_WaitForGPUIdle`
  before it) still hang while the renderer — destroyed after us — holds
  pipelines built from the shader. The destructor now destroys everything
  except the one shader object, which is deliberately leaked to process exit
  (~4KB; comment in `postfx_system.cpp`). Revisit only if PostFx ever gains
  shader hot-swapping.

GPU renderer remains opt-in until a windowed playtest signs off on the path
(that is now a product call, not a stability one).

**Signed off 2026-08-13.** A windowed `--gpu-renderer` session ran 9407 frames
and exited `EXIT=0` — symptom 3 did not reproduce, so the deliberate shader
leak holds in practice. The default was flipped to the GPU renderer in the
same pass (D212, v3 Tier 6a); `--classic-renderer` is now the escape hatch.
The leak itself is unchanged and still deliberate.
