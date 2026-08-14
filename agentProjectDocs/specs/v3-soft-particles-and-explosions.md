# Feature Spec: v3 Soft Particles + Layered Explosions

## Status

Approved (2026-08-14) — branch `visual-overhaul`, tiers 9-11.

## User Story

As a player, I want the glow to look like light instead of boxes, shots to
read as fast neon tracers, and a kill to feel like a real explosion, so the
arena looks polished rather than like lit rectangles.

## Requirements

1. **Tier 9 batched particles.** Additive particles draw as soft round discs
   through ONE `SDL_RenderGeometry` call per frame, not as per-entity SDL fill
   rects. Quad/UV construction lives in a pure, SDL-free `particle_mesh.hpp`.
   `render_walk` no longer draws additive-tinted `Color` entities.
2. **Tier 9b bloom restore.** With hard edges gone, `bloom.intensities` returns
   to the Tier 6b values (.55/.47/.39/.31, `default_intensity` 0.45) that were
   pulled back only to mask the squares (bugs/004 mitigation, now superseded).
3. **Tier 10 tracer.** Player and enemy shots read as long streaks: history
   length ~8x ribbon width, sharper tail taper, hotter/narrower white core.
   Tuning of the existing Tier 7 trail path — no new renderer.
4. **Tier 11 layered explosion.** Enemy death plays four staged layers: white
   flash (t=0), expanding shockwave RING (t~0.05), debris shards (t~0.10),
   ember particles fading to t=0.45. Ring and shards are `GlowLine`s through
   the existing neon line renderer; embers are Tier 9 particles.

## Acceptance Criteria

1. Given any additive emitter on screen, when a frame is drawn, then no particle
   has a hard rectangular edge and the bloom halo around it is round.
2. Given the title screen at the measured baseline, when `--fps` is sampled 3+
   reps before and after Tier 9, then the median frame time does NOT regress.
   **This is the gate that killed the previous attempt (27x, bugs/004) — a
   regression here stops the tier.**
3. Given `--seed 42` and the CLAUDE.md canary (saves reset, keys firing), when
   run on this branch and on `master`, then both print an identical summary
   after every tier. Presentation-only work must not move the sim.
4. Given an enemy dies, when the death frame is captured, then the flash, the
   ring and the shards are each visible in their stage of the timeline.
5. Given a degenerate particle (zero size, or a zero-length shard), when the
   mesh is built, then it emits no NaN vertices and does not divide by zero.

## Out of Scope

- Screen tearing seen once during the 2026-08-13 playtest → filed as bugs/007,
  not chased. VSync is already enabled (`main.cpp:180`).
- bugs/005 (menu ~52fps) — pre-existing on master, untouched here.
- Tier 6c (per-arena LUT grade + dash radial blur) stays deferred.

## Affected Boundaries

- `CPP/engine/ecs/systems/render_system.{hpp,cpp}` — new `render_particles`,
  `render_walk` skip.
- `CPP/engine/ecs/systems/particle_mesh.hpp` — NEW, pure, unit-tested.
- `CPP/game/enemy_death_system.cpp` — the four-layer timeline.
- `CPP/game/main.cpp` — the new pass, sited with the Tier 5/7 glow-line passes.
- `assets/data/GameData.json` — bloom intensities (Tier 9b), trail tuning (10).

## Task Breakdown

1. Tier 9a: `particle_mesh.hpp` + unit tests (no rendering yet).
2. Tier 9b: `render_particles` batch call; `render_walk` skip; `--fps` gate.
3. Tier 9c: bloom intensities restored; bugs/004 closed as fixed.
4. Tier 10: tracer tuning, judged in a windowed playtest.
5. Tier 11: layered explosion timeline.
6. bugs/007 filed.

## Open Questions

- None blocking.

## State

- **Tier 9 (9a/9b/9c) DONE** on `visual-overhaul`, 2026-08-14. bugs/004 closed as
  fixed. Gates: build clean, ctest 8/8, canary identical twice AND unchanged from
  the pre-Tier-9 baseline (`Frames: 3000  Final score: 100  Units: 24  Wave: 1
  Phase: 1`), timing median 141.1s -> 139.4s (no regression, 3 reps each).
- **Tiers 10 + 11 DONE**, 2026-08-14. Gates re-run after both: build clean,
  ctest 8/8, canary identical and still matching the pre-Tier-9 baseline, timing
  median 139.5s (flat).
- Tier 11 turned up the real cause of "the explosion is very simple": the death
  system was loading the CLASS-ORIGINAL atlas (a grey sphere growing into a
  rounded square), not the v2 art. Repointed at `v2/effect_explosion.json`, and
  the v2 clip itself was re-authored as a brief flash (12 frames @0.04) now that
  the ring and shards are drawn live.
- **Not playtested.** All Tier 9 verification is headless captures read back;
  nobody has judged the soft discs in a real window. `DISC_SCALE` (2.5) is a
  by-eye constant and is the first thing to tune when someone does.
- One unexplained `Game_Property_Tests` failure during a full ctest run
  immediately after a screenshot capture. Not reproduced in 14 subsequent runs
  (2 full ctest + 12 direct); no failure output was retained. Watch for it.

## Merge Notes

- **D202:** additive particles are batched into one `SDL_RenderGeometry` mesh
  UV'd to `glow_disc_64.png`, NOT given a per-entity texture. The texture route
  was measured at ~27x (bugs/004) because `draw_entity` makes six batch-flushing
  SDL state calls per particle; a batch makes six per FRAME.
- **D203:** the explosion shockwave ring and debris shards are `GlowLine`s, not
  new sprites or a new renderer — the Tier 5 line renderer already draws exactly
  this shape.
- **D204:** the tracer is tuning, not new machinery — `taper_widths` gained an
  `exponent` (shots use 2.0) and `GlowLine` a `core_scale` (shots 0.26), plus
  GameData trail length 14x3.0 -> 20x3.6 (~9.8x the 7.0 shot width) with the
  vertex budget raised 4000 -> 6000 to pay for it.
- **D206:** `EnemyDeathSystem` loads `images/v2/effect_explosion.json`. It had
  loaded the class-original placeholder since before v2; see ENGINE.md section 5.
  The blast layers also set `core = false` — a white-lifted core blooms into a
  flat grey ball that fills the ring.
- **D205:** `UIRenderSystem::render` sets `SDL_BLENDMODE_BLEND` itself instead of
  inheriting it. Found by Tier 9: the UI's alpha fills had been relying on
  additive particles setting the renderer-wide draw blend mode each frame, so
  removing particles from the render walk made every alpha-0 fill paint solid
  black. Fixed at the UI's single entry point rather than per fill site — one
  guard where all of them route.
- **NUMBERING HAZARD:** this branch's `decisions.md` stops at D196 and this
  branch's other spec holds D199-D201 unhoisted; `experimental` and
  `distribution` are reported to also claim numbers in this range. Reconcile at
  merge — do not assume D202-D204 are free on `master`.
