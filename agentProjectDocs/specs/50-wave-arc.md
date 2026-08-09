# Feature Spec: 50-wave arc + four tuning fixes (Iteration 3, Lane A)

## Status

In progress

## User Story

As a player, I want a 50-wave run with a flatter power curve and a shop that
opens on a predictable cadence, so that I bank power between difficulty spikes
instead of being outscaled by wave 8.

## Requirements

1. `waves` holds **50** rows. Waves 1-25 are fixed-count, 26-50 are timed
   (`duration > 0`). Waves 10/20/30/40/50 carry `"boss": true`.
2. Pressure is monotonically non-decreasing across the whole table: `count`,
   `duration`, `hp_mult`, `speed_mult` never fall, `spawn_interval` never rises.
3. `arenas` holds **8** entries at `first_wave` 1/7/13/20/26/32/38/45 — four
   themes, twice. The second pass reuses images/palettes and carries
   `specialty_tier: 2` with a different obstacle/hazard layout.
4. Full shop opens every 5th cleared wave (`% 5 == 0`), not every 4th.
5. Shield regen rate comes from `shop.shield_regen_frac` (0.08) instead of a
   hardcoded `0.2f`; `shop.shield_regen_delay` becomes 5.0.
6. The pause screen's footnote label fits inside its 300 px panel.

## Acceptance Criteria

1. Given the shipped `GameData.json`, when it is parsed, then `waves.size() == 50`
   and exactly waves 10/20/30/40/50 have `boss == true`.
2. Given any adjacent wave pair, when their pressure fields are compared, then
   none has regressed.
3. Given waves 1, 7, 13, 20, 26, 32, 38 and 45, when `active_arena_index` runs,
   then it returns 0..7 respectively — 8 distinct arenas.
4. Given a second-pass arena, then its obstacle/hazard **count** matches its
   first-pass twin but its coordinates differ, and `specialty_tier == 2`.
5. Given a Shield Capacitor purchase, when it applies, then
   `shield_regen == shield_max * shop.shield_regen_frac`.
6. Given cleared waves 1..50, then the full shop is due exactly on multiples of 5.
7. Edge case: a `GameConfig` with no `shop` block still defaults
   `shield_regen_frac` to 0.08 rather than 0.

## Out of Scope

- **Coin despawn.** The plan's item #8 (coins never despawn + intermission
  sweep) is **reversed** — see D52. `economy.pickup_lifetime` stays 12.0.
- Consuming `boss: true` (Phase 8), `specialty_tier` (Phase 7), the moon enemy
  types (Phase 6). This phase only authors the data.
- Balancing the second-pass layouts by hand — see Open Questions.

## Affected Boundaries

- `assets/GameData.json` — `waves`, `arenas`, `shop`, `screens.pause`
- `CPP/game/arena_config.{hpp,cpp}` — one `ShopConfig` field + its parse
- `CPP/game/shop_system.cpp` — one line, reads the new field off `cfg_`
- `CPP/game/main.cpp` — the shop-cadence line only

## Task Breakdown

1. Generate the 50-row wave table by formula; splice it into `GameData.json` by
   line range (never a whole-file Python round-trip — house style would be lost).
2. Re-key the four existing arenas to 1/7/13/20; append four mechanically
   derived twins at 26/32/38/45.
3. `ShopConfig::shield_regen_frac`, its parse, and the `shop_system.cpp` use.
4. `% 4 == 0` → `% 5 == 0`.
5. Pause label: shorter string, wider rect.
6. `CPP/game/tests/unit/test_wave_arc.cpp`.

## Open Questions

- **Second-pass arena layouts are PROVISIONAL.** They are the first-pass layout
  rotated 90° about the arena centre (1600,1600) with w/h swapped — a
  mechanical, in-bounds transform, deliberately *not* a blind hand-design. They
  need a real playtest before anyone calls them balanced.
