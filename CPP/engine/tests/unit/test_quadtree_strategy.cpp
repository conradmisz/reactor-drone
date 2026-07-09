/**
 * Unit tests for QuadtreeStrategy collision detection
 *
 * Edge cases: zero entities, one entity, same quadrant overlap, different
 * quadrants no overlap, boundary spanning, max_depth=0, large node_capacity.
 *
 * Requirements tested: 15.1, 15.2, 15.3, 15.4, 15.5, 15.6, 15.7
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/quadtree_strategy.hpp"
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include <algorithm>

// World bounds matching GameData.json
static constexpr float WORLD_X = -400.0f;
static constexpr float WORLD_Y = -300.0f;
static constexpr float WORLD_W = 800.0f;
static constexpr float WORLD_H = 600.0f;

// Compatible layers for all test entities
static constexpr uint8_t DEFAULT_LAYER = 1;
static constexpr uint8_t DEFAULT_MASK  = 1;

TEST_CASE("QuadtreeStrategy edge cases", "[collision][unit]") {
    EntityManager em;
    ComponentStorage storage;
    QuadtreeStrategy strategy(6, 8, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

    SECTION("Zero entities — Req 15.1") {
        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.empty());
        REQUIRE(strategy.last_pair_count() == 0);
    }

    SECTION("One entity — Req 15.2") {
        Entity a = em.create_entity();
        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{50.0f, 50.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.empty());
        REQUIRE(strategy.last_pair_count() == 0);
    }

    SECTION("Two overlapping entities in same quadrant — Req 15.3") {
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        // Both near center, well within one quadrant
        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{50.0f, 50.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.size() == 1);
        REQUIRE(pairs[0].first < pairs[0].second);
    }

    SECTION("Two non-overlapping entities in different quadrants — Req 15.4") {
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        // Place far apart in different quadrants
        storage.add_component(a, Position{-350.0f, -250.0f});
        storage.add_component(a, Collider{30.0f, 30.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{300.0f, 200.0f});
        storage.add_component(b, Collider{30.0f, 30.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.empty());
    }

    SECTION("Entity AABB spanning subdivision boundary — Req 15.5") {
        // Use small capacity to force subdivision, then place entity at boundary
        QuadtreeStrategy small_cap(6, 1, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);
        BruteForceStrategy brute_force;

        Entity a = em.create_entity();
        Entity b = em.create_entity();
        Entity c = em.create_entity();

        // a straddles the center boundary (x=0 is the midpoint of world)
        storage.add_component(a, Position{-25.0f, -25.0f});
        storage.add_component(a, Collider{50.0f, 50.0f, DEFAULT_LAYER, DEFAULT_MASK});

        // b is in the NE quadrant, overlapping with a
        storage.add_component(b, Position{10.0f, 10.0f});
        storage.add_component(b, Collider{30.0f, 30.0f, DEFAULT_LAYER, DEFAULT_MASK});

        // c is in the SW quadrant, overlapping with a
        storage.add_component(c, Position{-40.0f, -40.0f});
        storage.add_component(c, Collider{30.0f, 30.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto qt_pairs = small_cap.detect(storage);
        auto bf_pairs = brute_force.detect(storage);

        std::sort(qt_pairs.begin(), qt_pairs.end());
        std::sort(bf_pairs.begin(), bf_pairs.end());
        REQUIRE(qt_pairs == bf_pairs);
    }

    SECTION("Max_Depth = 0 (no subdivision) — Req 15.6") {
        QuadtreeStrategy no_subdiv(0, 1, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);
        BruteForceStrategy brute_force;

        Entity a = em.create_entity();
        Entity b = em.create_entity();
        Entity c = em.create_entity();

        storage.add_component(a, Position{-300.0f, -200.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{-250.0f, -150.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(c, Position{200.0f, 100.0f});
        storage.add_component(c, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto qt_pairs = no_subdiv.detect(storage);
        auto bf_pairs = brute_force.detect(storage);

        std::sort(qt_pairs.begin(), qt_pairs.end());
        std::sort(bf_pairs.begin(), bf_pairs.end());
        REQUIRE(qt_pairs == bf_pairs);
    }

    SECTION("Node_Capacity = 10000 (no subdivision triggered) — Req 15.7") {
        QuadtreeStrategy big_cap(6, 10000, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);
        BruteForceStrategy brute_force;

        Entity a = em.create_entity();
        Entity b = em.create_entity();
        Entity c = em.create_entity();

        storage.add_component(a, Position{-300.0f, -200.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{-250.0f, -150.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(c, Position{200.0f, 100.0f});
        storage.add_component(c, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto qt_pairs = big_cap.detect(storage);
        auto bf_pairs = brute_force.detect(storage);

        std::sort(qt_pairs.begin(), qt_pairs.end());
        std::sort(bf_pairs.begin(), bf_pairs.end());
        REQUIRE(qt_pairs == bf_pairs);
    }
}
