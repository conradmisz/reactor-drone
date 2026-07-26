#ifndef COLLISION_LAYERS_HPP
#define COLLISION_LAYERS_HPP

#include <cstdint>

/**
 * Collision layer/mask bits for the arena. Two entities collide when
 * (a.layer & b.mask) && (b.layer & a.mask) (see collision_math::layers_compatible).
 *
 *   player     ↔ enemy       (contact damage)
 *   player     ↔ hazard      (contact damage; static ContactDamage patch)
 *   projectile ↔ enemy       (shots hit enemies)
 *   projectile ↔ obstacle    (shots stop dead)
 *   enemy      does NOT collide with enemy; projectile does NOT hit the player.
 *   (drone-vs-obstacle blocking is resolved in code, not via layers — obstacles.hpp)
 */
namespace layers {
constexpr uint8_t PLAYER     = 0x01;
constexpr uint8_t ENEMY      = 0x02;
constexpr uint8_t PROJECTILE = 0x04;
constexpr uint8_t OBSTACLE   = 0x08;   // v2 Phase 6: solid static block
constexpr uint8_t HAZARD     = 0x10;   // v2 Phase 6: static ContactDamage patch

constexpr uint8_t PLAYER_MASK     = ENEMY | HAZARD;         // enemies + hazards damage the drone
constexpr uint8_t ENEMY_MASK      = PLAYER | PROJECTILE;    // enemies hit by player & shots
constexpr uint8_t PROJECTILE_MASK = ENEMY | OBSTACLE;       // shots hit enemies, stop on obstacles
constexpr uint8_t OBSTACLE_MASK   = PROJECTILE;             // obstacles register shot impacts
constexpr uint8_t HAZARD_MASK     = PLAYER;                 // hazards touch only the drone
}

#endif // COLLISION_LAYERS_HPP
