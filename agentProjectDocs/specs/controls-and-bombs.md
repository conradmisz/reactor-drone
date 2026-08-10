# Feature Spec: Controls & Bombs (iteration 5, Lane N)

## Status

Done

## User Story

As a player, I want the dash on the key my thumb is already on, mines I can
shoot instead of only outrun, and a drone that visibly grows as I spend credits.

## Requirements

1. **#6** Dash fires on SPACE, on a 10 s cooldown. Pressing SPACE at the title
   starts a run and must NOT also dash; dashing in a run must NOT restart one.
2. **#9** Mine detonation radius is smaller, and a mine can be destroyed by
   shooting it.
3. **#7** Buying a shop upgrade visibly changes the drone.

## Acceptance Criteria

1. Given the title screen, when SPACE is pressed and held into the run, then the
   run starts and no dash fires (the edge was spent at the title).
2. Given a run in PHASE_PLAYING, when SPACE is pressed, then the drone dashes and
   `dash_cd` is 10 s; a second press inside that window does nothing.
3. Given an armed mine and a player projectile overlapping it, when
   SpecialtySystem ticks, then the mine detonates, the projectile is consumed and
   the blast is `MINE_BLAST_SIZE` = 100 (was 150).
4. Given a mine still inside its arm delay, when a projectile overlaps it, then
   nothing happens (the arm delay gates both trigger paths).
5. Given N shop upgrades bought, when the drone flies, then its engine plume's
   colour/size/rate step up with N, and at N = 0 it is byte-identical to the
   plume spawn_world writes today.
6. The seeded replay canary (`--seed 42 --keys 10:SPACE --stopframe 3000`) is
   byte-identical across two runs.

## Out of Scope

- New player sprite art (Lane L owns the generator and the drone sprite).
- Mine `Health`/`Collider` and an HP bar: a mine is a bomb, one shot pops it.
- Rebinding anything else; LSHIFT stays as the scripted/headless dash alias
  because scripted SPACE also means "start the run".

## Affected Boundaries

`main.cpp` HOOK: dash, `specialty_system.*`, `shop_system.cpp` (preview glow),
new `upgrade_visuals.hpp`, `GameData.json` -> `dash`.

## Task Breakdown

1. Dash on the existing `space_edge` (D120).
2. `MINE_BLAST_SIZE` 150 -> 100 (D121).
3. Mine detonates on projectile overlap (D122).
4. `upgrade_visuals.hpp` + per-frame apply + shop preview glow (D123).
5. `test_lane_n.cpp`, gate, canary.

## Open Questions

- None.
