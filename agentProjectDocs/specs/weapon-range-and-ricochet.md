# Feature Spec: Long Barrel & Ricochet Coils (weapon shop rows)

## Status

Done

## User Story

As a player, I want to buy an upgrade that makes my shots reach further and one
that makes them bounce off walls, so that the shop offers a choice about *how*
my gun behaves and not only how hard it hits.

## Requirements

1. Two new rows in the existing `shop.upgrades` catalogue — `range` and
   `bounce` — bought, priced and levelled through the same path as every other
   upgrade row. No parallel item system, no new component type.
2. `range` increases how far a shot travels **without** changing its speed.
3. `bounce` makes a player shot reflect off obstacles and the arena boundary
   ring instead of being destroyed, spending one bounce per surface.
4. A shot with no bounces left behaves exactly as it does today: it stops dead
   on an obstacle and expires on `Lifetime` past the ring.
5. Fully deterministic — no RNG on the reflection path.
6. The particle budget (`DEFAULT_MAX_PARTICLES` = 4000) is not raised, and the
   cost of the change is measured, not assumed.

## Acceptance Criteria

1. Given a stock weapon, when Long Barrel is bought, then
   `projectile_speed` is unchanged and `speed * projectile_lifetime` rises by
   exactly `speed * 0.30`.
2. Given three Long Barrel purchases, when a fourth is attempted, then
   `upg_counts` stays at 3 (max_stacks) and the price curve is 80 / 120 / 180.
3. Given a shot with `bounces > 0` overlapping an obstacle, when
   `ProjectileHitSystem` runs, then the shot is **not** destroyed, its velocity
   is mirrored through the surface normal at unchanged speed, and its bounce
   count drops by one.
4. Given the same shot on the frame after a bounce, when the same collision is
   reported again, then no further bounce is spent and the shot is not
   destroyed — it is already clear of the surface.
5. Given a shot whose bounce budget is spent, when it meets an obstacle, then it
   is destroyed exactly as before.
6. Given a shot leaving the arena ring with bounces left, then it is reflected
   inward and repositioned inside the ring; with none left it flies out and
   expires on `Lifetime`.
7. Given a frame in which a shot overlaps **both** a wall and an enemy, then the
   enemy takes the damage and the bounce is not spent.

## Out of Scope

- Bouncing enemy projectiles (`ENEMY_SHOT` carries no `ProjectileTag`).
- Damage falloff or gain per bounce.
- Any change to the shop *screen* — Lane H owns its presentation; the 8 authored
  `shop_card_*` widgets already cover the now-8 upgrade rows exactly.
- Bounce reflecting off hazards (they are pass-through by design).

## Affected Boundaries

- `CPP/game/bullet_bounce.hpp` (new, pure — reuses `obstacles.hpp`)
- `CPP/game/projectile_hit_system.{hpp,cpp}`
- `CPP/game/player_fire_system.cpp`
- `CPP/game/tower_components.hpp` — one `int bounces` on `ProjectileData`
- `CPP/game/shop_system.cpp` — two `apply()` cases
- `assets/GameData.json` — two `shop.upgrades` rows
- `CPP/game/main.cpp` — two lines: `set_arena` wire-up, per-run reset

## Task Breakdown

1. `bullet_bounce.hpp`: `off_aabb` / `inside_circle` reflection helpers.
2. `ProjectileData.bounces`, stamped per shot by `PlayerFireSystem`.
3. `ProjectileHitSystem`: enemy-first resolution, then bounce-or-die.
4. Catalogue rows + `apply()` cases + per-run reset.
5. `test_weapon_items.cpp`; measure the particle cost headlessly.

## Open Questions

None.
