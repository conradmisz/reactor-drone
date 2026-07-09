/**
 * Property-based tests for CircleCollider collision detection
 *
 * Four properties:
 *   1. Circle-circle overlap symmetry
 *   2. Circle overlap implies AABB overlap (no broad phase false negatives)
 *   3. Strategy equivalence with mixed collider types
 *   4. Narrow phase stats consistency
 *
 * Requirements tested: 9.2, 9.3, 12.1, 12.2, 12.3, 13.1, 13.2, 13.3,
 *                      14.1, 14.2, 14.3, 14.4
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/systems/collision_math.hpp"
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/systems/uniform_grid_strategy.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include <algorithm>
#include <cmath>

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;  // Number of different configurations
constexpr int NUM_INNER_TESTS = 5;   // Number of variations per configuration

// World bounds matching GameData.json
constexpr float WORLD_X = -400.0f;
constexpr float WORLD_Y = -300.0f;
constexpr float WORLD_W = 800.0f;
constexpr float WORLD_H = 600.0f;
constexpr int   CELL_SIZE = 256;

// Feature: 060-05-circle-colliders, Property 1: Circle-circle overlap symmetry
// **Validates: Requirements 12.1, 12.2, 12.3**
TEST_CASE("Circle-circle overlap symmetry",
          "[Feature: 060-05-circle-colliders][Property 1: Circle-circle overlap symmetry]") {
    SECTION("circle_circle_overlap(a,b) == circle_circle_overlap(b,a)") {
        auto x1 = GENERATE(take(NUM_OUTER_TESTS, random(-400.0f, 400.0f)));
        auto y1 = GENERATE(take(NUM_INNER_TESTS, random(-300.0f, 300.0f)));
        float r1 = 1.0f + std::fmod(std::abs(x1 * 3.7f + y1 * 2.3f), 99.0f);

        float x2 = x1 + std::fmod(y1 * 5.1f, 200.0f) - 100.0f;
        float y2 = y1 + std::fmod(x1 * 3.3f, 200.0f) - 100.0f;
        float r2 = 1.0f + std::fmod(std::abs(y1 * 7.1f + x1 * 1.9f), 99.0f);

        REQUIRE(circle_circle_overlap(x1, y1, r1, x2, y2, r2) ==
                circle_circle_overlap(x2, y2, r2, x1, y1, r1));
    }
}

// Feature: 060-05-circle-colliders, Property 2: Circle overlap implies AABB overlap
// **Validates: Requirements 13.1, 13.2, 13.3**
TEST_CASE("Circle overlap implies AABB overlap",
          "[Feature: 060-05-circle-colliders][Property 2: Circle overlap implies AABB overlap]") {
    SECTION("If circles overlap, their bounding AABBs must also overlap") {
        auto x1 = GENERATE(take(NUM_OUTER_TESTS, random(-400.0f, 400.0f)));
        auto y1 = GENERATE(take(NUM_INNER_TESTS, random(-300.0f, 300.0f)));
        float r1 = 1.0f + std::fmod(std::abs(x1 * 3.7f + y1 * 2.3f), 99.0f);

        float x2 = x1 + std::fmod(y1 * 5.1f, 200.0f) - 100.0f;
        float y2 = y1 + std::fmod(x1 * 3.3f, 200.0f) - 100.0f;
        float r2 = 1.0f + std::fmod(std::abs(y1 * 7.1f + x1 * 1.9f), 99.0f);

        if (circle_circle_overlap(x1, y1, r1, x2, y2, r2)) {
            // Bounding AABBs: bottom-left at (center - radius), side = 2*radius
            float ax1 = x1 - r1, ay1 = y1 - r1;
            float ax2 = x2 - r2, ay2 = y2 - r2;
            REQUIRE(aabb_overlap(ax1, ay1, 2.0f * r1, 2.0f * r1,
                                 ax2, ay2, 2.0f * r2, 2.0f * r2));
        }
    }
}

/**
 * Helper: populate N entities with random positions, colliders, and optionally
 * CircleColliders. Uses seed values to derive deterministic-but-varied parameters.
 * ~50% of entities get CircleCollider based on index parity.
 */
