/**
 * Property-based tests for QuadtreeStrategy collision detection
 *
 * Four properties:
 *   1. Collision set equivalence with BruteForceStrategy
 *   2. Pair normalization and uniqueness
 *   3. Quadtree pair count bounded by brute force pair count
 *   4. Narrow phase stats consistency
 *
 * Requirements tested: 1.10, 2.5, 3.1, 4.1, 11.1, 12.1, 13.1, 14.1
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/systems/quadtree_strategy.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include <algorithm>
#include <cmath>
#include <set>

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;  // Number of different entity configurations
constexpr int NUM_INNER_TESTS = 5;   // Number of variations per configuration

// World bounds matching GameData.json
constexpr float WORLD_X = -400.0f;
constexpr float WORLD_Y = -300.0f;
constexpr float WORLD_W = 800.0f;
constexpr float WORLD_H = 600.0f;
constexpr int   QT_MAX_DEPTH = 6;
constexpr int   QT_NODE_CAPACITY = 8;

/**
 * Helper: populate N entities with random positions, colliders, and optionally
 * CircleColliders. ~50% of entities get CircleCollider based on index parity.
 * All entities use compatible layers (layer=1, mask=1).
 */
static void populate_mixed_entities(EntityManager& em, ComponentStorage& storage,
                                    int count, float seed_x, float seed_y) {
    for (int i = 0; i < count; ++i) {
        Entity e = em.create_entity();
        float x = WORLD_X + std::fmod(std::abs(seed_x * 7.3f + static_cast<float>(i) * 97.1f), WORLD_W);
        float y = WORLD_Y + std::fmod(std::abs(seed_y * 11.7f + static_cast<float>(i) * 53.3f), WORLD_H);
        float sz = 5.0f + std::fmod(std::abs(seed_x * 3.1f + seed_y * 2.7f + static_cast<float>(i) * 17.9f), 95.0f);

        storage.add_component(e, Position{x, y});
        storage.add_component(e, Collider{sz, sz, 1, 1});

        // ~50% of entities get CircleCollider (even indices)
        if (i % 2 == 0) {
            storage.add_component(e, CircleCollider{sz / 2.0f, 0.0f, 0.0f});
        }
    }
}

// Feature: 060-06-quadtree, Property 1: Collision set equivalence
// **Validates: Requirements 4.1, 11.1**
TEST_CASE("Quadtree collision set equivalence",
          "[Feature: 060-06-quadtree][Property 1: Collision set equivalence]") {
    SECTION("Quadtree produces identical collision pairs as brute force") {
        auto seed_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto seed_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        int n = static_cast<int>(std::fmod(std::abs(seed_x + seed_y), 21.0f));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy brute_force;
        QuadtreeStrategy quadtree(QT_MAX_DEPTH, QT_NODE_CAPACITY,
                                  WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

        populate_mixed_entities(em, storage, n, seed_x, seed_y);

        auto bf_pairs = brute_force.detect(storage);
        auto qt_pairs = quadtree.detect(storage);

        std::sort(bf_pairs.begin(), bf_pairs.end());
        std::sort(qt_pairs.begin(), qt_pairs.end());

        REQUIRE(bf_pairs == qt_pairs);
    }
}

// Feature: 060-06-quadtree, Property 2: Pair normalization and uniqueness
// **Validates: Requirements 1.10, 12.1**
TEST_CASE("Quadtree pair normalization and uniqueness",
          "[Feature: 060-06-quadtree][Property 2: Pair normalization and uniqueness]") {
    SECTION("Every pair (a, b) has a < b and no duplicates") {
        auto seed_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto seed_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        int n = static_cast<int>(std::fmod(std::abs(seed_x + seed_y), 21.0f));

        EntityManager em;
        ComponentStorage storage;
        QuadtreeStrategy quadtree(QT_MAX_DEPTH, QT_NODE_CAPACITY,
                                  WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

        populate_mixed_entities(em, storage, n, seed_x, seed_y);

        auto pairs = quadtree.detect(storage);

        for (const auto& [a, b] : pairs) {
            REQUIRE(a < b);
        }

        std::set<std::pair<Entity, Entity>> unique_pairs(pairs.begin(), pairs.end());
        REQUIRE(unique_pairs.size() == pairs.size());
    }
}

// Feature: 060-06-quadtree, Property 3: Quadtree pair count bounded by brute force
// **Validates: Requirements 3.1, 13.1**
TEST_CASE("Quadtree pair count bounded by brute force",
          "[Feature: 060-06-quadtree][Property 3: Quadtree pair count bounded by brute force]") {
    SECTION("Quadtree pair count <= brute force pair count") {
        auto seed_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto seed_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        int n = static_cast<int>(std::fmod(std::abs(seed_x + seed_y), 21.0f));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy brute_force;
        QuadtreeStrategy quadtree(QT_MAX_DEPTH, QT_NODE_CAPACITY,
                                  WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

        populate_mixed_entities(em, storage, n, seed_x, seed_y);

        brute_force.detect(storage);
        quadtree.detect(storage);

        REQUIRE(quadtree.last_pair_count() <= brute_force.last_pair_count());
    }
}

// Feature: 060-06-quadtree, Property 4: Narrow phase stats consistency
// **Validates: Requirements 2.5, 14.1**
TEST_CASE("Quadtree narrow phase stats consistency",
          "[Feature: 060-06-quadtree][Property 4: Narrow phase stats consistency]") {
    SECTION("Quadtree reports identical narrow phase counters as brute force") {
        auto seed_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto seed_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        // Use 2-15 entities to ensure some pairs exist
        int n = 2 + static_cast<int>(std::fmod(std::abs(seed_x + seed_y), 14.0f));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy brute_force;
        QuadtreeStrategy quadtree(QT_MAX_DEPTH, QT_NODE_CAPACITY,
                                  WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

        populate_mixed_entities(em, storage, n, seed_x, seed_y);

        brute_force.detect(storage);
        quadtree.detect(storage);

        REQUIRE(quadtree.last_narrow_cc() == brute_force.last_narrow_cc());
        REQUIRE(quadtree.last_narrow_ca() == brute_force.last_narrow_ca());
        REQUIRE(quadtree.last_narrow_aa() == brute_force.last_narrow_aa());
    }
}
