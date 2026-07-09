/**
 * Property-based tests for UniformGridStrategy collision detection
 *
 * Three properties:
 *   1. Collision set equivalence with BruteForceStrategy
 *   2. Grid pair count bounded by brute force pair count
 *   3. Pair normalization and uniqueness
 *
 * Requirements tested: 1.5, 1.7, 2.1, 2.2, 2.4, 3.1, 11.1, 11.2
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/systems/uniform_grid_strategy.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include <algorithm>
#include <set>

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;  // Number of different entity configurations
constexpr int NUM_INNER_TESTS = 5;   // Number of variations per configuration

// World bounds matching GameData.json
constexpr float WORLD_X = -400.0f;
constexpr float WORLD_Y = -300.0f;
constexpr float WORLD_W = 800.0f;
constexpr float WORLD_H = 600.0f;
constexpr int   CELL_SIZE = 256;

/**
 * Helper: populate N entities with random positions and colliders.
 * All entities use compatible layers (layer=1, mask=1) so all pairs
 * are evaluated by both strategies, ensuring fair comparison.
 */
static void populate_random_entities(EntityManager& em, ComponentStorage& storage,
                                     int count, float seed_x, float seed_y) {
    for (int i = 0; i < count; ++i) {
        Entity e = em.create_entity();
        // Spread entities across world using seed offsets
        float x = WORLD_X + std::fmod(std::abs(seed_x * 7.3f + static_cast<float>(i) * 97.1f), WORLD_W);
        float y = WORLD_Y + std::fmod(std::abs(seed_y * 11.7f + static_cast<float>(i) * 53.3f), WORLD_H);
        float w = 5.0f + std::fmod(std::abs(seed_x * 3.1f + seed_y * 2.7f + static_cast<float>(i) * 17.9f), 95.0f);
        float h = 5.0f + std::fmod(std::abs(seed_y * 5.3f + seed_x * 1.9f + static_cast<float>(i) * 23.7f), 95.0f);
        storage.add_component(e, Position{x, y});
        storage.add_component(e, Collider{w, h, 1, 1});
    }
}

// Feature: 060-04-uniform-grid, Property 1: Collision set equivalence
// **Validates: Requirements 3.1, 11.1**
TEST_CASE("Uniform grid collision set equivalence",
          "[Feature: 060-04-uniform-grid][Property 1: Collision set equivalence]") {
    SECTION("Grid produces identical collision pairs as brute force") {
        auto seed_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto seed_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        // Vary entity count based on seeds (0 to 20)
        int n = static_cast<int>(std::fmod(std::abs(seed_x + seed_y), 21.0f));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy brute_force;
        UniformGridStrategy uniform_grid(CELL_SIZE, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

        populate_random_entities(em, storage, n, seed_x, seed_y);

        auto bf_pairs = brute_force.detect(storage);
        auto ug_pairs = uniform_grid.detect(storage);

        std::sort(bf_pairs.begin(), bf_pairs.end());
        std::sort(ug_pairs.begin(), ug_pairs.end());

        REQUIRE(bf_pairs == ug_pairs);
    }
}

// Feature: 060-04-uniform-grid, Property 2: Grid pair count is bounded by brute force pair count
// **Validates: Requirements 1.5, 2.1, 2.2, 2.4, 11.2**
TEST_CASE("Uniform grid pair count bounded by brute force",
          "[Feature: 060-04-uniform-grid][Property 2: Grid pair count is bounded by brute force pair count]") {
    SECTION("Grid pair count <= brute force pair count, and resets each call") {
        auto seed_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto seed_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        int n = static_cast<int>(std::fmod(std::abs(seed_x + seed_y), 21.0f));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy brute_force;
        UniformGridStrategy uniform_grid(CELL_SIZE, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

        populate_random_entities(em, storage, n, seed_x, seed_y);

        brute_force.detect(storage);
        uniform_grid.detect(storage);

        REQUIRE(uniform_grid.last_pair_count() <= brute_force.last_pair_count());

        // Verify pair count resets: run again with different entity count
        EntityManager em2;
        ComponentStorage storage2;
        int n2 = (n + 3) % 21;  // Different count
        populate_random_entities(em2, storage2, n2, seed_y, seed_x);

        brute_force.detect(storage2);
        uniform_grid.detect(storage2);

        // pair_count reflects only the latest call
        REQUIRE(uniform_grid.last_pair_count() <= brute_force.last_pair_count());
    }
}

// Feature: 060-04-uniform-grid, Property 3: Pair normalization and uniqueness
// **Validates: Requirements 1.7**
TEST_CASE("Uniform grid pair normalization and uniqueness",
          "[Feature: 060-04-uniform-grid][Property 3: Pair normalization and uniqueness]") {
    SECTION("Every pair (a, b) has a < b and no duplicates") {
        auto seed_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto seed_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        int n = static_cast<int>(std::fmod(std::abs(seed_x + seed_y), 21.0f));

        EntityManager em;
        ComponentStorage storage;
        UniformGridStrategy uniform_grid(CELL_SIZE, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

        populate_random_entities(em, storage, n, seed_x, seed_y);

        auto pairs = uniform_grid.detect(storage);

        // Every pair normalized: a < b
        for (const auto& [a, b] : pairs) {
            REQUIRE(a < b);
        }

        // No duplicates
        std::set<std::pair<Entity, Entity>> unique_pairs(pairs.begin(), pairs.end());
        REQUIRE(unique_pairs.size() == pairs.size());
    }
}
