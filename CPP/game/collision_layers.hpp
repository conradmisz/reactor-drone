#ifndef COLLISION_LAYERS_HPP
#define COLLISION_LAYERS_HPP

#include <cstdint>

/**
 * Collision layer/mask bits for the arena. Two entities collide when
 * (a.layer & b.mask) && (b.layer & a.mask) (see collision_math::layers_compatible).
 *
 *   player     ↔ enemy       (contact damage)
 *   projectile ↔ enemy       (shots hit enemies)
 *   enemy      does NOT collide with enemy; projectile does NOT hit the player.
 */
namespace layers {
constexpr uint8_t PLAYER     = 0x01;
constexpr uint8_t ENEMY      = 0x02;
constexpr uint8_t PROJECTILE = 0x04;

constexpr uint8_t PLAYER_MASK     = ENEMY;                 // player collides with enemies
constexpr uint8_t ENEMY_MASK      = PLAYER | PROJECTILE;   // enemies hit by player & shots
constexpr uint8_t PROJECTILE_MASK = ENEMY;                 // shots collide with enemies
}

#endif // COLLISION_LAYERS_HPP
