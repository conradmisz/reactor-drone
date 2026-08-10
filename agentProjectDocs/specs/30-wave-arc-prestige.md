# Feature Spec: 30-wave arc + prestige runs (Iteration 5, Lane O, #14)

## Status

Done (unplayed)

## User Story

As a player, I want a run that ends at wave 30 and an option to *prestige* —
restart the arc with a permanently stronger hull but no purchased upgrades — so
that the arc is finishable in one sitting and finishing it still means something.

## Requirements

1. `waves` holds **30** rows, generated from the same linear-ramp family as D53:
   waves 1-15 fixed-count, 16-30 timed, bosses on **10 / 20 / 30**.
2. Pressure is monotonically non-decreasing across the whole table (the D53
   invariant `test_wave_arc.cpp` already asserts).
3. The nine arenas re-key to `first_wave` 1/4/8/12/16/19/23/27/**30** — the same
   two passes plus the Singularity finale, which must land on the last boss wave.
4. Moon-shooter injection waves rescale 3/15/30 -> 3/9/18 so tier 3 is not
   introduced on the final wave only.
5. `saves/meta.json` gains one field, `prestige`, clamped to 0..5. Everything
   derived from it (the stat multipliers) is computed, never stored (D80/D81).
6. Prestige buffs land at the **single** `start_run` application site, next to
   `apply_ship` / `apply_difficulty`, from the pristine `base_config` (D50).
7. Winning the arc raises a `prestige_offer` screen: PRESTIGE RUN (+1 level,
   fresh run, upgrades gone) or click anywhere to retry at the current level.
8. Replay determinism holds for a fixed prestige level; the level in force is
   printed at run start so a replay's inputs are visible.

## Acceptance Criteria

1. Given the shipped `GameData.json`, then `waves.size() == 30`, waves 1-15 are
   fixed-count, 16-30 timed, and exactly 10/20/30 carry `boss: true`.
2. Given any adjacent wave pair, then no pressure field has regressed.
3. Given wave 30, then `active_arena_index` returns the Singularity index (8).
4. Given prestige level L, then hull/speed/damage are scaled by
   1 + 0.10L / 1 + 0.05L / 1 + 0.08L, and L is clamped to 5.
5. Given a `meta.json` with `"prestige": 99`, when it loads, then the level is 5.
   Given a missing/corrupt file, then the level is 0 and the run still starts.
6. Given two runs at the same `--seed` and the same prestige level, then the
   headless output is byte-identical; given different levels, it differs.

## Out of Scope

- Balancing. Every number here is formula-generated or eyeballed, as before.
- Any UI that *displays* prestige outside the offer screen — Lane M owns the
  stat overview and reads `prestige_summary()` / the `prestige.level` key.
- Prestige currency, prestige-only content, per-ship prestige.

## Affected Boundaries

- `assets/GameData.json` — `waves`, `arenas[*].first_wave`,
  `enemy_types[*].first_wave` (moons), `screens.prestige_offer`
- `CPP/game/prestige.hpp` (new) — the bonus math and its public accessors
- `CPP/game/meta_save.{hpp,cpp}` — one persisted field
- `CPP/game/main.cpp` — one `start_run` line + one `HOOK: prestige` block
- `CPP/game/tests/unit/test_prestige.cpp` (new), `test_wave_arc.cpp`,
  `test_boss.cpp` (the shipped-data assertions move 50 -> 30)

## Task Breakdown

1. Regenerate the 30 wave rows by formula; splice into `GameData.json` by line
   range; re-parse to prove validity.
2. Re-key the nine arenas and the three moon `first_wave`s.
3. `prestige.hpp`: bonus, apply, summary, cap.
4. `MetaSave::prestige` + tolerant load/write.
5. `prestige_offer` screen block + the main.cpp hook that raises it, handles the
   click, and starts the next run.
6. Tests; update the two shipped-data test files.

## Open Questions

- **Waves 26-50 rotated-layout item (D53) is resolved by deletion**: the second
  pass now covers waves 16-29 instead of 26-49, so the mechanical 90-degree
  layouts are still there, just half as long-lived. They remain PROVISIONAL.
- Prestige percentages are unplayed guesses, like every other number here.
