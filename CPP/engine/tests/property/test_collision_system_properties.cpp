/**
 * Property-based tests for CollisionSystem pair-to-component translation
 *
 * These tests verify that the CollisionSystem correctly translates
 * BruteForceStrategy collision pairs into per-entity CollidedWith components.
 *
 * Properties tested:
 *   2: Completeness and bidirectionality
 *   3: Soundness — no phantom collisions
 *   4: No duplicate entity IDs
 *   5: Frame clearing
 *   6: Destruction pipeline removes CollidedWith
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/systems/collision_system.hpp"
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/destruction.hpp"
#include <set>
#include <map>
#include <algorithm>

constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

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

// Feature: 050-04-make-collision-pure-ecs, Property 2: Completeness and bidirectionality
// **Validates: Requirements 3.2, 3.3, 3.4, 9.5, 9.7**
TEST_CASE("Feature: 050-04-make-collision-pure-ecs, Property 2: Completeness and bidirectionality",
          "[collision][property]") {
    SECTION("Every strategy pair (A,B) results in B in A's CollidedWith and A in B's CollidedWith") {
        auto base_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto base_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy strategy;
        CollisionSystem system(strategy);
        Blackboard blackboard;

        populate_entities(em, storage, 5, base_x, base_y);

        // Run the CollisionSystem
        system.update(storage, blackboard);

        // Run the strategy separately to get the expected pairs
        auto pairs = strategy.detect(storage);

        for (const auto& [a, b] : pairs) {
            // A must have CollidedWith containing B
            REQUIRE(storage.has_component<CollidedWith>(a));
            auto cw_a = storage.get_component<CollidedWith>(a);
            auto& vec_a = cw_a->get().entities;
            REQUIRE(std::find(vec_a.begin(), vec_a.end(), b) != vec_a.end());

            // B must have CollidedWith containing A
            REQUIRE(storage.has_component<CollidedWith>(b));
            auto cw_b = storage.get_component<CollidedWith>(b);
            auto& vec_b = cw_b->get().entities;
            REQUIRE(std::find(vec_b.begin(), vec_b.end(), a) != vec_b.end());
        }
    }
}


// Feature: 050-04-make-collision-pure-ecs, Property 3: Soundness — no phantom collisions
// **Validates: Requirements 9.6**
TEST_CASE("Feature: 050-04-make-collision-pure-ecs, Property 3: Soundness — no phantom collisions",
          "[collision][property]") {
    SECTION("Every entity ID in CollidedWith corresponds to a real strategy pair") {
        auto base_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto base_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy strategy;
        CollisionSystem system(strategy);
        Blackboard blackboard;

        populate_entities(em, storage, 5, base_x, base_y);

        // Run the CollisionSystem
        system.update(storage, blackboard);

        // Run the strategy separately to get the expected pairs
        auto pairs = strategy.detect(storage);

        // Build a set of normalized pairs from strategy output
        std::set<std::pair<Entity, Entity>> pair_set;
        for (const auto& [a, b] : pairs) {
            pair_set.insert({std::min(a, b), std::max(a, b)});
        }

        // For every entity with CollidedWith, verify every ID is a real pair
        auto collided_entities = storage.entities_with_component<CollidedWith>();
        for (Entity e : collided_entities) {
            auto cw = storage.get_component<CollidedWith>(e);
            for (Entity other : cw->get().entities) {
                auto normalized = std::make_pair(std::min(e, other), std::max(e, other));
                REQUIRE(pair_set.count(normalized) == 1);
            }
        }
    }
}

// Feature: 050-04-make-collision-pure-ecs, Property 4: No duplicate entity IDs
// **Validates: Requirements 9.8**
TEST_CASE("Feature: 050-04-make-collision-pure-ecs, Property 4: No duplicate entity IDs",
          "[collision][property]") {
    SECTION("No CollidedWith vector contains duplicate IDs") {
        auto base_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto base_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy strategy;
        CollisionSystem system(strategy);
        Blackboard blackboard;

        populate_entities(em, storage, 5, base_x, base_y);

        // Run the CollisionSystem
        system.update(storage, blackboard);

        // For every entity with CollidedWith, check for duplicates
        auto collided_entities = storage.entities_with_component<CollidedWith>();
        for (Entity e : collided_entities) {
            auto cw = storage.get_component<CollidedWith>(e);
            auto& vec = cw->get().entities;
            std::set<Entity> unique_ids(vec.begin(), vec.end());
            REQUIRE(unique_ids.size() == vec.size());
        }
    }
}

// Feature: 050-04-make-collision-pure-ecs, Property 5: Frame clearing
// **Validates: Requirements 3.1, 10.1**
TEST_CASE("Feature: 050-04-make-collision-pure-ecs, Property 5: Frame clearing",
          "[collision][property]") {
    SECTION("After moving entities far apart, second update produces no CollidedWith") {
        auto base_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto base_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy strategy;
        CollisionSystem system(strategy);
        Blackboard blackboard;

        populate_entities(em, storage, 5, base_x, base_y);

        // Frame 1: run collision (may produce CollidedWith components)
        system.update(storage, blackboard);

        // Move all entities far away so no collisions occur
        auto all_entities = storage.entities_with_component<Position>();
        for (Entity e : all_entities) {
            auto pos = storage.get_component<Position>(e);
            pos->get().x += 10000.0f;
            pos->get().y += 10000.0f;
            // Spread them out so they don't collide with each other
            pos->get().x += static_cast<float>(e) * 10000.0f;
        }

        // Frame 2: run collision again
        system.update(storage, blackboard);

        // Verify no entities have CollidedWith after frame 2
        auto collided_entities = storage.entities_with_component<CollidedWith>();
        REQUIRE(collided_entities.empty());
    }
}

// Feature: 050-04-make-collision-pure-ecs, Property 6: Destruction pipeline removes CollidedWith
// **Validates: Requirements 5.1**
TEST_CASE("Feature: 050-04-make-collision-pure-ecs, Property 6: Destruction pipeline removes CollidedWith",
          "[collision][property]") {
    SECTION("Destroyed entities lose CollidedWith, survivors keep theirs") {
        auto base_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto base_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy strategy;
        CollisionSystem system(strategy);
        Blackboard blackboard;

        populate_entities(em, storage, 5, base_x, base_y);

        // Run collision to get some CollidedWith components
        system.update(storage, blackboard);

        // Snapshot which entities have CollidedWith and their contents
        auto collided_before = storage.entities_with_component<CollidedWith>();

        // If no collisions occurred, skip this iteration (nothing to test)
        if (collided_before.empty()) {
            return;
        }

        // Mark the first collided entity for destruction
        Entity to_destroy = collided_before.front();
        storage.add_component(to_destroy, DestroyRequest{});

        // Snapshot surviving entities' CollidedWith before destruction
        std::map<Entity, std::vector<Entity>> survivor_cw;
        for (Entity e : collided_before) {
            if (e != to_destroy) {
                auto cw = storage.get_component<CollidedWith>(e);
                survivor_cw[e] = cw->get().entities;
            }
        }

        // Run destruction pipeline
        destroy_marked_entities(em, storage);

        // Verify destroyed entity has no CollidedWith
        REQUIRE_FALSE(storage.has_component<CollidedWith>(to_destroy));

        // Verify surviving entities' CollidedWith is unchanged
        for (const auto& [entity, expected_vec] : survivor_cw) {
            REQUIRE(storage.has_component<CollidedWith>(entity));
            auto cw = storage.get_component<CollidedWith>(entity);
            REQUIRE(cw->get().entities == expected_vec);
        }
    }
}
