# Feature Spec: Enemy projectiles & the moon shooters (#3)

## Status

Done — built, unit-tested, headless-verified. **Unplayed** (no windowed playtest).

## User Story

As a player, I want enemies that shoot back so that positioning matters as much
as aim, and so the arena stops being a melee-only problem.

## Requirements

1. An enemy whose `EnemyBehavior.kind` is `SHOOTER` fires on a **per-entity float
   countdown**, never an RNG draw — determinism is a project invariant.
2. A shot is the `PlayerFireSystem` recipe on `layers::ENEMY_SHOT`, carrying
   `ContactDamage`. **No new damage system**: `PlayerDamageSystem` already hurts
   the drone for anything carrying `ContactDamage` in its `CollidedWith`.
3. A shot is destroyed by whatever it hits. The tier-3 laser is the exception —
   it pierces, and that asymmetry *is* the tier-3 upgrade.
4. `moon_1/2/3` are three ordinary `enemy_types` rows (locked-in user answer),
   tiers 1 = slow straight, 2 = faster + tracking with a clamped turn rate,
   3 = laser. They reach the stream at roughly waves 3 / 15 / 30.
5. `WaveSpawnerSystem` attaches `EnemyBehavior` from the type's config.

## Acceptance Criteria

1. Given a shooter with a 1.0 s interval, when 0.75 s has passed, then nothing has
   been fired; at 1.0 s exactly one shot exists, and it re-arms rather than
   firing every frame.
2. Given a tier-2 shooter, when the drone is 90° off its muzzle, then the muzzle
   angle moves by at most `turn_rate * dt` that frame (a moving drone can
   out-turn it).
3. Given a shot and a laser that both report a `CollidedWith`, when the system
   ticks, then the shot carries a `DestroyRequest` and the laser does not.
4. Given a shot overlapping the drone, when `PlayerDamageSystem` then
   `DamageApplySystem` run, then hull falls by the shot's `ContactDamage.amount`
   — through the existing path, with no third system involved.
5. Edge case: a shot must pay **no** score and **no** currency, or a player could
   farm credits off incoming fire.

## Out of Scope

- Enemies leading their shots, or retreating to maintain range.
- Enemy shots hitting other enemies (no friendly fire, by mask).

## Affected Boundaries

`CPP/game/enemy_fire_system.{hpp,cpp}` (new), `wave_spawner_system.*`,
the `enemy_types` + `specialty` blocks of `GameData.json`,
`// === HOOK: enemy-fire ===`.

## Task Breakdown

1. `enemy_fire::turn_toward` + `shot_spec` as pure helpers (test first).
2. `enemy_fire::spawn_shot` — the shared projectile recipe.
3. `EnemyFireSystem::update`: tick, fire, then expire non-piercing shots.
4. Spawner attaches `EnemyBehavior`; the moon rows land in `GameData.json`.

## Open Questions

- None. **Note the deviation:** the moons are *not* written into any wave's
  `types` list (that block is Lane A's). They arrive on a spawn-counter cadence
  from `EnemyType::first_wave` instead — see D67.
