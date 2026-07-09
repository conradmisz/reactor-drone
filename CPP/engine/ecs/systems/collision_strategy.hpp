#ifndef COLLISION_STRATEGY_HPP
#define COLLISION_STRATEGY_HPP

#include "engine/ecs/component_storage.hpp"
#include <cstddef>
#include <vector>
#include <utility>

/**
 * Abstract interface for collision detection strategies.
 *
 * Each strategy receives a const reference to ComponentStorage and returns
 * a vector of collision pairs. Pairs are normalized: smaller Entity ID first.
 * No duplicates.
 *
 * Implementations: BruteForceStrategy (Phase 2), UniformGridStrategy (Phase 8),
 * QuadtreeStrategy (Phase 8).
 */
class CollisionStrategy {
public:
    virtual ~CollisionStrategy() = default;

    /**
     * Detect all colliding entity pairs.
     *
     * @param storage Component storage to query for Position and Collider
     * @return Vector of (Entity, Entity) pairs where first < second
     */
    virtual std::vector<std::pair<Entity, Entity>>
    detect(const ComponentStorage& storage) const = 0;

    /**
     * Return the number of pairwise comparisons performed during the most
     * recent detect() call. Default returns 0 for strategies that don't track.
     */
    virtual size_t last_pair_count() const { return 0; }

    /**
     * Return the number of narrow phase circle-circle checks performed
     * during the most recent detect() call.
     */
    virtual size_t last_narrow_cc() const { return 0; }

    /**
     * Return the number of narrow phase circle-AABB checks performed
     * during the most recent detect() call.
     */
    virtual size_t last_narrow_ca() const { return 0; }

    /**
     * Return the number of narrow phase AABB-AABB checks performed
     * during the most recent detect() call.
     */
    virtual size_t last_narrow_aa() const { return 0; }

    /**
     * Return the number of narrow phase OBB-OBB checks performed
     * during the most recent detect() call.
     */
    virtual size_t last_narrow_oo() const { return 0; }

    /**
     * Return the number of narrow phase OBB-circle checks performed
     * during the most recent detect() call.
     */
    virtual size_t last_narrow_oc() const { return 0; }

    /**
     * Return the number of narrow phase OBB-AABB checks performed
     * during the most recent detect() call.
     */
    virtual size_t last_narrow_oa() const { return 0; }
};

#endif // COLLISION_STRATEGY_HPP
