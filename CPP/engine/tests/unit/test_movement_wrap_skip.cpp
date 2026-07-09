/**
 * Unit tests for MovementSystem WrapAround skip behavior
 *
 * These tests verify that the MovementSystem skips boundary clamping for
 * entities with the WrapAround component while still clamping entities without it.
 *
 * Requirements tested: 13.1, 13.2, 13.3
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/movement_system.hpp"
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
    bb.set<double>("delta_time", 1.0);
    return bb;
}

TEST_CASE("MovementSystem WrapAround skip", "[movement][wrap][unit]") {
    EntityManager em;
    ComponentStorage storage;
    Blackboard bb = make_blackboard();
    MovementSystem movement_system;

    SECTION("WrapAroundEntityNotClamped — Req 13.1") {
        // Entity at (350,250) with velocity (200,200) and WrapAround
        // After update: pos = (350+200, 250+200) = (550, 450)
        // World max_x for 50-wide entity = -400+800-50 = 350, so 550 > 350
        // World max_y for 50-tall entity = -300+600-50 = 250, so 450 > 250
        // But WrapAround means NO clamping
        Entity e = em.create_entity();
        storage.add_component(e, Position{350.0f, 250.0f});
        storage.add_component(e, Velocity{200.0f, 200.0f});
        storage.add_component(e, Size{ENTITY_W, ENTITY_H});
        storage.add_component(e, WrapAround{});

        movement_system.update(storage, bb);

        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());
        REQUIRE(pos->get().x == 550.0f);
        REQUIRE(pos->get().y == 450.0f);
    }

    SECTION("NonWrapAroundEntityClamped — Req 13.2") {
        // Entity at (350,250) with velocity (200,200) and NO WrapAround
        // After velocity: pos = (550, 450)
        // World max_x = -400+800-50 = 350, so clamped to 350
        // World max_y = -300+600-50 = 250, so clamped to 250
        Entity e = em.create_entity();
        storage.add_component(e, Position{350.0f, 250.0f});
        storage.add_component(e, Velocity{200.0f, 200.0f});
        storage.add_component(e, Size{ENTITY_W, ENTITY_H});
        // No WrapAround

        movement_system.update(storage, bb);

        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());
        REQUIRE(pos->get().x == 350.0f);
        REQUIRE(pos->get().y == 250.0f);
    }

    SECTION("MixedEntities — Req 13.3") {
        // Entity A: with WrapAround → NOT clamped → (550, 450)
        Entity a = em.create_entity();
        storage.add_component(a, Position{350.0f, 250.0f});
        storage.add_component(a, Velocity{200.0f, 200.0f});
        storage.add_component(a, Size{ENTITY_W, ENTITY_H});
        storage.add_component(a, WrapAround{});

        // Entity B: without WrapAround → clamped → (350, 250)
        Entity b = em.create_entity();
        storage.add_component(b, Position{350.0f, 250.0f});
        storage.add_component(b, Velocity{200.0f, 200.0f});
        storage.add_component(b, Size{ENTITY_W, ENTITY_H});
        // No WrapAround

        movement_system.update(storage, bb);

        auto pos_a = storage.get_component<Position>(a);
        REQUIRE(pos_a.has_value());
        REQUIRE(pos_a->get().x == 550.0f);
        REQUIRE(pos_a->get().y == 450.0f);

        auto pos_b = storage.get_component<Position>(b);
        REQUIRE(pos_b.has_value());
        REQUIRE(pos_b->get().x == 350.0f);
        REQUIRE(pos_b->get().y == 250.0f);
    }
}
