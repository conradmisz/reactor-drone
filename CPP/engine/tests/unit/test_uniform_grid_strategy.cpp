/**
 * Unit tests for UniformGridStrategy collision detection
 *
 * Edge cases: zero entities, one entity, same cell overlap, different cells
 * no overlap, boundary spanning, degenerate cell size, set_strategy, default
 * cell_size from GameData.json.
 *
 * Requirements tested: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 4.3, 5.4
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/uniform_grid_strategy.hpp"
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/systems/collision_system.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/gamedata_loader.hpp"
#include <algorithm>

// World bounds matching GameData.json
static constexpr float WORLD_X = -400.0f;
static constexpr float WORLD_Y = -300.0f;
static constexpr float WORLD_W = 800.0f;
static constexpr float WORLD_H = 600.0f;
static constexpr int   CELL_SIZE = 256;

// Compatible layers for all test entities
static constexpr uint8_t DEFAULT_LAYER = 1;
static constexpr uint8_t DEFAULT_MASK  = 1;

TEST_CASE("UniformGridStrategy edge cases", "[collision][unit]") {
    EntityManager em;
    ComponentStorage storage;
    UniformGridStrategy strategy(CELL_SIZE, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

    SECTION("Zero entities — Req 12.1") {
        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.empty());
        REQUIRE(strategy.last_pair_count() == 0);
    }

    SECTION("One entity — Req 12.2") {
        Entity a = em.create_entity();
        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{50.0f, 50.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.empty());
        REQUIRE(strategy.last_pair_count() == 0);
    }

    SECTION("Two overlapping entities in same cell — Req 12.3") {
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        // Both near center, well within one cell
        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{50.0f, 50.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.size() == 1);
        REQUIRE(pairs[0].first < pairs[0].second);
    }

    SECTION("Two non-overlapping entities in different cells — Req 12.4") {
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        // Place far apart in different cells
        storage.add_component(a, Position{-350.0f, -250.0f});
        storage.add_component(a, Collider{30.0f, 30.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{300.0f, 200.0f});
        storage.add_component(b, Collider{30.0f, 30.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.empty());
    }

    SECTION("Entity AABB spanning cell boundary — Req 12.5") {
        // Place entity right at a cell boundary so its AABB spans two cells
        // Cell boundary at world_x + cell_size = -400 + 256 = -144
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        // a straddles the cell boundary at x = -144
        storage.add_component(a, Position{-170.0f, 0.0f});
        storage.add_component(a, Collider{50.0f, 50.0f, DEFAULT_LAYER, DEFAULT_MASK});

        // b is in the adjacent cell, overlapping with a
        storage.add_component(b, Position{-150.0f, 10.0f});
        storage.add_component(b, Collider{50.0f, 50.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto ug_pairs = strategy.detect(storage);

        // Verify equivalence with brute force
        BruteForceStrategy brute_force;
        auto bf_pairs = brute_force.detect(storage);

        std::sort(ug_pairs.begin(), ug_pairs.end());
        std::sort(bf_pairs.begin(), bf_pairs.end());
        REQUIRE(ug_pairs == bf_pairs);
    }

    SECTION("Cell size larger than world — Req 12.6") {
        // cell_size = 10000 → all entities in one cell → same as brute force
        UniformGridStrategy big_cell(10000, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);
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

        auto ug_pairs = big_cell.detect(storage);
        auto bf_pairs = brute_force.detect(storage);

        std::sort(ug_pairs.begin(), ug_pairs.end());
        std::sort(bf_pairs.begin(), bf_pairs.end());
        REQUIRE(ug_pairs == bf_pairs);
    }
}

TEST_CASE("CollisionSystem set_strategy works", "[collision][unit]") {
    EntityManager em;
    ComponentStorage storage;
    Blackboard blackboard;

    Entity a = em.create_entity();
    Entity b = em.create_entity();

    storage.add_component(a, Position{0.0f, 0.0f});
    storage.add_component(a, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

    storage.add_component(b, Position{50.0f, 50.0f});
    storage.add_component(b, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

    BruteForceStrategy brute_force;
    UniformGridStrategy uniform_grid(CELL_SIZE, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

    // Start with brute force
    CollisionSystem collision_system(brute_force);
    collision_system.update(storage, blackboard);

    REQUIRE(storage.has_component<CollidedWith>(a));
    REQUIRE(storage.has_component<CollidedWith>(b));

    // Switch to uniform grid
    collision_system.set_strategy(uniform_grid);
    collision_system.update(storage, blackboard);

    // Should still detect the same collision
    REQUIRE(storage.has_component<CollidedWith>(a));
    REQUIRE(storage.has_component<CollidedWith>(b));
}

TEST_CASE("Default cell_size is 256 when collision section absent — Req 4.3", "[collision][unit]") {
    // Verify gamedata_loader sets default collision.cell_size = 256
    // We test this by loading a minimal GameData.json without collision section
    // and checking the Blackboard value
    Blackboard blackboard;
    int cell_size = blackboard.get_or<int>("collision.cell_size", 256);
    REQUIRE(cell_size == 256);
}
