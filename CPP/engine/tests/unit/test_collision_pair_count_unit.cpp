/**
 * Unit tests for collision pair counter edge cases
 *
 * These tests verify pair count behavior for 0, 1, and 2 entities,
 * reset behavior across detect() calls, and the default strategy return.
 *
 * Feature: 060-01-setup-stress-test
 * Requirements tested: 9.3, 9.4, 14.4
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/systems/collision_strategy.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"

TEST_CASE("Collision pair counter edge cases", "[unit][Feature: 060-01-setup-stress-test]") {
    EntityManager em;
    ComponentStorage storage;
    BruteForceStrategy strategy;

    SECTION("detect() with 0 entities — pair_count = 0, empty results") {
        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.empty());
        REQUIRE(strategy.last_pair_count() == 0);
    }

    SECTION("detect() with 1 entity — pair_count = 0, empty results") {
        Entity a = em.create_entity();
        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{50.0f, 50.0f, 1, 1});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.empty());
        REQUIRE(strategy.last_pair_count() == 0);
    }

    SECTION("detect() with 2 entities (compatible layers, overlapping) — pair_count = 1") {
        Entity a = em.create_entity();
        Entity b = em.create_entity();
        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, 1, 1});
        storage.add_component(b, Position{50.0f, 50.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, 1, 1});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.size() == 1);
        REQUIRE(strategy.last_pair_count() == 1);
    }

    SECTION("last_pair_count() resets on each detect() call") {
        // Create 3 entities → 3 pairs
        for (int i = 0; i < 3; ++i) {
            Entity e = em.create_entity();
            storage.add_component(e, Position{static_cast<float>(i) * 10.0f, 0.0f});
            storage.add_component(e, Collider{1000.0f, 1000.0f, 1, 1});
        }

        strategy.detect(storage);
        REQUIRE(strategy.last_pair_count() == 3);

        // Remove all Position/Collider components so 0 entities are collidable
        auto entities = storage.entities_with_component<Position>();
        for (Entity e : entities) {
            storage.remove_component<Position>(e);
            storage.remove_component<Collider>(e);
        }

        strategy.detect(storage);
        REQUIRE(strategy.last_pair_count() == 0);
    }

    SECTION("Default CollisionStrategy::last_pair_count() returns 0") {
        // Create a minimal concrete strategy that only implements detect()
        struct MinimalStrategy : public CollisionStrategy {
            std::vector<std::pair<Entity, Entity>>
            detect(const ComponentStorage& /*storage*/) const override {
                return {};
            }
        };

        MinimalStrategy minimal;
        REQUIRE(minimal.last_pair_count() == 0);
    }
}
