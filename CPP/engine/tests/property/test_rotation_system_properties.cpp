/**
 * Property-based tests for RotationSystem
 *
 * These tests verify universal invariants of the RotationSystem:
 * linear update correctness, update additivity, and non-interference.
 *
 * Requirements tested: 1.2, 1.4, 2.1, 10.5, 10.6, 10.7, 10.8
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/systems/rotation_system.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

// Feature: 050-05-rotation-rendering, Property 1: Rotation linear update correctness
// **Validates: Requirements 1.2, 1.4, 10.5, 10.6**
TEST_CASE("Rotation linear update correctness", "[rotation][property]") {
    SECTION("new_angle equals original_angle + angular_velocity * delta_time") {
        auto angle = GENERATE(take(NUM_OUTER_TESTS, random(-100.0f, 100.0f)));
        auto angular_velocity = GENERATE(take(NUM_INNER_TESTS, random(-10.0f, 10.0f)));
        auto dt = GENERATE(take(NUM_INNER_TESTS, random(0.001, 1.0)));

        EntityManager em;
        ComponentStorage storage;
        Blackboard bb;
        RotationSystem rotation_system;

        bb.set<double>("delta_time", dt);

        Entity e = em.create_entity();
        storage.add_component(e, Rotation{angle, angular_velocity});

        float original_angle = angle;

        rotation_system.update(storage, bb);

        auto rot = storage.get_component<Rotation>(e);
        REQUIRE(rot.has_value());
        REQUIRE(rot->get().angle == Catch::Approx(original_angle + angular_velocity * static_cast<float>(dt)));
    }
}

// Feature: 050-05-rotation-rendering, Property 2: Rotation update additivity
// **Validates: Requirements 10.7**
TEST_CASE("Rotation update additivity", "[rotation][property]") {
    SECTION("two updates with dt1, dt2 equals one update with dt1+dt2") {
        auto angle = GENERATE(take(NUM_OUTER_TESTS, random(-100.0f, 100.0f)));
        auto angular_velocity = GENERATE(take(NUM_INNER_TESTS, random(-10.0f, 10.0f)));
        auto dt1 = GENERATE(take(NUM_INNER_TESTS, random(0.001, 1.0)));
        auto dt2 = GENERATE(take(NUM_INNER_TESTS, random(0.001, 1.0)));

        // Path A: two consecutive updates with dt1 then dt2
        EntityManager em_a;
        ComponentStorage storage_a;
        Blackboard bb_a;
        RotationSystem rotation_system_a;

        Entity e_a = em_a.create_entity();
        storage_a.add_component(e_a, Rotation{angle, angular_velocity});

        bb_a.set<double>("delta_time", dt1);
        rotation_system_a.update(storage_a, bb_a);

        bb_a.set<double>("delta_time", dt2);
        rotation_system_a.update(storage_a, bb_a);

        auto rot_a = storage_a.get_component<Rotation>(e_a);

        // Path B: single update with dt1+dt2
        EntityManager em_b;
        ComponentStorage storage_b;
        Blackboard bb_b;
        RotationSystem rotation_system_b;

        Entity e_b = em_b.create_entity();
        storage_b.add_component(e_b, Rotation{angle, angular_velocity});

        bb_b.set<double>("delta_time", dt1 + dt2);
        rotation_system_b.update(storage_b, bb_b);

        auto rot_b = storage_b.get_component<Rotation>(e_b);

        REQUIRE(rot_a.has_value());
        REQUIRE(rot_b.has_value());
        REQUIRE(rot_a->get().angle == Catch::Approx(rot_b->get().angle).margin(1e-4f));
    }
}

// Feature: 050-05-rotation-rendering, Property 3: Rotation non-interference
// **Validates: Requirements 2.1, 10.8**
TEST_CASE("Rotation non-interference", "[rotation][property]") {
    SECTION("entities without Rotation are unchanged after update") {
        auto px = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto py = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));
        auto dt = GENERATE(take(NUM_INNER_TESTS, random(0.001, 1.0)));

        EntityManager em;
        ComponentStorage storage;
        Blackboard bb;
        RotationSystem rotation_system;

        bb.set<double>("delta_time", dt);

        Entity e = em.create_entity();
        storage.add_component(e, Position{px, py});

        rotation_system.update(storage, bb);

        // Position must be unchanged
        auto pos = storage.get_component<Position>(e);
        REQUIRE(pos.has_value());
        REQUIRE(pos->get().x == px);
        REQUIRE(pos->get().y == py);

        // No Rotation component should have been added
        REQUIRE_FALSE(storage.has_component<Rotation>(e));
    }
}
