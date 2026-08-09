#ifndef COLLISION_LAYERS_HPP
#define COLLISION_LAYERS_HPP

#include <cstdint>

/**
 * Collision layer/mask bits for the arena. Two entities collide when
 * (a.layer & b.mask) && (b.layer & a.mask) (see collision_math::layers_compatible).
 *
 *   player     ↔ enemy       (contact damage)
 *   player     ↔ hazard      (contact damage; static ContactDamage patch)
 *   player     ↔ enemy shot  (contact damage; enemy-fired projectile)
 *   projectile ↔ enemy       (shots hit enemies)
 *   projectile ↔ obstacle    (shots stop dead)
 *   enemy      does NOT collide with enemy; the PLAYER's projectiles do NOT hit
 *   the player, and enemy shots do NOT hit enemies (no friendly fire either way).
 *   (drone-vs-obstacle blocking is resolved in code, not via layers — obstacles.hpp)
 */
namespace layers {
constexpr uint8_t PLAYER     = 0x01;
constexpr uint8_t ENEMY      = 0x02;
constexpr uint8_t PROJECTILE = 0x04;
constexpr uint8_t OBSTACLE   = 0x08;   // v2 Phase 6: solid static block
constexpr uint8_t HAZARD     = 0x10;   // v2 Phase 6: static ContactDamage patch
// Iteration 3 (D51): a projectile fired BY an enemy. A separate bit rather than
// reusing PROJECTILE because that layer's mask is what stops player shots hitting
// the player — an enemy shot needs the mirror image of it.
constexpr uint8_t ENEMY_SHOT = 0x20;

// Enemy shots reach the drone through PLAYER_MASK, which is why they need no
// damage system of their own: they carry ContactDamage, and PlayerDamageSystem
// already hurts the drone for anything carrying it (enemies and hazards alike).
constexpr uint8_t PLAYER_MASK     = ENEMY | HAZARD | ENEMY_SHOT;
constexpr uint8_t ENEMY_MASK      = PLAYER | PROJECTILE;    // enemies hit by player & shots
constexpr uint8_t PROJECTILE_MASK = ENEMY | OBSTACLE;       // shots hit enemies, stop on obstacles
constexpr uint8_t OBSTACLE_MASK   = PROJECTILE | ENEMY_SHOT;// obstacles stop shots from either side
constexpr uint8_t HAZARD_MASK     = PLAYER;                 // hazards touch only the drone
constexpr uint8_t ENEMY_SHOT_MASK = PLAYER | OBSTACLE;      // enemy shots hit the drone, stop on walls
}

#endif // COLLISION_LAYERS_HPP
