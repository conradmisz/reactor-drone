#include "collision_system.hpp"
#include "engine/ecs/component_storage.hpp"
#include <unordered_map>

CollisionSystem::CollisionSystem(const CollisionStrategy& strategy)
    : strategy_(&strategy) {}

void CollisionSystem::set_strategy(const CollisionStrategy& strategy) {
    strategy_ = &strategy;
}

void CollisionSystem::update(ComponentStorage& storage, Blackboard& blackboard) {
    // 1. Clear all existing CollidedWith components from previous frame
    auto old_entities = storage.entities_with_component<CollidedWith>();
    for (Entity e : old_entities) {
        storage.remove_component<CollidedWith>(e);
    }

    // 2. Run collision detection via the strategy
    auto pairs = strategy_->detect(storage);

    // Write pair count to Blackboard for performance overlay
    blackboard.set("collision.pairs_checked", static_cast<int>(strategy_->last_pair_count()));

    // Write narrow phase stats to Blackboard
    blackboard.set("collision.narrow_circle_circle", static_cast<int>(strategy_->last_narrow_cc()));
    blackboard.set("collision.narrow_circle_aabb", static_cast<int>(strategy_->last_narrow_ca()));
    blackboard.set("collision.narrow_aabb_aabb", static_cast<int>(strategy_->last_narrow_aa()));

    // Write OBB narrow phase stats to Blackboard
    blackboard.set("collision.narrow_obb_obb", static_cast<int>(strategy_->last_narrow_oo()));
    blackboard.set("collision.narrow_obb_circle", static_cast<int>(strategy_->last_narrow_oc()));
    blackboard.set("collision.narrow_obb_aabb", static_cast<int>(strategy_->last_narrow_oa()));

    // 3. Accumulate collision partners per entity
    std::unordered_map<Entity, std::vector<Entity>> partners;
    for (const auto& [a, b] : pairs) {
        partners[a].push_back(b);
        partners[b].push_back(a);
    }

    // 4. Write one CollidedWith component per entity
    for (auto& [entity, entities] : partners) {
        storage.add_component<CollidedWith>(entity, CollidedWith{std::move(entities)});
    }
}
