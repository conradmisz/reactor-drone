/**
 * Property-based tests for LifetimeSystem
 *
 * These tests verify universal invariants of the LifetimeSystem:
 * decrement correctness and DestroyRequest biconditional.
 *
 * Requirements tested: 4.2, 5.1, 5.2, 15.1, 15.2, 15.3
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/systems/lifetime_system.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

// Feature: 050-03-wrap-and-lifetime-systems, Property 4: Lifetime decrement correctness
// **Validates: Requirements 4.2, 15.3**
TEST_CASE("Lifetime decrement correctness", "[lifetime][property]") {
    SECTION("remaining equals original minus delta_time") {
        auto remaining = GENERATE(take(NUM_OUTER_TESTS, random(0.01f, 10.0f)));
        auto dt = GENERATE(take(NUM_INNER_TESTS, random(0.001, 1.0)));

        EntityManager em;
        ComponentStorage storage;
        Blackboard bb;
        LifetimeSystem lifetime_system;

        bb.set<double>("delta_time", dt);

        Entity e = em.create_entity();
        storage.add_component(e, Lifetime{remaining});

        float original_remaining = remaining;

        lifetime_system.update(storage, bb);

        auto lt = storage.get_component<Lifetime>(e);
        REQUIRE(lt.has_value());
        REQUIRE(lt->get().remaining == Catch::Approx(original_remaining - static_cast<float>(dt)));
    }
}

// Feature: 050-03-wrap-and-lifetime-systems, Property 5: Lifetime DestroyRequest biconditional
// **Validates: Requirements 5.1, 5.2, 15.1, 15.2**
TEST_CASE("Lifetime DestroyRequest biconditional", "[lifetime][property]") {
    SECTION("DestroyRequest iff remaining - dt <= 0") {
        auto remaining = GENERATE(take(NUM_OUTER_TESTS, random(0.01f, 2.0f)));
        auto dt = GENERATE(take(NUM_INNER_TESTS, random(0.01, 2.0)));

        EntityManager em;
        ComponentStorage storage;
        Blackboard bb;
        LifetimeSystem lifetime_system;

        bb.set<double>("delta_time", dt);

        Entity e = em.create_entity();
        storage.add_component(e, Lifetime{remaining});

        float original_remaining = remaining;

        lifetime_system.update(storage, bb);

        float expected_remaining = original_remaining - static_cast<float>(dt);

        if (expected_remaining <= 0.0f) {
            REQUIRE(storage.has_component<DestroyRequest>(e));
        } else {
            REQUIRE_FALSE(storage.has_component<DestroyRequest>(e));
        }
    }
}
