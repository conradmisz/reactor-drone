# Feature Spec: Arena transition VFX

## Status

Done (unplayed — verified by unit tests and a forced-shift headless run, not a
windowed playtest).

## User Story

As a player, I want the old arena's scenery to visibly come apart while the new
one assembles, so that an arena shift reads as an event rather than as a frame
where the pillars blinked out.

## Requirements

1. On the frame a shift starts, **every outgoing prop loses its `Collider`**.
   Nothing about the simulation may still see scenery that is only being drawn.
2. The outgoing props then shrink to nothing across the existing 5s crossfade
   window, trailing debris, and are all gone by the end of it.
3. The incoming props (spawned at `SHIFT_PROP_SWAP`, 2.0s in) scale up on a
   stagger over the remaining 3s. Their colliders scale with them, so a
   half-grown pillar is never an invisible wall.
4. The stagger/scale curve is pure and testable without a window, and it *is*
   the crossfade's smoothstep — one curve, not two.
5. Determinism is untouched: no new RNG in the game loop, all randomness is the
   already-seeded `ParticleSystem`.
6. A full-arena destruction must be measured against `DEFAULT_MAX_PARTICLES`
   (2000, already truncating at wave 20). The cap is not raised here.

## Acceptance Criteria

1. Given a live arena with 28 solid props, when `teardown_props` runs, then
   `entities_with_component<Collider>()` is empty **on that call**, while every
   prop still has a non-zero `Size`.
2. Given a teardown at t=0, when the window reaches t=1, then every prop is at
   scale 0 and `destroy_all` leaves no `Size`, `Collider` or `ParticleEmitter`
   behind.
3. Given a set of incoming props at t=0.3, then each prop's `Collider` width
   equals its `Size` width and both are below the authored value; at t=1 both
   equal the authored value exactly, about the authored centre.
4. Given `count <= 1` or `stagger_span >= 1`, then `staggered_t` still returns a
   finite clamped 0..1 (no divide-by-zero).
5. **Edge case:** given a prop whose entity was already destroyed by its
   `Lifetime`, when the animation ticks, then it is skipped rather than
   dereferenced.
6. Given the worst arena (28 solid props), when the 5s window is simulated
   against the real `ParticleSystem`, then peak live particles < 500.
7. Two headless runs of the same seed print a byte-identical summary.

## Out of Scope

- Raising `DEFAULT_MAX_PARTICLES` — an engine change owned by Phase 10.
- Per-prop art (cracked/rubble sprites). Shrink + debris only.
- Any change to *when* a shift fires. `begin_arena_shift` is now callable from
  anywhere, but the only caller in this lane is still the cleared-wave edge.

## Affected Boundaries

- `CPP/game/arena_vfx.hpp` (new — pure curves + prop bookkeeping)
- `CPP/game/main.cpp`: the arena-shift code path, `clear_arena_props`'
  neighbours, and the `// === HOOK: arena-vfx ===` block. No engine file, no
  CMake list, no `GameData.json` block.

## Task Breakdown

1. `arena_vfx.hpp`: `smoothstep`, `staggered_t`, `capture_props`,
   `teardown_props`, `scale_prop`, `animate`, `destroy_all`.
2. Move the crossfade's smoothstep lambda onto the helper (D76).
3. Extract the shift-start into `begin_arena_shift(int)` (D78) and call
   `teardown_props` from it, before anything else.
4. Drive both sets from the hook block against `shift_timer`.
5. `test_arena_vfx.cpp` — curve, ordering, sweep, particle census.

## Open Questions

- None blocking. Unplayed in a window; the visual read of a 0.55 stagger span is
  a taste call that should be confirmed in the Phase 10 playtest.
