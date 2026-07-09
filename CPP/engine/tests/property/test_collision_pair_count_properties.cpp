/**
 * Property-based tests for collision pair counter
 *
 * These tests verify the brute-force pair count invariant (N*(N-1)/2)
 * and the Blackboard propagation of collision.pairs_checked.
 *
 * Feature: 060-01-setup-stress-test
 * Requirements tested: 9.2, 9.3, 9.4, 13.1, 13.2, 13.3, 14.4, 17.1
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/systems/collision_system.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include <set>

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

/**
 * Helper: create N entities with Position and Collider (compatible layers).
 * Entities are placed close together so all pairs are evaluated.
 */
static void create_collidable_entities(EntityManager& em, ComponentStorage& storage,
                                       int count, float base_x, float base_y) {
    for (int i = 0; i < count; ++i) {
        Entity e = em.create_entity();
        float x = base_x + static_cast<float>(i) * 10.0f;
        float y = base_y + static_cast<float>(i) * 10.0f;
        storage.add_component(e, Position{x, y});
        storage.add_component(e, Collider{1000.0f, 1000.0f, 1, 1});
    }
}

// ============================================================================
// Feature: 060-01-setup-stress-test, Property 4: Brute-force pair count invariant
//
// For any N in [0, 20], after calling detect() with N collidable entities,
// last_pair_count() == N*(N-1)/2. Returned pairs are normalized and unique.
//
// **Validates: Requirements 9.2, 9.3, 9.4, 14.4, 17.1**
// ============================================================================
TEST_CASE("Brute-force pair count invariant",
          "[property][Feature: 060-01-setup-stress-test, Property 4: Brute-force pair count invariant]") {
    SECTION("last_pair_count() equals N*(N-1)/2 for random N") {
        auto n = GENERATE(take(NUM_OUTER_TESTS, random(0, 20)));
        auto base_x = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy strategy;

        create_collidable_entities(em, storage, n, base_x, 0.0f);

        auto pairs = strategy.detect(storage);

        // Verify pair count invariant
        size_t expected = static_cast<size_t>(n) * static_cast<size_t>(n - 1) / 2;
        REQUIRE(strategy.last_pair_count() == expected);

        // Verify all pairs are normalized (first < second)
        for (const auto& [a, b] : pairs) {
            REQUIRE(a < b);
        }

        // Verify no duplicate pairs
        std::set<std::pair<Entity, Entity>> unique_pairs(pairs.begin(), pairs.end());
        REQUIRE(unique_pairs.size() == pairs.size());
    }
}

// ============================================================================
// Feature: 060-01-setup-stress-test, Property 5: Collision pair count Blackboard propagation
//
// For any N collidable entities, after CollisionSystem::update(),
// blackboard["collision.pairs_checked"] == N*(N-1)/2.
// Calling again with M entities updates to M*(M-1)/2 (not accumulated).
//
// **Validates: Requirements 13.1, 13.2, 13.3**
// ============================================================================
TEST_CASE("Collision pair count Blackboard propagation",
          "[property][Feature: 060-01-setup-stress-test, Property 5: Collision pair count Blackboard propagation]") {
    SECTION("Blackboard reflects N*(N-1)/2 after update, resets on second call") {
        auto n = GENERATE(take(NUM_OUTER_TESTS, random(0, 15)));
        auto m = GENERATE(take(NUM_INNER_TESTS, random(0, 10)));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy strategy;
        CollisionSystem system(strategy);
        Blackboard blackboard;

        // First call with N entities
        create_collidable_entities(em, storage, n, 0.0f, 0.0f);
        system.update(storage, blackboard);

        int expected_n = n * (n - 1) / 2;
        REQUIRE(blackboard.get<int>("collision.pairs_checked") == expected_n);

        // Remove all entities' Position and Collider to reset
        auto all_entities = storage.entities_with_component<Position>();
        for (Entity e : all_entities) {
            storage.remove_component<Position>(e);
            storage.remove_component<Collider>(e);
        }

        // Second call with M entities
        create_collidable_entities(em, storage, m, 100.0f, 100.0f);
        system.update(storage, blackboard);

        int expected_m = m * (m - 1) / 2;
        REQUIRE(blackboard.get<int>("collision.pairs_checked") == expected_m);
    }
}
