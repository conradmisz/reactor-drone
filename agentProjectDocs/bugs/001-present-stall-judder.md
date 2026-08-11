---
id: 001
title: The game visibly hitches ~4 times a second — one frame in twelve takes ~57ms
status: investigating
severity: high
area: engine
opened: 2026-08-10
resolved:
---

## Symptom

Playing the game feels choppy and laggy — the user's words: "it feels like the
world is moving frame by frame". Not a uniform low framerate: eleven frames are
delivered perfectly on budget and the twelfth freezes.

Measured over 1200 gameplay frames, no vsync, Release build:

    hist  <17ms: 1099    17-50ms: 1    50-67ms: 100

There is nothing between 17ms and 50ms. Frames are either exactly on budget
(16.7ms) or ~57ms. Wall clock for 1800 frames is 36.2s — a nominal 60 FPS run
delivers 49.7 FPS.

The stall is inside `SDL_RenderPresent`. Game work either side of it is 0.4ms:

    [stall] frame=12  total=50.0ms present=49.6ms rest=0.4ms
    [stall] frame=24  total=56.3ms present=55.0ms rest=1.3ms
    [stall] frame=36  total=57.1ms present=56.7ms rest=0.4ms
    ... every 12th frame, from frame 12 to the end of the run

## Reproduce

Happens every time, from the title screen onward — it does not need gameplay.

1. `cmake -B CPP/build -S CPP && cmake --build CPP/build --target game`
2. `./CPP/build/game/game --seed 42 --keys 5:SPACE --stopframe 1800`
3. Time it. Expected 30.0s at 60 FPS; actual 36.2s.

Minimal reproduction **without any project code** — 40 lines of SDL3 that
create a window, clear it, present, and sleep to a 60 FPS budget:
`scratchpad/repro.c` (see Ruled Out). It stalls identically: every 12th frame,
`SDL_RenderPresent` blocks 56-57ms.

## Environment

- Commit 4607ebd, branch `feature/engine-suite`
- Pop!_OS, kernel 6.18.7-76061807-generic
- **COSMIC desktop (`cosmic-comp`), Wayland session**
- **NVIDIA proprietary driver**, GPU `10de:2f04`
- Displays: DP-3 1920x1080@59.96, DP-2 2560x1440@59.91
- SDL3, default backends: `video=x11` (XWayland), `render=opengl`
- Reproduces on Debug (no `CMAKE_BUILD_TYPE`) and Release (`-O3`) alike

## Ruled Out

**Append-only. Never delete a line from this section.**

- **Tested:** Built with `-DCMAKE_BUILD_TYPE=Release` (`-O3`) and timed the same
  1800-frame run against the default no-optimization build.
  **Observed:** 36.26s vs 36.21s — identical.
  **Eliminates:** the missing `CMAKE_BUILD_TYPE` as the cause. (It is still worth
  setting a default, but it is not this bug.)

- **Tested:** Disabled the frame pacer entirely and ran uncapped.
  **Observed:** 0.68ms per frame — 1470 FPS, 1800 frames in 1.39s.
  **Eliminates:** CPU cost, GPU cost, and game-logic cost. The game has ~24x the
  headroom it needs for 60 FPS.

- **Tested:** Instrumented `Timer::end_frame_internal` — sleep request vs actual,
  and `sleep_error_` per frame.
  **Observed:** requested 7.84ms, actually slept 7.91ms, overshoot 0.09ms.
  Frame-time p50 and p90 are both exactly 16.7ms.
  **Eliminates:** the sleep/busy-wait pacer in `engine/timer.cpp`. It hits its
  target on 91% of frames and its error is ~0.1ms, not 40ms.

- **Tested:** `SDL_SetRenderVSync(renderer, 1)`, with the software pacer left on
  and again with it disabled.
  **Observed:** worse — 22.7ms and 23.3ms average, and the tail moves from ~57ms
  to ~121ms (155 frames over 100ms in 1800).
  **Eliminates:** "just turn vsync on" as the fix. Vsync makes it worse here.

- **Tested:** All SDL render backends over 400 frames: default, `opengl`,
  `opengles2`, `vulkan`, `software`.
  **Observed:** 33, 33, 35, 32, 33 stalls respectively.
  **Eliminates:** the GPU driver and the render backend. The software rasterizer
  stalls exactly as much as OpenGL, so this is not a GPU path at all.

