#ifndef LIFETIME_SYSTEM_HPP
#define LIFETIME_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * Decrements Lifetime::remaining each frame and attaches DestroyRequest
 * when an entity's lifetime expires (remaining <= 0).
 *
 * Reads: Lifetime, delta_time from Blackboard
 * Writes: Lifetime::remaining, DestroyRequest (tag)
 */
class LifetimeSystem {
public:
    void update(ComponentStorage& storage, const Blackboard& blackboard);
};

#endif // LIFETIME_SYSTEM_HPP
