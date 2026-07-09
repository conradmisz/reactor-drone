#ifndef PLAYER_FIRE_SYSTEM_HPP
#define PLAYER_FIRE_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include <random>

/**
 * PlayerFireSystem — spawns projectiles along the player's aim while fire is held.
 *
 * Fire is held when the player's Input.fire (space) is set OR the Blackboard flag
 * "mouse.held" (left button down) is true. Firing is gated by WeaponStats:
 * cooldown_remaining ticks down by delta_time and, when a shot is fired, is reset
 * to 1 / fire_rate. Projectiles get a Velocity along the player's Rotation.angle
 * (jittered by WeaponStats.spread), a short Lifetime, and projectile-layer colliders.
 */
class PlayerFireSystem {
public:
    explicit PlayerFireSystem(unsigned int seed = 1234u) : rng_(seed) {}

    void update(ComponentStorage& storage,
                EntityManager& entity_manager,
                const Blackboard& blackboard);

private:
    std::mt19937 rng_;
};

#endif // PLAYER_FIRE_SYSTEM_HPP
