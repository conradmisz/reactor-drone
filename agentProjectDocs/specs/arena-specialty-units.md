# Feature Spec: Arena specialty units (#9)

## Status

Done — built, unit-tested, headless-verified. **Unplayed** (no windowed playtest).

## User Story

As a player, I want each arena theme to field its own signature enemy so that
arriving in a new arena changes how I have to fight, not just what I am looking
at.

## Requirements

1. Four behaviours, one per theme:
   - **Bio-lab — spitter**: leaves a short-lived damaging patch behind it.
   - **Foundry — miner**: drops proximity mines.
   - **Core — bulwark**: frontal damage reduction, slow turn rate.
   - **Prism — splitter**: splits into two smaller units on death (user-confirmed).
2. `ArenaDef.specialty_unit` selects the type; the spawner injects one every N
   spawns while that arena is live.
3. The second pass (waves 26-50) carries `specialty_tier: 2` — **the same unit,
   harder**, not a different unit.
4. The transient-hazard entity recipe is a shared helper, not a copy per user.

## Acceptance Criteria

1. Given the shipped `GameData.json`, when the specialty unit of each arena is
   resolved, then Bio-lab→spitter, Foundry→miner, Core→bulwark, Prism→splitter,
   on **both** passes, and no arena resolves to -1.
2. Given wave 1 vs wave 26, when the arena is resolved, then both name the same
   `specialty_unit` and their `specialty_tier` is 1 and 2 respectively.
3. Given a spitter on its interval, when it acts, then exactly one `HAZARD`-layer
   entity with a positive, bounded `Lifetime` exists — a patch with no `Lifetime`
   would be a permanent hazard the arena never authored.
4. Given a deployed mine and the drone outside its trigger radius, when the
   system ticks, then nothing happens; inside the radius, the mine is destroyed
   and one blast patch replaces it (the blast is the damage, not the mine).
5. Given a bulwark facing the drone, then its `Health.armor_multiplier` is < 1;
   flanked (attacker outside the frontal arc), it is exactly 1.
6. Given a tier-1 splitter's death, then exactly **two** children exist, each
   smaller and weaker than the parent and carrying **no** `EnemyBehavior` — and
   killing a child produces no grandchildren.

## Out of Scope

- Routing `main.cpp`'s permanent arena vents through the shared helper (Lane E
  is rewriting that same lambda — see D69).
- Per-unit art. All four reuse existing enemy sprites.

## Affected Boundaries

`CPP/game/specialty_system.{hpp,cpp}` + `hazard_patch.hpp` (new),
`wave_spawner_system.*`, `enemy_death_system.cpp` (the splitter),
the `enemy_types` + `specialty` blocks of `GameData.json`,
`// === HOOK: specialty ===`.

## Task Breakdown

1. `hazard::spawn_patch` — the one transient-hazard recipe.
2. `specialty::inside_arc` as a pure helper (test first).
3. `SpecialtySystem::update` — spitter, miner + deployed mine, bulwark.
4. Splitter in `EnemyDeathSystem`, after `drop_loot`, drawing no RNG.
5. `specialty` data block: cadences + the arena→type name map.

## Open Questions

- None. A deployed mine reuses `EnemyBehavior{MINER, tier 0}` rather than a new
  `MineTag`; the three spare fields carry arm delay, blast damage and radius.
