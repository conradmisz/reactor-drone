/**
 * Unit tests for OBB pipeline integration
 *
 * Tests the full collision pipeline (broad phase + narrow phase) with
 * OBBCollider entities alongside CircleCollider and AABB-only entities.
 * Verifies all three strategies produce identical results.
 *
 * Requirements tested: 20.1, 20.2, 20.3, 20.4, 20.5, 20.6
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/systems/uniform_grid_strategy.hpp"
#include "engine/ecs/systems/quadtree_strategy.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include <algorithm>

static constexpr uint8_t DEFAULT_LAYER = 1;
static constexpr uint8_t DEFAULT_MASK  = 1;

TEST_CASE("OBB pipeline integration", "[collision][unit]") {
    EntityManager em;
    ComponentStorage storage;
    BruteForceStrategy brute;
    UniformGridStrategy grid(256, -400.0f, -300.0f, 800.0f, 600.0f);
    QuadtreeStrategy quad(6, 8, -400.0f, -300.0f, 800.0f, 600.0f);

    SECTION("OBB + circle overlapping in narrow phase — Req 20.1") {
        // OBB at (0,0) hw=20 hh=20, center=(20,20), angle=0
        // Circle at (10,10) r=15, center=(25,25)
        // Distance between centers = sqrt(25+25) ≈ 7.07, well within overlap
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{40.0f, 40.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(a, OBBCollider{20.0f, 20.0f});
        storage.add_component(a, Rotation{0.0f, 0.0f});

        storage.add_component(b, Position{10.0f, 10.0f});
        storage.add_component(b, Collider{30.0f, 30.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(b, CircleCollider{15.0f, 0.0f, 0.0f});

        auto bf = brute.detect(storage);
        auto ug = grid.detect(storage);
        auto qt = quad.detect(storage);

        REQUIRE(bf.size() == 1);
        REQUIRE(ug.size() == 1);
        REQUIRE(qt.size() == 1);
    }

    SECTION("OBB + circle pass broad phase but fail narrow — Req 20.2") {
        // OBB at (0,0) hw=20 hh=20, center=(20,20), angle=0
        // Max-AABB extent = sqrt(800) ≈ 28.28, so AABB from ~(-8.28,-8.28) to ~(48.28,48.28)
        // Circle at (40,40) r=5, center=(45,45), bounding AABB (40,40)-(50,50)
        // Broad phase AABBs overlap, but OBB edge at x=40 and circle center at 45
        // with radius 5 → closest point on OBB to circle center (45,45) is (40,40)
        // dist = sqrt(25+25) ≈ 7.07 > 5 → no overlap
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{40.0f, 40.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(a, OBBCollider{20.0f, 20.0f});
        storage.add_component(a, Rotation{0.0f, 0.0f});

        storage.add_component(b, Position{40.0f, 40.0f});
        storage.add_component(b, Collider{10.0f, 10.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(b, CircleCollider{5.0f, 0.0f, 0.0f});

        auto bf = brute.detect(storage);
        auto ug = grid.detect(storage);
        auto qt = quad.detect(storage);

        REQUIRE(bf.size() == 0);
        REQUIRE(ug.size() == 0);
        REQUIRE(qt.size() == 0);
    }

    SECTION("OBB + AABB-only overlapping — Req 20.3") {
        // OBB at (0,0) hw=20 hh=20, center=(20,20), angle=0
        // AABB at (10,10) 30x30 → center=(25,25), hw=15, hh=15
        // Both axis-aligned, clearly overlap
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{40.0f, 40.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(a, OBBCollider{20.0f, 20.0f});
        storage.add_component(a, Rotation{0.0f, 0.0f});

        storage.add_component(b, Position{10.0f, 10.0f});
        storage.add_component(b, Collider{30.0f, 30.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto bf = brute.detect(storage);
        auto ug = grid.detect(storage);
        auto qt = quad.detect(storage);

        REQUIRE(bf.size() == 1);
        REQUIRE(ug.size() == 1);
        REQUIRE(qt.size() == 1);
    }

    SECTION("Two OBB entities overlapping — Req 20.4") {
        // OBB1 at (0,0) hw=20 hh=20, center=(20,20), angle=0
        // OBB2 at (10,10) hw=20 hh=20, center=(30,30), angle=PI/6
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{40.0f, 40.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(a, OBBCollider{20.0f, 20.0f});
        storage.add_component(a, Rotation{0.0f, 0.0f});

        storage.add_component(b, Position{10.0f, 10.0f});
        storage.add_component(b, Collider{40.0f, 40.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(b, OBBCollider{20.0f, 20.0f});
        storage.add_component(b, Rotation{3.14159265f / 6.0f, 0.0f});

        auto bf = brute.detect(storage);
        auto ug = grid.detect(storage);
        auto qt = quad.detect(storage);

        REQUIRE(bf.size() == 1);
        REQUIRE(ug.size() == 1);
        REQUIRE(qt.size() == 1);
    }

    SECTION("Incompatible layers — no collision — Req 20.5") {
        // Two overlapping OBB entities with incompatible layers
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{40.0f, 40.0f, 1, 1});
        storage.add_component(a, OBBCollider{20.0f, 20.0f});
        storage.add_component(a, Rotation{0.0f, 0.0f});

        storage.add_component(b, Position{5.0f, 5.0f});
        storage.add_component(b, Collider{40.0f, 40.0f, 2, 2});
        storage.add_component(b, OBBCollider{20.0f, 20.0f});
        storage.add_component(b, Rotation{0.0f, 0.0f});

        auto bf = brute.detect(storage);
        auto ug = grid.detect(storage);
        auto qt = quad.detect(storage);

        REQUIRE(bf.size() == 0);
        REQUIRE(ug.size() == 0);
        REQUIRE(qt.size() == 0);
    }

    SECTION("Three-strategy equivalence — Req 20.6") {
        // Mix of OBB, circle, and AABB-only entities
        Entity e1 = em.create_entity();
        Entity e2 = em.create_entity();
        Entity e3 = em.create_entity();

        // OBB entity
        storage.add_component(e1, Position{0.0f, 0.0f});
        storage.add_component(e1, Collider{40.0f, 40.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(e1, OBBCollider{20.0f, 20.0f});
        storage.add_component(e1, Rotation{0.5f, 0.0f});

        // Circle entity
        storage.add_component(e2, Position{15.0f, 15.0f});
        storage.add_component(e2, Collider{30.0f, 30.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(e2, CircleCollider{15.0f, 0.0f, 0.0f});

        // AABB-only entity
        storage.add_component(e3, Position{50.0f, 50.0f});
        storage.add_component(e3, Collider{30.0f, 30.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto bf = brute.detect(storage);
        auto ug = grid.detect(storage);
        auto qt = quad.detect(storage);

        std::sort(bf.begin(), bf.end());
        std::sort(ug.begin(), ug.end());
        std::sort(qt.begin(), qt.end());

        REQUIRE(bf == ug);
        REQUIRE(bf == qt);
    }
}
