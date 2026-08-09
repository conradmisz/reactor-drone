# Feature Spec: Persistent score and ship unlocks

## Status

In progress (Lane F, iteration 3). All numbers provisional and unplayed.

## User Story

As a player, I want my score to accumulate across runs so that reaching a
lifetime total of 4000 unlocks a second, purple ship with a different primary
fire (fast gatling, low damage per shot).

## Requirements

1. A meta-save at `saves/meta.json` holds one number: lifetime cumulative score.
   Missing or corrupt file = defaults, never a crash.
2. Lifetime score grows at **run end** (death, victory, quit) by that run's
   final score. It is written once per run.
3. Ships are data: a `ships` block in `GameData.json`, each entry a
   sidecar/idle_clip/weapon variant plus `unlock_score`.
4. `ships[ship_id]` is applied over `PlayerConfig` at run start, in the one
   place `apply_difficulty` is applied from the pristine `base_config`.
5. The title screen offers ship selection only when more than one ship is
   unlocked; while the purple ship is locked, the screen shows its threshold.
6. The meta-save must not influence anything the replay canary observes for a
   given ship.

## Acceptance Criteria

1. Given no `saves/` directory, when the game starts, then lifetime score reads
   0 and the run plays normally.
2. Given a `meta.json` containing garbage, when it is loaded, then it falls back
   to lifetime 0 (no exception escapes).
3. Given a written meta-save, when it is re-read, then lifetime score
   round-trips exactly.
4. Given lifetime 3999, the purple ship is locked; given exactly 4000, it is
   unlocked.
5. Given the purple ship is locked, when the ship-cycle button is clicked, then
   the selection stays on Standard (a locked ship is never selectable).
6. Given `ship_id = 1`, when a run starts, then `PlayerConfig::weapon` equals the
   purple ship's weapon.
7. Two runs of `--seed 42 --keys 10:SPACE --stopframe 3000` print byte-identical
   summary lines, with and without an existing meta-save.

## Out of Scope

- Run-state save/load (entity snapshots, `SerializationRegistry`, `LoadSystem`).
- Per-ship art. The purple ship reuses the standard drone sprite — the player
  entity's persistent `Tint` is owned by `FlashSystem` (it removes the Tint when
  a hit flash expires), so a tinted hull needs a `flash_system.cpp` change or a
  generated sprite. Pillow is not installed here and the generator is offline;
  both are a separate change (D83).
- Persisting the ship *choice* across runs — deliberately not saved, so the
  save file can never alter a replay.

## Affected Boundaries

`CPP/game/meta_save.{hpp,cpp}` (new), `arena_config.{hpp,cpp}` (`ShipDef` +
parse), `player_components.hpp` (`ShipState::ship_id`), `main.cpp` (run start /
run end / title menu), `assets/GameData.json` (`ships` + `main_menu`).

## Task Breakdown

1. `meta_save.{hpp,cpp}` — load/write/derive-unlocks, plus tests.
2. `ShipDef` + `ships` parse + `apply_ship`.
3. `ships` block and two `main_menu` widgets in `GameData.json`.
4. `main.cpp`: apply ship in `start_run`, bank score at run end, cycle ships on
   the title screen.

## Design notes

- **Unlocks are derived, not stored.** `unlocked = lifetime >= unlock_score`
  reads the threshold from `GameData.json` every start, so a retuned threshold
  takes effect immediately and a stored list can never desync from the data.
- **Gatling numbers (provisional):** Standard is `fire_rate 4.0 / damage 20.0`
  = 80 DPS. Purple is `fire_rate 12.0 / damage 6.0` = 72 DPS — 3x the cadence at
  90% of the sustained damage. The 10% is the price of a much more forgiving
  weapon: misses cost a third as much, and chip damage lands continuously.
  Spread is widened 0.06 -> 0.12 so the stream reads as a hose, and projectile
  lifetime is cut 1.0 -> 0.8s to keep the on-screen projectile count bounded.

## Open Questions

- None blocking. Purple art is deferred, not undecided.
