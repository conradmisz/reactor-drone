# Feature Spec: v3 Neon Projectiles + Display

## Status

Approved (2026-08-13) — branch `visual-overhaul`, tiers 6–8.

## User Story

As a player, I want shots to read as neon laser ribbons with real motion
trails, the GPU post-fx actually on by default, and a fullscreen option, so
the game delivers the Laser Hockey look it was aiming at.

## Requirements

1. **Canary fix (do first).** The documented canary `--keys 10:SPACE` presses
   SPACE once — it starts the run and nothing else, so it ends 0 score / 0
   units and never reaches hit-stop (`main.cpp:2432`, raised only at `:615`
   kill / `:1679` boss). Replace with a firing canary in `CLAUDE.md` and
   `plans/v3-neon-polish-plan.md`. Verified form: SPACE every 4th frame from
   10 to 2990, seed 42, `--stopframe 3000` → `score 100, units 24, wave 1`.
2. **Tier 6 GPU.** (a) `--gpu-renderer` becomes default, `--classic-renderer`
   the escape hatch (the plan's original intent; shipped inverted). Auto
   fallback on GPU init failure stays; dummy/headless still forces classic.
   (b) Retune existing uniforms via GameData only. (c) **DEFERRED 2026-08-13
   at the user's call** — per-arena LUT grade (Tier 4 item 3) + dash radial
   blur. The 6a flip plus the 6b retune were judged to deliver the GPU feel
   without a new 32^3 LUT generator and shader pass. Not cancelled: it stays
   the natural next tier if the grade still reads flat.
3. **Tier 7 trails.** Position-history ribbons on player projectiles, enemy
   projectiles, player drone, dash arc.
4. **Tier 8 fullscreen.** Third `SettingsSave` field + third options-screen
   row beside `screen_shake` / `minimap`, applied via
   `SDL_SetWindowFullscreen`. Defaults off.

## Acceptance Criteria

1. Firing canary summary is **byte-identical between master and the branch**
   after every tier. This is the presentation-only proof; the inert canary is
   not acceptable evidence.
2. Two runs of the firing canary on the branch are byte-identical
   (determinism).
3. `runTestsAll.py` 8/8; zero `warning:` from project code (Lua `tmpnam`
   exempt).
4. Ribbon vertex budget per frame is capped in GameData; tail segments drop
   first. `--fps` measured before/after tier 7 and recorded.
5. A sprite/entity with no `Trail` renders exactly as today.
6. Fullscreen off by default → headless runs and the canary unaffected.
7. `ENGINE.md` updated in the same commit as each engine change.

## Out of Scope

- Merging any of this to `master`.
- Trails on pickups, explosions, or enemies themselves.
- Runtime shader compilation — `.spv` stays offline-built and committed.
- Fixing the `bugs/003` shader leak (still deliberate, still ~4KB to exit).

## Affected Boundaries

- `engine/ecs/systems/line_mesh_math.hpp` — per-point widths + alpha ramp
  from the `u` it already computes. Pure, already unit-tested.
- `engine/ecs/systems/trail_math.hpp` — NEW, pure: ring buffer with
  `push_sample(pos, min_spacing)` (spacing guard stops duplicate samples
  while stationary or hit-stopped) + `to_ribbon()`.
- `Trail` component — render-only observer. Never read by a gameplay system,
  never touches RNG or `delta_time`. This invariant is what keeps AC-1 true.
- `game/main.cpp` — one more pass in the existing `glow_lines` build loop
  (~2349-2404, already doing ring / obstacles / beams). `render_glow_lines`
  itself is unchanged.
- `game/settings_save.hpp`, options screen, `ui-context.md`.
- `assets/shaders/postfx.frag.glsl` + `.spv`.

## Task Breakdown

1. Canary fix in both docs; re-verify master vs branch.
2. Tier 6a default flip (+ `bugs/003` note).
3. Tier 6b GameData retune.
4. ~~Tier 6c LUT grade + dash radial blur~~ — DEFERRED, see req 2(c).
5. Tier 7a `trail_math.hpp` + unit tests (no rendering yet).
6. Tier 7b `Trail` component, sampling, emission, vertex cap.
7. Tier 8 fullscreen setting + menu row.

## Open Questions

- None blocking. Decided during design: `glow_lines` assembly stays in
  `main.cpp` for consistency with the existing three passes rather than being
  extracted now.

## Merge Notes

- **Decision:** GPU renderer default flipped (D199) — the plan always
  specified it; the opt-in was a stability hold that `bugs/003` + the
  2026-08-13 windowed playtest resolved. → `decisions.md`
- **Decision:** documented replay canary was inert and could not reach
  hit-stop; replaced with a firing canary. → `decisions.md`
- **Decision:** trail history lives in a render-side map in `main.cpp`, not an
  ECS component (D200). Keeping it out of `component_storage` means no
  gameplay system *can* read it, so presentation-only is enforced by
  construction rather than by convention. → `decisions.md`
- **Decision:** Tier 6c (per-arena LUT grade + dash radial blur) deferred
  after 6a/6b were judged sufficient. → `decisions.md`
- **State:** tiers 6a, 6b, 7a, 7b, 8 done on `visual-overhaul`; 6c deferred.
  Branch is unplayed since 7b — the trails have never been seen.
  → `progress-tracker.md`
