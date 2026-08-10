# Feature Spec: The boss and its active items (#4)

## Status

Done — built, unit-tested, headless-verified. **Unplayed** (no windowed playtest).
One wiring action outstanding for the integrator (the wave-50 arena shift, D72).

## User Story

As a player, I want a themed capital ship every ten waves that ends by handing me
a new power, so the run has punctuation and my kit grows at the punctuation marks.

## Requirements

1. A wave flagged `boss` spawns exactly one boss: large, high HP, slow,
   `EnemyBehavior.kind == BOSS`, summoning adds on a timer.
2. **The wave clears when the boss dies** — and not before.
3. The boss is **themed to the live arena**: its tint, plus one signature attack
   borrowed from that arena's specialty unit (Foundry mines, Bio-lab spits).
4. Wave 50 is a distinct, harder fight in a black-hole/galaxy arena.
5. The kill pushes a `boss_reward` screen offering three actives; the pick writes
   `ShipState.active_id`. Later bosses upgrade the held active or offer an
   unowned one. All three sit on a 30 s `ShipState.active_cd`.
6. Hard-mode boss lethality is **one** `DifficultyDef` field, scaled in
   `apply_difficulty` — not a boss-specific difficulty path.
7. A boss fight makes **no RNG draws** (adds spawn on fixed angles).

## Acceptance Criteria

1. Given a non-boss wave, then no boss exists and the spawner's clear hold is
   down; given a boss wave, exactly one boss exists, the hold is up, and it stays
   exactly one across frames.
2. Given `summon_interval` 1.0 s at dt 0.25, when 3 ticks pass, then no adds; on
   the 4th, exactly `summon_count` adds; and it re-arms rather than summoning
   again immediately.
3. Given the boss is killed, then the reward screen is pushed, three choices are
   offered, and **the clear hold is still up**; after a pick, the hold drops, the
   click is consumed, and `ShipState.active_id` is the chosen effect's id.
4. Given a player already holding an active, then it is offered **first** as an
   UPGRADE, and re-picking it shortens the cooldown instead of granting a copy.
5. Given the Foundry arena, then the boss wears the Foundry tint and its
   `EnemyBehavior.tier` carries `MINER` as its signature.
6. Given the shipped data: 9 arenas, the 9th named `Singularity` at
   `first_wave: 50`; boss waves exactly at 10/20/30/40/50; `boss_mult > 1` on
   Hard, and applying Hard raises both boss HP and boss contact damage.
7. Edge case: the repulsion device fires **below** 20 % hull, not at it — at
   exactly 20 % the drone still owns its panic button.

## Out of Scope

- A real capital-ship sprite (the boss reuses the hulk plate; new art is offline
  generator work).
- Raising `DEFAULT_MAX_PARTICLES` — that is the integration phase's call.

## Affected Boundaries

`CPP/game/boss_system.{hpp,cpp}` + `active_items.{hpp,cpp}` (new),
`wave_spawner_system.*` (the clear hold), `item_system.hpp` (push extracted),
`arena_config.*` (`DifficultyDef::boss_mult`, `BossConfig` finale fields),
the `boss` / `actives` / `arenas`[9] / `screens.boss_reward` blocks of
`GameData.json`, `// === HOOK: boss ===` and `// === HOOK: actives ===`.

## Task Breakdown

1. `WaveSpawnerSystem::set_clear_hold` — the wave-clear gate.
2. `BossSystem`: spawn, summon, signature attack, death → reward, pick.
3. `boss_reward` screen + its contract test.
4. `actives::tick` — missiles, laser sweep, repulsion device.
5. `DifficultyDef::boss_mult`, scaled in `apply_difficulty`.

## Open Questions

- **Integrator, one line:** the wave-50 mid-fight shift calls a stub in the boss
  hook because this worktree predates Lane E's `begin_arena_shift`. See D72 and
  the `MERGE ACTION` comment in `main.cpp`.
- The actives are fired with `E` and have **no** `--keys` alias, so they are
  covered by unit tests rather than a headless script (D75).
