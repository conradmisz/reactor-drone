#ifndef ENEMY_SEEK_SYSTEM_HPP
#define ENEMY_SEEK_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"

/**
 * EnemySeekSystem — steers every enemy's Velocity straight at the player.
 *
 * Replaces Class-090's path following: instead of walking a precomputed grid
 * path, each enemy points its velocity at the player's current center at its own
 * speed (reused from PathFollower.speed). The engine MovementSystem then moves it.
 * No-op when there is no player (e.g. before spawn or after death).
 */
class EnemySeekSystem {
public:
    void update(ComponentStorage& storage);
};

#endif // ENEMY_SEEK_SYSTEM_HPP