- **Tested:** Both video drivers, `SDL_VIDEODRIVER=x11` (XWayland) and
  `SDL_VIDEODRIVER=wayland` (native).
  **Observed:** 33/400 and 40/400 stalls. Native Wayland is slightly worse.
  **Eliminates:** XWayland specifically. Both paths stall.

- **Tested:** NVIDIA driver env knobs `__GL_SYNC_TO_VBLANK=0` and
  `__GL_MaxFramesAllowed=1`, on both video drivers.
  **Observed:** 33/400 stalls in every combination — no change whatsoever.
  **Eliminates:** NVIDIA's swap-interval and frame-queue settings as a workaround.

- **Tested:** A 40-line SDL3 program with none of this project's code — create
  window, `SDL_RenderClear`, `SDL_RenderPresent`, sleep to a 60 FPS budget.
  **Observed:** 33 stalls in 400 frames, every 12th frame, present blocking
  56-57ms. Identical signature to the game.
  **Eliminates:** every line of Reactor Drone. The bug is in the SDL3 /
  compositor / driver stack, not in this codebase.

- **Tested:** Swept the pacing target in the minimal repro: 30, 45, 59, 60, 61,
  75, 120 FPS, 600 frames each.
  **Observed:** 30 FPS → **1 stall in 600**. 45 → 60, 59 → 50, 60 → 50, 61 → 46,
  75 → 40, 120 → 28. Normalising by elapsed time, that is a near-constant
  ~4.3 stalls per second at every rate from 45 to 120 FPS.
  **Eliminates:** a count-based cause (a swapchain N frames deep would stall every
  N frames regardless of rate). The stall is **time-periodic at ~230ms**, and it
  disappears entirely at 30 FPS.

- **Tested:** Fullscreen window (`SDL_WINDOW_FULLSCREEN`), on both video drivers
  and with `__GL_SYNC_TO_VBLANK=0`, to try for a direct-scanout compositor bypass.
  **Observed:** 33, 40, 33 stalls in 400 — identical to windowed.
  **Eliminates:** fullscreen/compositor-bypass as a workaround.

- **Tested:** The same repro loop with `SDL_RenderPresent` removed entirely,
  timing the present slot, the pacing slot, and everything else separately.
  **Observed:** with present, 33 stalls, all 33 inside present. Without present,
  **zero stalls anywhere in the frame.**
  **Eliminates:** the OS scheduler, CPU frequency/idle states, and the pacing
  sleep. The client is being blocked *inside* present, by the compositor.

- **Tested:** Real game at `--fps 30` — `--seed 42 --keys 5:SPACE --fps 30
  --stopframe 900`.
  **Observed:** 900 frames in 30.27s. Dead on 30 FPS, 0.27s of slack across the
  whole run, no hitching.
  **Eliminates:** nothing, but confirms suspect 4: **a 30 FPS cap is a working
  mitigation available today.** Repro at 30 FPS: 4 stalls in 600. At 40 FPS: 66.
  At 50: 55. The cliff is between 30 and 40 FPS.

## Suspects

0. **Confounder not yet ruled out:** every measurement was taken with the game
   window *unfocused* (the terminal had focus). COSMIC may throttle unfocused
   surfaces. The user reports the hitching while actually playing, so it is not
   only a focus artifact — but the numbers here may overstate it. Test: re-run
   the repro with the window focused and in front.

1. **`cosmic-comp` periodically blocks client buffer release** (~230ms period) —
   test: run the minimal repro under a different compositor on the same machine
   (GNOME/mutter or KWin Wayland session, or a bare X11 session) and count stalls.
   This is the single test that would settle it, and it is the obvious next step.

2. **Something in the session polls at ~4Hz and stalls the compositor** — e.g.
   `cosmic-panel`, `cosmic-osd`, or a settings daemon. Test: count stalls with
   the panel/OSD processes stopped.

3. **The 59.96 / 59.91 Hz two-monitor mismatch forces a compositor resync** —
   test: unplug or disable DP-2 and re-run the repro on a single display.

4. **A 30 FPS cap is an acceptable shipping workaround** — test: run the real
   game at `--fps 30` and confirm the hitching is gone and it feels smoother
   than the hitchy 60. Cheap, and worth knowing even if a real fix lands.

## Resolution

Not resolved. Nothing in this repository has been changed for this bug: the
minimal-repro result means there is nothing here to fix. Do not close this by
editing `engine/timer.cpp` — the pacer was measured and is accurate to 0.1ms.