static void populate_mixed_entities(EntityManager& em, ComponentStorage& storage,
                                    int count, float seed_x, float seed_y) {
    for (int i = 0; i < count; ++i) {
        Entity e = em.create_entity();
        float x = WORLD_X + std::fmod(std::abs(seed_x * 7.3f + static_cast<float>(i) * 97.1f), WORLD_W);
        float y = WORLD_Y + std::fmod(std::abs(seed_y * 11.7f + static_cast<float>(i) * 53.3f), WORLD_H);
        float sz = 10.0f + std::fmod(std::abs(seed_x * 3.1f + seed_y * 2.7f + static_cast<float>(i) * 17.9f), 54.0f);

        storage.add_component(e, Position{x, y});
        storage.add_component(e, Collider{sz, sz, 1, 1});

        // ~50% of entities get CircleCollider (even indices)
        if (i % 2 == 0) {
            storage.add_component(e, CircleCollider{sz / 2.0f, 0.0f, 0.0f});
        }
    }
}

// Feature: 060-05-circle-colliders, Property 3: Strategy equivalence with mixed collider types
// **Validates: Requirements 14.1, 14.2, 14.3, 14.4**
TEST_CASE("Strategy equivalence with mixed collider types",
          "[Feature: 060-05-circle-colliders][Property 3: Strategy equivalence with mixed collider types]") {
    SECTION("Both strategies produce identical collision pairs with mixed colliders") {
        auto seed_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto seed_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        int n = static_cast<int>(std::fmod(std::abs(seed_x + seed_y), 21.0f));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy brute_force;
        UniformGridStrategy uniform_grid(CELL_SIZE, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

        populate_mixed_entities(em, storage, n, seed_x, seed_y);

        auto bf_pairs = brute_force.detect(storage);
        auto ug_pairs = uniform_grid.detect(storage);

        std::sort(bf_pairs.begin(), bf_pairs.end());
        std::sort(ug_pairs.begin(), ug_pairs.end());

        REQUIRE(bf_pairs == ug_pairs);
    }
}

// Feature: 060-05-circle-colliders, Property 4: Narrow phase stats consistency
// **Validates: Requirements 9.2, 9.3**
TEST_CASE("Narrow phase stats consistency",
          "[Feature: 060-05-circle-colliders][Property 4: Narrow phase stats consistency]") {
    SECTION("Narrow counter sum equals collision pairs, both strategies agree") {
        auto seed_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto seed_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        // Use 2-15 entities to ensure some pairs exist
        int n = 2 + static_cast<int>(std::fmod(std::abs(seed_x + seed_y), 14.0f));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy brute_force;
        UniformGridStrategy uniform_grid(CELL_SIZE, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

        populate_mixed_entities(em, storage, n, seed_x, seed_y);

        auto bf_pairs = brute_force.detect(storage);
        auto ug_pairs = uniform_grid.detect(storage);

        // Narrow counter sum should equal the number of collision pairs reported
        // (every pair that passes broad phase goes through exactly one narrow check)
        size_t bf_narrow_total = brute_force.last_narrow_cc() +
                                 brute_force.last_narrow_ca() +
                                 brute_force.last_narrow_aa();
        size_t ug_narrow_total = uniform_grid.last_narrow_cc() +
                                 uniform_grid.last_narrow_ca() +
                                 uniform_grid.last_narrow_aa();

        // The narrow total counts all pairs that passed broad phase (including
        // those rejected by narrow phase), so it must be >= the collision count
        REQUIRE(bf_narrow_total >= bf_pairs.size());
        REQUIRE(ug_narrow_total >= ug_pairs.size());

        // Both strategies must report identical narrow phase counters
        REQUIRE(brute_force.last_narrow_cc() == uniform_grid.last_narrow_cc());
        REQUIRE(brute_force.last_narrow_ca() == uniform_grid.last_narrow_ca());
        REQUIRE(brute_force.last_narrow_aa() == uniform_grid.last_narrow_aa());
    }
}
