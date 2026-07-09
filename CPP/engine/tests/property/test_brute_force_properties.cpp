/**
 * Property-based tests for BruteForceStrategy collision detection
 *
 * These tests verify structural invariants of the brute force strategy:
 * pair normalization, uniqueness, and soundness (no false positives).
 *
 * Requirements tested: 4.5, 4.6, 12.1, 12.2, 12.3
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/systems/collision_math.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include <set>

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;  // Number of different entity configurations
constexpr int NUM_INNER_TESTS = 5;   // Number of variations per configuration

/**
 * Helper: create N entities with random Position and Collider components.
 * Uses deterministic offsets from the seed values to avoid nested GENERATE.
 */
static void populate_entities(EntityManager& em, ComponentStorage& storage,
                              int count, float base_x, float base_y) {
    for (int i = 0; i < count; ++i) {
        Entity e = em.create_entity();
        float x = base_x + static_cast<float>(i) * 30.0f;
        float y = base_y + static_cast<float>(i) * 20.0f;
        storage.add_component(e, Position{x, y});
        storage.add_component(e, Collider{100.0f, 100.0f, 1, 1});
    }
}

// Feature: 050-02-aabb-collision-detection, Property 5: Brute force pair normalization
// **Validates: Requirements 4.5, 12.1**
TEST_CASE("Brute force pair normalization", "[collision][property]") {
    SECTION("Every pair (a, b) satisfies a < b") {
        auto base_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto base_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy strategy;

        populate_entities(em, storage, 5, base_x, base_y);
        auto pairs = strategy.detect(storage);

        for (const auto& [a, b] : pairs) {
            REQUIRE(a < b);
        }
    }
}

// Feature: 050-02-aabb-collision-detection, Property 6: Brute force pair uniqueness
// **Validates: Requirements 4.6, 12.2**
TEST_CASE("Brute force pair uniqueness", "[collision][property]") {
    SECTION("No two pairs are identical") {
        auto base_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto base_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy strategy;

        populate_entities(em, storage, 5, base_x, base_y);
        auto pairs = strategy.detect(storage);

        std::set<std::pair<Entity, Entity>> unique_pairs(pairs.begin(), pairs.end());
        REQUIRE(unique_pairs.size() == pairs.size());
    }
}

// Feature: 050-02-aabb-collision-detection, Property 7: Brute force soundness (no false positives)
// **Validates: Requirements 12.3, 4.4**
TEST_CASE("Brute force soundness — no false positives", "[collision][property]") {
    SECTION("Every reported pair passes layers_compatible and aabb_overlap") {
        auto base_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto base_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy strategy;

        populate_entities(em, storage, 5, base_x, base_y);
        auto pairs = strategy.detect(storage);

        for (const auto& [a, b] : pairs) {
            // Both entities must have Position and Collider
            REQUIRE(storage.has_component<Position>(a));
            REQUIRE(storage.has_component<Collider>(a));
            REQUIRE(storage.has_component<Position>(b));
            REQUIRE(storage.has_component<Collider>(b));

            auto pos_a = storage.get_component<Position>(a);
            auto col_a = storage.get_component<Collider>(a);
            auto pos_b = storage.get_component<Position>(b);
            auto col_b = storage.get_component<Collider>(b);

            // Must pass layer/mask filter
            REQUIRE(layers_compatible(col_a->get().layer, col_a->get().mask,
                                      col_b->get().layer, col_b->get().mask));

            // Must pass AABB overlap test
            REQUIRE(aabb_overlap(pos_a->get().x, pos_a->get().y,
                                 col_a->get().width, col_a->get().height,
                                 pos_b->get().x, pos_b->get().y,
                                 col_b->get().width, col_b->get().height));
        }
    }
}
