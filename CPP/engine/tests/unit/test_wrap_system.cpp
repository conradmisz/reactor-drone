/**
 * Unit tests for WrapSystem
 *
 * These tests verify the WrapSystem wraps entities correctly at each of the
 * four world edges, leaves inside entities unchanged, and does not affect
 * entities without the WrapAround component.
 *
 * Requirements tested: 11.1, 11.2, 11.3, 11.4, 11.5, 11.6
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/wrap_system.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

// World bounds: centered at origin, 800x600
static constexpr float WORLD_X = -400.0f;
static constexpr float WORLD_Y = -300.0f;
static constexpr float WORLD_W = 800.0f;
static constexpr float WORLD_H = 600.0f;

// Entity size for all tests
static constexpr float ENTITY_W = 50.0f;
static constexpr float ENTITY_H = 50.0f;

static Blackboard make_blackboard() {
    Blackboard bb;
    bb.set<float>("world.x", WORLD_X);
    bb.set<float>("world.y", WORLD_Y);
    bb.set<float>("world.width", WORLD_W);
    bb.set<float>("world.height", WORLD_H);
    return bb;
}

TEST_CASE("WrapSystem edge wrapping", "[wrap][unit]") {
    EntityManager em;
    ComponentStorage storage;
    Blackboard bb = make_blackboard();
    WrapSystem wrap_system;

    SECTION("WrapRightEdge — Req 11.1") {
        // Entity at pos.x=400 (left edge at world right edge: -400+800=400)
        // Should wrap to pos.x = world_x - w = -400 - 50 = -450
        Entity e = em.create_entity();
        storage.add_component(e, Position{400.0f, 0.0f});
        storage.add_component(e, Size{ENTITY_W, ENTITY_H});
        storage.add_component(e, WrapAround{});

        wrap_system.update(storage, bb);

        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());
        REQUIRE(pos->get().x == -450.0f);
    }

    SECTION("WrapLeftEdge — Req 11.2") {
        // Entity at pos.x=-451, right edge = -451+50 = -401 <= -400
        // Should wrap to pos.x = world_x + world_w = -400 + 800 = 400
        Entity e = em.create_entity();
        storage.add_component(e, Position{-451.0f, 0.0f});
        storage.add_component(e, Size{ENTITY_W, ENTITY_H});
        storage.add_component(e, WrapAround{});

        wrap_system.update(storage, bb);

        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());
        REQUIRE(pos->get().x == 400.0f);
    }

    SECTION("WrapTopEdge — Req 11.3") {
        // Entity at pos.y=300 (bottom edge at world top edge: -300+600=300)
        // Should wrap to pos.y = world_y - h = -300 - 50 = -350
        Entity e = em.create_entity();
        storage.add_component(e, Position{0.0f, 300.0f});
        storage.add_component(e, Size{ENTITY_W, ENTITY_H});
        storage.add_component(e, WrapAround{});

        wrap_system.update(storage, bb);

        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());
        REQUIRE(pos->get().y == -350.0f);
    }

    SECTION("WrapBottomEdge — Req 11.4") {
        // Entity at pos.y=-351, top edge = -351+50 = -301 <= -300
        // Should wrap to pos.y = world_y + world_h = -300 + 600 = 300
        Entity e = em.create_entity();
        storage.add_component(e, Position{0.0f, -351.0f});
        storage.add_component(e, Size{ENTITY_W, ENTITY_H});
        storage.add_component(e, WrapAround{});

        wrap_system.update(storage, bb);

        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());
        REQUIRE(pos->get().y == 300.0f);
    }

    SECTION("InsideWorldNoWrap — Req 11.5") {
        // Entity at (0,0), well inside world bounds — position unchanged
        Entity e = em.create_entity();
        storage.add_component(e, Position{0.0f, 0.0f});
        storage.add_component(e, Size{ENTITY_W, ENTITY_H});
        storage.add_component(e, WrapAround{});

        wrap_system.update(storage, bb);

        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());
        REQUIRE(pos->get().x == 0.0f);
        REQUIRE(pos->get().y == 0.0f);
    }

    SECTION("NoWrapAroundNoWrap — Req 11.6") {
        // Entity WITHOUT WrapAround at (500,500) — outside world but unchanged
        Entity e = em.create_entity();
        storage.add_component(e, Position{500.0f, 500.0f});
        storage.add_component(e, Size{ENTITY_W, ENTITY_H});
        // No WrapAround component

        wrap_system.update(storage, bb);

        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());
        REQUIRE(pos->get().x == 500.0f);
        REQUIRE(pos->get().y == 500.0f);
    }
}
