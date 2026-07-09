/**
 * Unit tests for RotationSystem
 *
 * These tests verify the RotationSystem correctly updates entity angles
 * based on angular velocity and delta time.
 *
 * Requirements tested: 9.1, 9.2, 9.3, 9.4, 9.5, 9.6
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/ecs/systems/rotation_system.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

TEST_CASE("RotationSystem angle update", "[rotation][unit]") {
    EntityManager em;
    ComponentStorage storage;
    Blackboard blackboard;
    RotationSystem system;

    SECTION("PositiveVelocity — Req 9.1") {
        blackboard.set<double>("delta_time", 0.5);

        Entity e = em.create_entity();
        storage.add_component(e, Rotation{0.0f, 1.0f});

        system.update(storage, blackboard);

        auto rot = storage.get_component<Rotation>(e);
        REQUIRE(rot.has_value());
        REQUIRE(rot->get().angle == Catch::Approx(0.5f));
    }

    SECTION("NegativeVelocity — Req 9.2") {
        blackboard.set<double>("delta_time", 0.25);

        Entity e = em.create_entity();
        storage.add_component(e, Rotation{1.0f, -2.0f});

        system.update(storage, blackboard);

        auto rot = storage.get_component<Rotation>(e);
        REQUIRE(rot.has_value());
        REQUIRE(rot->get().angle == Catch::Approx(0.5f));
    }

    SECTION("ZeroVelocity — Req 9.3") {
        blackboard.set<double>("delta_time", 0.5);

        Entity e = em.create_entity();
        storage.add_component(e, Rotation{1.5f, 0.0f});

        system.update(storage, blackboard);

        auto rot = storage.get_component<Rotation>(e);
        REQUIRE(rot.has_value());
        REQUIRE(rot->get().angle == Catch::Approx(1.5f));
    }

    SECTION("EntityWithoutRotation — Req 9.4") {
        blackboard.set<double>("delta_time", 0.5);

        Entity e = em.create_entity();
        storage.add_component(e, Position{10.0f, 20.0f});

        system.update(storage, blackboard);

        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());
        REQUIRE(pos->get().x == Catch::Approx(10.0f));
        REQUIRE(pos->get().y == Catch::Approx(20.0f));
        REQUIRE_FALSE(storage.has_component<Rotation>(e));
    }

    SECTION("MultipleEntities — Req 9.5") {
        blackboard.set<double>("delta_time", 0.5);

        Entity e1 = em.create_entity();
        storage.add_component(e1, Rotation{0.0f, 2.0f});  // 0.0 + 2.0*0.5 = 1.0

        Entity e2 = em.create_entity();
        storage.add_component(e2, Rotation{1.0f, -1.0f});  // 1.0 + (-1.0)*0.5 = 0.5

        system.update(storage, blackboard);

        auto rot1 = storage.get_component<Rotation>(e1);
        REQUIRE(rot1.has_value());
        REQUIRE(rot1->get().angle == Catch::Approx(1.0f));

        auto rot2 = storage.get_component<Rotation>(e2);
        REQUIRE(rot2.has_value());
        REQUIRE(rot2->get().angle == Catch::Approx(0.5f));
    }

    SECTION("EmptyStorage — Req 9.6") {
        blackboard.set<double>("delta_time", 0.5);

        // No entities added — should complete without error
        REQUIRE_NOTHROW(system.update(storage, blackboard));
    }
}
