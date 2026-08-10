# Feature Spec: Coins never drop inside something

## Status

Done (Lane K, D101). Verified by unit test; unplayed.

## User Story

As a player, I want dropped money to be reachable, so that collecting my reward
is not a choice between the credits and standing in a poison patch.

## Requirements

1. A currency or key pickup must not overlap an obstacle, a hazard patch, a
   deployed enemy mine, or another pickup.
2. The fix must not change how many random numbers a kill draws, in any
   situation. Determinism is a project invariant (ENGINE.md §4) and
   `EnemyDeathSystem::drop_loot` is its reference implementation.
3. Coins keep their `Lifetime` (D52) — the despawn is the risk/reward the user
   explicitly asked for and is untouched.

## Acceptance Criteria

1. Given a kill at the centre of a hazard patch, when it drops loot, then no coin
   overlaps that patch.
2. Given a kill next to a pillar and a mine, when it drops loot, then no coin
   overlaps either, and no two coins overlap each other.
3. Given two identically-seeded worlds, one empty and one where every candidate
   position is blocked, when each kills an enemy and then kills a second in clean
   space, then the second kill's coin positions are **identical** between them —
   the search consumed no RNG in either.
4. Given a coin dropped in open space, when it is placed, then it does not move.
5. Given a spot with nothing free within reach, when a coin is placed, then it
   keeps its drawn position rather than searching further.

## Out of Scope

- Arena-circle and wall clamping. The drawn scatter could already put a coin
  slightly outside; the nudge is bounded and does not make it meaningfully worse.
- Sustain (health/shield) pickups. They are placed on a golden-angle spiral with
  no RNG at all (D56) and are already spread across the arena.
- Enemies and projectiles as blockers. Loot under a live enemy is fine and
  resolves itself; treating everything with a `Collider` as solid would nudge
  most coins in a crowded arena for no gain.

## Affected Boundaries

- `CPP/game/enemy_death_system.{hpp,cpp}`: `loot_place::blocked` /
  `loot_place::nudge_free`, called from `drop_loot`'s `make_pickup`.
- New test: `CPP/game/tests/unit/test_loot_placement.cpp`.

## Task Breakdown

1. `blocked()` — AABB test against `Collider` layers `OBSTACLE|HAZARD`, against
   `EnemyBehavior{MINER, tier 0}` (mines carry no collider), and against `Pickup`.
2. `nudge_free()` — a fixed golden-angle spiral of `SEARCH_STEPS` candidates,
   pure and RNG-free, first free one wins, drawn point as the fallback.
3. Call it inside `make_pickup`, so every kind of drop is covered once.

## Open Questions

- None.
