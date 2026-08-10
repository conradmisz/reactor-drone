# Feature Spec: Health & shield pickups (Game Note #10)

## Status

Done — Lane B, iteration 3 (D56). Unit-tested and headless-verified; not playtested.

## User Story

As a pilot deep in a wave, I want repair and shield scrap to appear around the
arena so that surviving is a matter of *moving* rather than of having banked
enough credits before the wave started.

## Requirements

1. On a data-driven interval, one `Pickup` of kind `Health` or `Shield` appears
   at a point inside the arena and at least `min_player_dist` from the drone.
2. At most `max_live` of these exist at once. Currency and key drops do not count
   against that cap, and it does not count against theirs.
3. `Health` clamps into the player's `Health` component; `Shield` clamps into
   `ShipState.shield` against `shield_max`. Neither can exceed its maximum and
   neither may fall through to the currency branch.
4. Placement is deterministic: the same seed and the same inputs place the same
   pickups on the same frames.
5. `sustain.interval == 0` leaves the game exactly as it was.

## Acceptance Criteria

1. Given `interval: 2.0` at 60 Hz, when 119 frames pass, then nothing is placed;
   on frame 120 exactly one pickup appears.
2. Given a hull at 75/100, when a 25-value health pickup is collected, hull is
   100; when a 999-value one is collected, hull is still 100 and credits are 0.
3. Given `shield_max == 0`, when a shield pickup is collected, shield stays 0 and
   credits stay 0 — an unbanked cell is worth nothing, not free money.
4. Given `max_live: 2` and twenty intervals with nothing collected, exactly two
   exist; collecting one frees exactly one slot and the next interval refills it
   (no banked burst).
5. Given two runs of the same configuration, the full sequence of placement
   positions and kinds is bit-identical.
6. Given `interval: 0`, 2000 frames place nothing.

## Out of Scope

- Generated sprite art. The pickups are flat `Color` rects for now; the green
  scrap sprite is an offline `assets/generator/v2/` change.
- Any interaction with the shop, the drop table, or the magnet item beyond what
  `PickupSystem` already does for every pickup.

## Affected Boundaries

- `CPP/game/sustain_spawn_system.{hpp,cpp}` (new), the `sustain-spawn` hook in
  `main.cpp`, `PickupSystem::update` (two new kind branches), the `sustain` block
  in `GameData.json`.

## Task Breakdown

1. `sustain_spawn` free function + its two pure helpers.
2. `PickupSystem` Health / Shield branches with clamping.
3. Turn `sustain.interval` on in `GameData.json`; delete the "inert" line in
   `test_scaffolding.cpp`.
4. `test_sustain_spawn.cpp`.

## Open Questions

None. (The one that was open — whether placement should draw from the seeded RNG
— was closed by D56: it does not draw at all.)
