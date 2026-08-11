# Feature Spec: Tutorial stage

## Status

Scoped (not started).

## User Story

As a new player, I want a guided tutorial run that teaches each control with
on-screen prompts so I learn move/aim/dash/items/shop by doing them, not by
reading a manual.

## Requirements

1. A **TUTORIAL** button on the main menu starts a fixed-seed run with a
   `tutorial.active` Blackboard flag set; normal wave spawning is suppressed by
   one guard in `WaveSpawnerSystem` (follow the D182 `boss_engaged_` latch
   precedent).
2. A new `TutorialSystem` (~150-250 lines) drives an **ordered step table** of
   `{prompt, done-predicate}` entries. Predicates read input/Blackboard state
   only. Steps, in order: move (WASD/arrows), aim + fire (mouse / SPACE hold),
   dash (SPACE edge), consumable (Q), active item (E), open shop (B), buy
   something, launch next wave.
3. One pooled label (pause_stats pooled-widget recipe, z ≥ 40) shows the
   current prompt as a literal `"[KEY] ACTION"` string — bracket convention
   per `pause_stats::active_key`; there is no key-binding abstraction.
4. ESC exits the tutorial back to the main menu at any step.
5. Optionally, one stationary dummy target spawns for the fire step.
6. **Determinism**: progression is input-driven only — no RNG draws, no sim
   timers — so the replay canary is safe by construction.
7. Guard test: the step table advances headlessly via synthetic Blackboard
   writes.

## Acceptance Criteria

1. Given the main menu, TUTORIAL starts a run with no normal wave spawns and
   the first prompt visible; a normal run started afterwards spawns waves as
   before (flag cleared).
2. Given each step's action performed in order (headless `--keys`/`--clicks`
   where drivable), the prompt advances to the next step; completing the final
   step ends the tutorial.
3. Given ESC mid-tutorial, the game returns to the main menu and a subsequent
   normal run is unaffected.
4. Given the guard test writing synthetic Blackboard state, every step's
   predicate fires and the table advances in order without a window.
5. Given `--keys 10:SPACE --seed 42` on a normal run, the canary summary is
   byte-identical twice — tutorial code is inert when the flag is unset.

## Out of Scope

- A key-binding/remap abstraction — prompts stay literal strings.
- Scripted enemy waves, combat tutorials beyond the dummy target, or a
  multi-stage curriculum.
- Persisting tutorial completion; auto-launching for first-time players.
- Any change to normal-run behavior beyond the single spawn guard.

## Affected Boundaries

- `assets/GameData.json` (main_menu TUTORIAL button; shares the button
  re-layout with the mechanics-page spec if both land)
- `CPP/game/main.cpp` (menu click branch, tutorial run start/flag)
- new `CPP/game/` TutorialSystem (step table + pooled prompt label)
- `CPP/game/` WaveSpawnerSystem (one suppress guard)
- `CPP/game/tests/unit/` (step-table guard test)
- `ENGINE.md` if the system slots into the frame order (same commit)

## Task Breakdown

1. Step table + predicates + guard test (headless, synthetic Blackboard) —
   this is ~80% of the feature.
2. Wave-suppression guard + tutorial.active flag + fixed-seed entry from a
   new menu button.
3. Pooled prompt label + ESC exit; optional dummy target.
4. Gate-green (warning-free build, ctest, canary ×2) + a headless scripted
   walk-through of the full step sequence.

Estimate: 1-2 days.

## Open Questions

- Where the "launch next wave" step ends: exit to menu automatically, or hand
  control to a real wave 1? Decide before task 2.
- Dummy target: include or cut? Cut if the fire predicate works on input alone.
