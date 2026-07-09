#ifndef COLLISION_SYSTEM_HPP
#define COLLISION_SYSTEM_HPP

#include "collision_strategy.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * Orchestrates collision detection each frame.
 *
 * Delegates to a CollisionStrategy for the actual detection, then translates
 * the resulting pairs into per-entity CollidedWith components in ComponentStorage.
 * Clears all existing CollidedWith components at the start of each update
 * (no accumulation across frames).
 */
class CollisionSystem {
public:
    /**
     * Construct with a collision strategy.
     * The CollisionSystem does NOT own the strategy — caller manages lifetime.
     */
    explicit CollisionSystem(const CollisionStrategy& strategy);

    /**
     * Switch to a different collision strategy at runtime.
     * The CollisionSystem does NOT own the strategy — caller manages lifetime.
     */
    void set_strategy(const CollisionStrategy& strategy);

    /**
     * Run collision detection and write CollidedWith components.
     *
     * Clears all existing CollidedWith components, runs the strategy,
     * then attaches a CollidedWith component to each entity involved
     * in a collision containing all of its collision partners.
     *
     * @param storage Component storage for entity queries and CollidedWith output
     */
    void update(ComponentStorage& storage, Blackboard& blackboard);

private:
    const CollisionStrategy* strategy_;
};

#endif // COLLISION_SYSTEM_HPP
