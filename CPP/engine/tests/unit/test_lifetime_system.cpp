/**
 * Unit tests for LifetimeSystem
 *
 * These tests verify the LifetimeSystem correctly decrements lifetime
 * and marks expired entities with DestroyRequest.
 *
 * Requirements tested: 12.1, 12.2, 12.3, 12.4, 12.5
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/ecs/systems/lifetime_system.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

TEST_CASE("LifetimeSystem decrement and expiration", "[lifetime][unit]") {
    EntityManager em;
    ComponentStorage storage;
    Blackboard bb;
    LifetimeSystem lifetime_system;

    SECTION("DecrementNotExpired — Req 12.1") {
        bb.set<double>("delta_time", 0.5);

        Entity e = em.create_entity();
        storage.add_component(e, Lifetime{1.0f});

        lifetime_system.update(storage, bb);

        auto lt = storage.get_component<Lifetime>(e);
        REQUIRE(lt.has_value());
        REQUIRE(lt->get().remaining == Catch::Approx(0.5f));
        REQUIRE_FALSE(storage.has_component<DestroyRequest>(e));
    }

    SECTION("DecrementExactExpiry — Req 12.2") {
        bb.set<double>("delta_time", 0.5);

        Entity e = em.create_entity();
        storage.add_component(e, Lifetime{0.5f});

        lifetime_system.update(storage, bb);

        auto lt = storage.get_component<Lifetime>(e);
        REQUIRE(lt.has_value());
        REQUIRE(lt->get().remaining <= 0.0f);
        REQUIRE(storage.has_component<DestroyRequest>(e));
    }

    SECTION("DecrementOvershoot — Req 12.3") {
        bb.set<double>("delta_time", 0.5);

        Entity e = em.create_entity();
        storage.add_component(e, Lifetime{0.3f});

        lifetime_system.update(storage, bb);

        auto lt = storage.get_component<Lifetime>(e);
        REQUIRE(lt.has_value());
        REQUIRE(lt->get().remaining < 0.0f);
        REQUIRE(lt->get().remaining == Catch::Approx(-0.2f));
        REQUIRE(storage.has_component<DestroyRequest>(e));
    }

    SECTION("NoLifetimeNoDestroy — Req 12.4") {
        bb.set<double>("delta_time", 0.5);

        Entity e = em.create_entity();
        storage.add_component(e, Position{10.0f, 20.0f});

        lifetime_system.update(storage, bb);

        REQUIRE_FALSE(storage.has_component<DestroyRequest>(e));
    }

    SECTION("MultipleEntitiesMixed — Req 12.5") {
        bb.set<double>("delta_time", 0.5);

        Entity e1 = em.create_entity();
        storage.add_component(e1, Lifetime{2.0f});  // survives: 2.0 - 0.5 = 1.5

        Entity e2 = em.create_entity();
        storage.add_component(e2, Lifetime{0.5f});  // expires: 0.5 - 0.5 = 0.0

        Entity e3 = em.create_entity();
        storage.add_component(e3, Lifetime{0.1f});  // expires: 0.1 - 0.5 = -0.4

        lifetime_system.update(storage, bb);

        // e1 survives
        REQUIRE(storage.get_component<Lifetime>(e1)->get().remaining == Catch::Approx(1.5f));
        REQUIRE_FALSE(storage.has_component<DestroyRequest>(e1));

        // e2 expires (exact zero)
        REQUIRE(storage.get_component<Lifetime>(e2)->get().remaining <= 0.0f);
        REQUIRE(storage.has_component<DestroyRequest>(e2));

        // e3 expires (overshoot)
        REQUIRE(storage.get_component<Lifetime>(e3)->get().remaining < 0.0f);
        REQUIRE(storage.has_component<DestroyRequest>(e3));
    }
}
