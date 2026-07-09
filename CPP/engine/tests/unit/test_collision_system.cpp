/**
 * Unit tests for CollisionSystem
 *
 * These tests verify the CollisionSystem correctly translates strategy pairs
 * into per-entity CollidedWith components, clears stale data between frames,
 * and respects layer filtering.
 *
 * Requirements tested: 7.1, 7.2, 7.3
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/collision_system.hpp"
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include <algorithm>

// Helper: all entities use compatible layers unless otherwise specified
static constexpr uint8_t DEFAULT_LAYER = 1;
static constexpr uint8_t DEFAULT_MASK  = 1;

TEST_CASE("CollisionSystem CollidedWith output", "[collision][unit]") {
    EntityManager em;
    ComponentStorage storage;
    BruteForceStrategy strategy;
    CollisionSystem system(strategy);
    Blackboard blackboard;

    SECTION("SingleOverlappingPair — Req 7.1") {
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{50.0f, 50.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        system.update(storage, blackboard);

        REQUIRE(storage.has_component<CollidedWith>(a));
        REQUIRE(storage.has_component<CollidedWith>(b));

        auto& cw_a = storage.get_component<CollidedWith>(a)->get();
        auto& cw_b = storage.get_component<CollidedWith>(b)->get();

        REQUIRE(cw_a.entities.size() == 1);
        REQUIRE(cw_a.entities[0] == b);

        REQUIRE(cw_b.entities.size() == 1);
        REQUIRE(cw_b.entities[0] == a);
    }

    SECTION("NoCollisions — Req 7.2") {
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{50.0f, 50.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{500.0f, 500.0f});
        storage.add_component(b, Collider{50.0f, 50.0f, DEFAULT_LAYER, DEFAULT_MASK});

        system.update(storage, blackboard);

        REQUIRE_FALSE(storage.has_component<CollidedWith>(a));
        REQUIRE_FALSE(storage.has_component<CollidedWith>(b));
    }

    SECTION("MultipleCollisionsOnOneEntity — Req 7.1") {
        Entity a = em.create_entity();
        Entity b = em.create_entity();
        Entity c = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{10.0f, 10.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(c, Position{20.0f, 20.0f});
        storage.add_component(c, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        system.update(storage, blackboard);

        REQUIRE(storage.has_component<CollidedWith>(a));
        REQUIRE(storage.has_component<CollidedWith>(b));
        REQUIRE(storage.has_component<CollidedWith>(c));

        // Each entity collides with the other two
        auto& cw_b = storage.get_component<CollidedWith>(b)->get();
        REQUIRE(cw_b.entities.size() == 2);
    }

    SECTION("FrameClearing — Req 7.3") {
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        // Frame 1: overlapping
        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{50.0f, 50.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        system.update(storage, blackboard);
        REQUIRE(storage.has_component<CollidedWith>(a));
        REQUIRE(storage.has_component<CollidedWith>(b));

        // Frame 2: move entities far apart
        storage.get_component<Position>(a)->get() = Position{0.0f, 0.0f};
        storage.get_component<Position>(b)->get() = Position{9999.0f, 9999.0f};

        system.update(storage, blackboard);
        REQUIRE_FALSE(storage.has_component<CollidedWith>(a));
        REQUIRE_FALSE(storage.has_component<CollidedWith>(b));
    }

    SECTION("IncompatibleLayers — Req 7.2") {
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        // A: layer=1, mask=1 — only collides with layer 1
        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, 1, 1});

        // B: layer=2, mask=2 — only collides with layer 2
        storage.add_component(b, Position{50.0f, 50.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, 2, 2});

        system.update(storage, blackboard);

        REQUIRE_FALSE(storage.has_component<CollidedWith>(a));
        REQUIRE_FALSE(storage.has_component<CollidedWith>(b));
    }
}
