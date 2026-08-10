# Feature Spec: Modular chassis + the upgrade kit (D133/D134)

## Status

Done — built, unit-tested, headless-verified with real in-game frames.
**Unplayed** (no windowed playtest).

## User Story

As a player, I want the drone to visibly wear what I bought, so that my build is
something I can see in the arena rather than a column of numbers in the shop.

## Requirements

1. The player chassis is redesigned to *accommodate* a kit: slimmer/longer hull,
   pods further outboard, and the hardpoints (two flank rails, a tail socket)
   drawn EMPTY on the stock drone.
2. One overlay per shop upgrade row, each in its own longitudinal station so all
   seven can be worn at once without overlapping.
3. Shield Capacitor is NOT a static part — it is a field ring that hums *around*
   the drone (clear standoff gap, never touching the hull) and shows live state:
   humming, struck, broken, rebuilding.
4. No baked combinations: `max_stacks` spans 2..8 over eight rows, so the
   combinatorial atlas is ~1.6M sprites.
5. Deterministic: the kit is a pure function of `ShipState`, so the replay canary
   is unchanged.
6. Every part is symmetric about the horizontal axis (the art invariant: sprites
   face right and rotate, they never flip).

## Acceptance Criteria

1. Given a fresh run, when the drone spawns, then no part is drawn and the empty
   hardpoints are visible on the hull.
2. Given a purchase in row R, when the next frame renders, then exactly the part
   mapped to R appears — and nothing else lights up.
3. Given a Shield Capacitor purchase, when the shield is full/struck/emptied/
   refilling, then the ring draws hum/bloom/stubs/rebuild respectively, and the
   rebuild frame tracks `shield / shield_max` rather than elapsed time.
4. Given no Shield Capacitor, then no ring is drawn at all.
5. Given any state and any phase or fraction (including out-of-range), then the
   chosen frame index is inside the 21-frame strip.
6. Given `--seed 42`, two headless runs remain byte-identical.

## Design

- **Followers, not components.** An entity draws one sprite and the player's is
  the chassis, so each part is its own entity carrying `Images` + `Position` +
  `Size` + `Rotation` + `RenderLayer`, created by `spawn_world` beside the item
  aura and re-pointed at the player every playing frame.
- **Parked = zero size**, the pooled-and-parked idiom the minimap blips use
  (D58). No per-frame entity churn.
- **Parts are authored in the chassis's own 128-space**, so they composite 1:1
  at whatever size the player is drawn at, and ride the same rotation.
- **The field is 192px against the chassis's 128** and worn at `FIELD_SIZE_MULT`
  = 1.5x the player size. That ratio *is* the standoff gap.
- **One strip, no clips.** The field's four behaviours are a loop, a one-shot, a
  static and a progress bar; only one of those is an `Animation`. `main.cpp`
  writes `SpriteSheet.current_frame` from `upgrade_visuals::field_frame`.
- **The bloom is directional**: `PlayerDamageSystem` publishes
  `player.hit_bearing` (write-only, cosmetic) and the ring rotates to it. Every
  other part of the ring is radially symmetric, so the spin is invisible
  otherwise.

## Verification

`ctest` 8/8 (`test_kit_visuals.cpp`: row mapping, state machine, and every
(state, phase, fraction) landing inside the strip); manifest OK (13 sidecars);
canary byte-identical twice on `--seed 42`; real in-game frames captured with
`--screenshot` under a temporary full-kit patch (**reverted**), confirming the
parts rotate with the hull and the ring holds its gap.

## Out of Scope

- Parts scaling with stack level (they show *what* you own; the plume ramp from
  D123 still shows *how much*).
- A windowed playtest.
