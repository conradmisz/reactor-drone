# Feature Spec: Main menu suite

## Status

Done — shipped 2026-08-10 in three phases (D137); all verification headless.

## User Story

As a player, I want a real front-end — a menu hub with run setup, save slots,
settings, records and a way back to it — so the game opens like a finished game
instead of dropping me on a difficulty prompt.

## Requirements

1. Boot lands on a **hub**: CONTINUE (newest save, hidden when none), PLAY,
   SAVE SLOTS, RECORDS, HOW TO PLAY, SETTINGS, QUIT.
2. PLAY opens **run_setup**: Normal/Hard (selected one visibly highlighted),
   the ship selector (locked ship keeps showing its unlock threshold), LAUNCH,
   BACK. `SPACE` at the title still quick-starts a Normal run — the replay
   canary's `--keys 10:SPACE` path must not change meaning.
3. **Three save slots** (`saves/run1..3.json`). The legacy `saves/run.json`
   migrates to slot 1 once. The slots screen shows wave/difficulty/ship/score
   per slot with LOAD and DELETE; empty slots say so and disable both.
   A fresh run saves to the first empty slot (else slot 1); a loaded run saves
   back to its own slot. Saves carry `saved_at` so CONTINUE picks the newest.
4. **Settings**: screen-shake and minimap checkboxes, persisted in
   `saves/settings.json`, applied live via Blackboard flags. Defaults preserve
   current behaviour exactly (both on).
5. **Records**: lifetime score, prestige level, best wave, runs flown.
   `meta.json` gains `best_wave` and `runs_played`, updated where scores bank.
6. **How to play**: one static screen — controls + the rules that catch people.
7. **Return to menu**: pause gains MAIN MENU; at game-over/victory, ESC returns
   to the menu (click/SPACE still restarts, unchanged).
8. All screens are data (`GameData.json → screens`), on the 800×600 canvas and
   the D88 grid; every button reachable by mouse AND keyboard/scripted clicks;
   no engine changes; no new widget types; persistent state never reaches a
   fresh run's sim (D80 discipline — timestamps and settings are write/read
   outside the sim).

## Acceptance Criteria

1. Given a fresh boot with no saves, the hub shows no CONTINUE and PLAY →
   run_setup → LAUNCH starts a Normal run (also reachable headlessly by
   `--clicks`).
2. Given `--keys 10:SPACE --seed 42`, the summary line is byte-identical to
   the pre-feature canary twice in a row.
3. Given saves in slots 2 and 3, CONTINUE resumes the newer one; the slots
   screen LOADs either; DELETE empties a slot and its file; a legacy
   `run.json` appears as slot 1 after first boot and the old file is gone.
4. Given screen shake off in settings, a hit produces no camera offset and the
   file survives restart; a missing/corrupt `settings.json` yields defaults.
5. Given a finished run (death or victory), `meta.json`'s `best_wave` /
   `runs_played` update and the records screen shows them.
6. Given a mid-run pause → MAIN MENU, the hub appears, the world is frozen
   behind it, and PLAY starts a clean fresh run.
7. Each new screen has a pin test in the `test_*_screen.cpp` convention; a
   malformed slot file degrades to "empty slot", never a crash.

## Out of Scope

- Audio settings (no audio system exists), fullscreen toggle, rebindable keys.
- Autosave/checkpoints; more than three slots.
- A game_over/victory screen redesign (ESC path only).
- Any engine (`CPP/engine/`) change, including a widget visibility flag —
  hidden widgets use the established blank-label + flat-style trick (D82).

## Affected Boundaries

- `assets/GameData.json` (screens, ui_styles only if a style is missing)
- `CPP/game/main.cpp` (phase machine handlers, refresh functions)
- `CPP/game/run_save.{hpp,cpp}` (slot paths, saved_at, migration)
- `CPP/game/meta_save.{hpp,cpp}` (best_wave, runs_played)
- new `CPP/game/settings_save.hpp` (mirrors meta_save pattern)
- `CPP/game/tests/unit/` (per-screen pins + slot/settings tests)

## Task Breakdown

1. **Phase A** — hub (CONTINUE/PLAY/QUIT) + run_setup; SPACE path pinned by
   the existing canary; update `test_difficulty.cpp` expectations.
2. **Phase B** — slot plumbing + migration + save_slots screen + newest-first
   CONTINUE; slot tests.
3. **Phase C** — settings_save + gates at the shake/minimap sites + records +
   how_to_play + pause MAIN MENU + ESC-from-end; remaining tests.

Each phase lands gate-green (warning-free build, 8/8 ctest, canary ×2).

## Open Questions

— (resolved: slots=3; settings=shake+minimap; extras=records/how-to/quit;
launch flow=single setup screen; ESC is the end-screen menu path)
