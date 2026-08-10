# Feature Spec: Save & quit mid-run

## Status

Done (Lane K, D100/D102/D103). Unplayed — verified headlessly and by unit test.

## User Story

As a player, I want to save my run from the pause menu and pick it up later, so
that closing the game does not cost me a 20-wave arc.

## Requirements

1. Pause -> **SAVE** writes the run in progress: wave, credits, score, hull,
   shield, gear/upgrade levels, owned items, ship, difficulty, seed.
2. The title screen offers **CONTINUE** only when a usable save exists, and says
   which wave and difficulty it will resume.
3. **Run state only.** No entity-graph snapshot, no `SerializationRegistry`, no
   `LoadSystem`. A resumed run is rebuilt by the ordinary run-start path and then
   has the saved numbers overlaid.
4. A missing, empty, malformed, wrongly-typed, partial or negative-valued save
   never crashes or stalls a run — it degrades to "no save" or to per-field
   defaults, matching `meta_load` (D80).
5. A save that merely exists must not change a fresh run's simulation by one RNG
   draw.
6. It is a *second* save, separate from Lane F's `saves/meta.json`, which stays
   exactly one number (D80).

## Acceptance Criteria

1. Given a run at wave 8 with 300 credits, when the player presses SAVE and
   restarts the process, then the title offers `CONTINUE - wave 8, Normal` and
   clicking it starts wave 8 with 300 credits and the same hull.
2. Given `saves/run.json` contains `{{{`, or `[]`, or `{"version": 99}`, when
   the game starts, then no CONTINUE is offered and the game runs normally.
3. Given a save with `hull` missing, when it is resumed, then the drone spawns at
   full health rather than dead.
4. Given any `saves/run.json`, when the replay canary is run twice with and twice
   without it, then all four summary lines are byte-identical.
5. Given a run that ends in death or victory, when the title is next shown, then
   no CONTINUE is offered — a finished run is not resumable.
6. Given a player who saves and then quits from the pause menu, when they return,
   then the save is still there (quitting does **not** clear it).

## Out of Scope

- Multiple save slots, and the `save_slots` screen from the original plan.
- Autosave. SAVE is an explicit act; adding a cadence later is one call site.
- Saving mid-wave progress. A resume restarts the saved wave from the top, which
  is why there is nothing half-spawned to reconcile.
- Restoring loot lying on the floor, live enemies, or arena-shift state: all of
  it is world, not run state.

## Affected Boundaries

- New: `CPP/game/run_save.{hpp,cpp}` + `CPP/game/tests/unit/test_run_save.cpp`.
- `CPP/game/main.cpp`: one field on `start_run`, the pause SAVE handler, the
  title CONTINUE handler, and the run-end clear.
- `CPP/game/wave_spawner_system.hpp`: `resume_at_wave(int)` (reset + one field).
- `assets/GameData.json`: `pause.pause_save`, `main_menu.menu_continue`.

## Task Breakdown

1. `RunSave` + tolerant load / write / clear / capture / apply.
2. `start_run(difficulty, const RunSave* resume)` — one construction path.
3. Pause SAVE widget and handler; title CONTINUE widget and handler.
4. Clear on death and victory only.
5. Tests + the canary with and without a save file.

## Open Questions

- None. (Noted, not open: the drop RNG is seeded once at startup from the
  *launch* config's seed, so a resumed run's loot stream follows that rather than
  the saved seed. It affects nothing observable and re-seeding it would change
  the economy scaling that `apply_difficulty` owns.)
