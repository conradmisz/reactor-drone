---
id: 016
title: Tier-3 enemy lasers pass through obstacles — cover does not cover
status: resolved
severity: medium
area: gameplay
opened: 2026-08-16
resolved: 2026-08-16
---

## Symptom

Enemy projectiles visibly cross walls/pillars and hit the drone behind cover
(playtest #5 item 2, noticed at high waves).

## Reproduce

Reach a wave with tier-3 shooters (lasers); stand behind an obstacle in their
line of fire. The laser crosses the pillar and connects.

## Ruled Out

- **Tested:** audited every shot-spawn path's collision layers (player
  primary/secondary via PROJECTILE_MASK, enemy_fire/boss volley/bullet_pattern
  via ENEMY_SHOT_MASK, obstacles via OBSTACLE_MASK). **Observed:** all masks
  include OBSTACLE both directions. **Eliminates:** layer/mask authoring.
- **Tested:** read projectile_hit_system's pierce path. **Observed:** player
  pierce shots still stop on hit_solid. **Eliminates:** the player-side path.

## Suspects

1. **enemy_fire's die-on-hit loop exempts pierce shots from ALL collisions**
   — confirmed: the tier-3 laser skipped destruction whether it hit the drone
   or a wall.

## Resolution

The pierce exemption now applies only when nothing solid was struck: the loop
scans CollidedWith for any OBSTACLE-layer collider and destroys the shot on
walls regardless of pierce (D231). Lasers still pierce the drone-and-bodies
line as designed. Verified by build + suites; behavioural re-check is
playtest #6's (cover vs tier-3 lasers).
