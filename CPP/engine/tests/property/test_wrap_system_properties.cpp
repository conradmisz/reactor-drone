/**
 * Property-based tests for WrapSystem
 *
 * These tests verify universal invariants of the WrapSystem:
 * identity for inside entities, post-condition overlap, and non-interference.
 *
 * Requirements tested: 14.5, 14.6, 1.2, 1.3, 1.4, 1.5, 2.1, 2.2
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/systems/wrap_system.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

// Fixed world bounds for all property tests
static constexpr float WORLD_X = -400.0f;
static constexpr float WORLD_Y = -300.0f;
static constexpr float WORLD_W = 800.0f;
static constexpr float WORLD_H = 600.0f;

static Blackboard make_blackboard() {
    Blackboard bb;
    bb.set<float>("world.x", WORLD_X);
    bb.set<float>("world.y", WORLD_Y);
    bb.set<float>("world.width", WORLD_W);
    bb.set<float>("world.height", WORLD_H);
    return bb;
}

// Feature: 050-03-wrap-and-lifetime-systems, Property 1: WrapSystem identity
// **Validates: Requirements 14.5**
TEST_CASE("WrapSystem identity — inside entities unchanged", "[wrap][property]") {
    SECTION("Entities inside world bounds are not moved") {
        // For a 50x50 entity, valid x range is [-400, 350] and y range is [-300, 250]
        // so that the entire bounding box stays inside the world
        auto pos_x = GENERATE(take(NUM_OUTER_TESTS, random(-400.0f, 350.0f)));
        auto pos_y = GENERATE(take(NUM_INNER_TESTS, random(-300.0f, 250.0f)));

        EntityManager em;
        ComponentStorage storage;
        Blackboard bb = make_blackboard();
        WrapSystem wrap_system;

        Entity e = em.create_entity();
        storage.add_component(e, Position{pos_x, pos_y});
        storage.add_component(e, Size{50.0f, 50.0f});
        storage.add_component(e, WrapAround{});

        wrap_system.update(storage, bb);

        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());
        REQUIRE(pos->get().x == pos_x);
        REQUIRE(pos->get().y == pos_y);
    }
}

// Feature: 050-03-wrap-and-lifetime-systems, Property 2: WrapSystem post-condition
// **Validates: Requirements 14.6, 1.2, 1.3, 1.4, 1.5**
TEST_CASE("WrapSystem post-condition — wrapped entity overlaps world", "[wrap][property]") {
    SECTION("Entity bounding box overlaps world after wrapping") {
        auto pos_x = GENERATE(take(NUM_OUTER_TESTS, random(-2000.0f, 2000.0f)));
        auto pos_y = GENERATE(take(NUM_INNER_TESTS, random(-2000.0f, 2000.0f)));
        auto size_w = GENERATE(take(1, random(1.0f, 100.0f)));
        auto size_h = GENERATE(take(1, random(1.0f, 100.0f)));

        EntityManager em;
        ComponentStorage storage;
        Blackboard bb = make_blackboard();
        WrapSystem wrap_system;

        Entity e = em.create_entity();
        storage.add_component(e, Position{pos_x, pos_y});
        storage.add_component(e, Size{size_w, size_h});
        storage.add_component(e, WrapAround{});

        wrap_system.update(storage, bb);

        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());

        float ex = pos->get().x;
        float ey = pos->get().y;

        // Overlap check: entity bounding box touches or overlaps world bounds
        // Edge-aligned placement means the entity's edge may exactly equal the
        // world boundary, so we use >= / <= rather than strict > / <
        bool overlaps_x = (ex + size_w >= WORLD_X) && (ex <= WORLD_X + WORLD_W);
        bool overlaps_y = (ey + size_h >= WORLD_Y) && (ey <= WORLD_Y + WORLD_H);
        REQUIRE(overlaps_x);
        REQUIRE(overlaps_y);
    }
}

// Feature: 050-03-wrap-and-lifetime-systems, Property 3: WrapSystem non-interference
// **Validates: Requirements 2.1, 2.2**
TEST_CASE("WrapSystem non-interference — non-WrapAround entities untouched", "[wrap][property]") {
    SECTION("Entities without WrapAround are not moved") {
        auto pos_x = GENERATE(take(NUM_OUTER_TESTS, random(-2000.0f, 2000.0f)));
        auto pos_y = GENERATE(take(NUM_INNER_TESTS, random(-2000.0f, 2000.0f)));

        EntityManager em;
        ComponentStorage storage;
        Blackboard bb = make_blackboard();
        WrapSystem wrap_system;

        Entity e = em.create_entity();
        storage.add_component(e, Position{pos_x, pos_y});
        storage.add_component(e, Size{50.0f, 50.0f});
        // No WrapAround component

        wrap_system.update(storage, bb);

        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());
        REQUIRE(pos->get().x == pos_x);
        REQUIRE(pos->get().y == pos_y);
    }
}
