#ifndef PLAYER_DAMAGE_SYSTEM_HPP
#define PLAYER_DAMAGE_SYSTEM_HPP

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * PlayerDamageSystem — drains player health on contact with an enemy, with a
 * short invulnerability window after each hit.
 *
 * Reads the player's CollidedWith (populated by the engine CollisionSystem). On
 * contact while not invulnerable it queues a DamageEvent against the player
 * (reusing the shared damage pipeline) using the enemy's ContactDamage.amount,
 * then starts an i-frame countdown ("player.invuln_window" seconds). The i-frame
 * timer lives on the Blackboard ("player.iframes") and ticks down by delta_time.
 */
class PlayerDamageSystem {
public:
    void update(EntityManager& entity_manager,
                ComponentStorage& storage,
                Blackboard& blackboard);
};

#endif // PLAYER_DAMAGE_SYSTEM_HPP
