# Feature Spec: Difficulty modes + hard mode (Gameplay Phase B)

## Status

Done — shipped 2026-08-09. Balance numbers **provisional** (nobody has played
them; see Open Questions).

## User Story

As a player, I want the early waves to actually threaten me, and I want to pick
Normal or Hard at the start of a run, so that the game has a sense of danger and
a reason to be replayed.

## Requirements

1. Normal's early waves (1-4) carry real pressure: more enemies, spawned closer
   together, moving faster, with the second and third enemy type arriving
   earlier than wave 4.
2. A run starts by choosing a difficulty on a `main_menu` screen. `SPACE` still
   starts a Normal run (headless scripts depend on it).
3. Hard scales *only* enemy-side numbers: count, spawn spacing, HP, speed, credit
   payout, hazard damage, and how early enemy types unlock. The player's stats,
   the drop economy and the shop are identical on both.
4. Difficulty is a set of multipliers over the single authored wave table — no
   second table, no new enemy types (the D10 rule).
5. Replay determinism survives: two runs of the same `--seed` and difficulty
   print an identical summary line.

## Acceptance Criteria

1. Given the game starts, when the first frame renders, then the `main_menu`
   screen is up with NORMAL and HARD buttons. ✅
2. Given the main menu, when HARD is clicked, then the run starts and stdout
   reads `Run start: difficulty Hard`. ✅ (`--clicks 10:622,349`)
3. Given the main menu, when `SPACE` is pressed, then a Normal run starts. ✅
4. Given the same `--seed` and difficulty, when the run is repeated, then the
   summary line is byte-identical. ✅ (both difficulties, twice each)
5. Given an idle drone on seed 42, when it is left to be swarmed, then it dies
   *sooner* on Hard than on Normal — the multipliers reach the simulation, not
   just the label. ✅ (dead by frame 900 on Hard, alive at 900 on Normal)
6. Given a difficulty with `type_lookahead > 0`, when it is applied, then each
   wave's roster is the union of its own and the next N waves' rosters, computed
   from the *original* rosters. ✅ (unit test)
7. Given a wave whose `types` is empty ("every type"), when a lookahead is
   applied, then it stays empty — a merge must never narrow it. ✅ (unit test)
8. Given any difficulty, when it is applied, then no player, shop or economy
   field changes. ✅ (unit test)

## Out of Scope

- A harsher player economy on Hard — explicitly rejected by the user.
- Hazard *behaviour* changes; Hard only scales `HazardDef::damage`.
- The boss and the moon enemy types (Phase C) — "more lethal boss" cannot ship
  before the boss does.
- An Options screen, and remembering the last difficulty across launches (no
  save system yet).
- Showing the active difficulty in the HUD.

## Affected Boundaries

- `CPP/game/arena_config.{hpp,cpp}` — `DifficultyDef`, `apply_difficulty`, parse.
- `CPP/game/main.cpp` — `base_config`, `start_run()`, the title-screen menu, and
  the Escape guard at the title.
- `assets/GameData.json` — rebalanced `waves`, +25% `enemy_types` speeds, new
  `difficulties` block, new `main_menu` screen.
- No engine change, so `ENGINE.md` is untouched.

## Task Breakdown

1. `DifficultyDef` + `apply_difficulty` (header-only, pure, testable). ✅
2. Loader block + `GameConfig::difficulties`. ✅
3. `main_menu` screen data + `start_run()` wiring. ✅
4. Rebalanced Normal wave table + enemy speeds. ✅
5. `test_difficulty.cpp` — scaling, lookahead, the player-untouched guarantee,
   and a contract test on the shipped menu's callback names. ✅

## Open Questions

- **The numbers are unplayed.** "Aggressive" (wave 1 = 12 enemies at 0.45 s,
  +25% enemy speed) is the user's stated target, not a measured one. Waves 5-11
  were rescaled to stay above wave 4 and are the least-justified part.
- Does Hard's `count_mult 1.5` blow the 2000-particle budget in the late fixed
  waves (46 × 1.5 = 69 enemies)? Untested — it needs a played late-game run.
- Should the active difficulty appear anywhere in-game after the menu closes?
