#ifndef BRUTE_FORCE_STRATEGY_HPP
#define BRUTE_FORCE_STRATEGY_HPP

#include "collision_strategy.hpp"

/**
 * Brute force collision detection — O(N²) pairwise comparison.
 *
 * Queries ComponentStorage for all entities with both Position and Collider.
 * For each unique pair, applies layer/mask filter first (cheap), then AABB
 * overlap test. Pairs are normalized (smaller ID first) and deduplicated.
 */
class BruteForceStrategy : public CollisionStrategy {
public:
    std::vector<std::pair<Entity, Entity>>
    detect(const ComponentStorage& storage) const override;

    size_t last_pair_count() const override;

    size_t last_narrow_cc() const override;
    size_t last_narrow_ca() const override;
    size_t last_narrow_aa() const override;

    size_t last_narrow_oo() const override;
    size_t last_narrow_oc() const override;
    size_t last_narrow_oa() const override;

private:
    mutable size_t pair_count_ = 0;
    mutable size_t narrow_cc_ = 0, narrow_ca_ = 0, narrow_aa_ = 0;
    mutable size_t narrow_oo_ = 0, narrow_oc_ = 0, narrow_oa_ = 0;
};

#endif // BRUTE_FORCE_STRATEGY_HPP
