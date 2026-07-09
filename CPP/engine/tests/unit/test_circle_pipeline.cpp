/**
 * Unit tests for two-phase collision pipeline with mixed entity types
 *
 * Tests the full pipeline (broad phase AABB + narrow phase shape-specific)
 * with CircleCollider and AABB-only entities.
 *
 * Requirements tested: 17.1, 17.2, 17.3, 17.4, 17.5
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/systems/uniform_grid_strategy.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include <algorithm>

static constexpr uint8_t DEFAULT_LAYER = 1;
static constexpr uint8_t DEFAULT_MASK  = 1;

TEST_CASE("Two-phase pipeline with mixed entity types", "[collision][unit]") {
    EntityManager em;
    ComponentStorage storage;
    BruteForceStrategy brute;
    UniformGridStrategy grid(256, -400.0f, -300.0f, 800.0f, 600.0f);

    SECTION("Two CircleCollider entities overlapping in narrow phase — Req 17.1") {
        // Two circles at (0,0) r=20 and (10,0) r=20 — circles overlap
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{40.0f, 40.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(a, CircleCollider{20.0f, 0.0f, 0.0f});

        storage.add_component(b, Position{10.0f, 0.0f});
        storage.add_component(b, Collider{40.0f, 40.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(b, CircleCollider{20.0f, 0.0f, 0.0f});

        auto bf_pairs = brute.detect(storage);
        auto ug_pairs = grid.detect(storage);

        REQUIRE(bf_pairs.size() == 1);
        REQUIRE(ug_pairs.size() == 1);
    }

    SECTION("Two CircleCollider entities pass broad phase but fail narrow — Req 17.2") {
        // Two circles whose bounding AABBs overlap at corners but circles don't touch
        // A at (0,0) r=20: AABB (0,0)-(40,40), center (20,20)
        // B at (35,35) r=20: AABB (35,35)-(75,75), center (55,55)
        // AABBs overlap (35<40 and 35<40), but circle distance:
        // sqrt((55-20)^2 + (55-20)^2) = sqrt(2450) ≈ 49.5 > 40 (r1+r2)
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{40.0f, 40.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(a, CircleCollider{20.0f, 0.0f, 0.0f});

        storage.add_component(b, Position{35.0f, 35.0f});
        storage.add_component(b, Collider{40.0f, 40.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(b, CircleCollider{20.0f, 0.0f, 0.0f});

        auto bf_pairs = brute.detect(storage);
        auto ug_pairs = grid.detect(storage);

        REQUIRE(bf_pairs.size() == 0);
        REQUIRE(ug_pairs.size() == 0);
    }

    SECTION("CircleCollider vs AABB-only entity overlapping — Req 17.3") {
        // Circle at (0,0) r=20, center=(20,20). AABB-only at (15,15) 30x30.
        // circle_aabb_overlap(20,20,20, 15,15,30,30) — center inside AABB → true
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{40.0f, 40.0f, DEFAULT_LAYER, DEFAULT_MASK});
        storage.add_component(a, CircleCollider{20.0f, 0.0f, 0.0f});

        storage.add_component(b, Position{15.0f, 15.0f});
        storage.add_component(b, Collider{30.0f, 30.0f, DEFAULT_LAYER, DEFAULT_MASK});
        // b has NO CircleCollider — AABB-only

        auto bf_pairs = brute.detect(storage);
        auto ug_pairs = grid.detect(storage);

        REQUIRE(bf_pairs.size() == 1);
        REQUIRE(ug_pairs.size() == 1);
    }

    SECTION("Two AABB-only entities overlapping — Req 17.4") {
        // Standard AABB overlap, no CircleColliders — unchanged behavior
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{50.0f, 50.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto bf_pairs = brute.detect(storage);
        auto ug_pairs = grid.detect(storage);

        REQUIRE(bf_pairs.size() == 1);
        REQUIRE(ug_pairs.size() == 1);
    }

    SECTION("Incompatible layers — no collision regardless of collider type — Req 17.5") {
        // Two overlapping circle entities with incompatible layers
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{40.0f, 40.0f, 1, 1});
        storage.add_component(a, CircleCollider{20.0f, 0.0f, 0.0f});

        storage.add_component(b, Position{5.0f, 5.0f});
        storage.add_component(b, Collider{40.0f, 40.0f, 2, 2});
        storage.add_component(b, CircleCollider{20.0f, 0.0f, 0.0f});

        auto bf_pairs = brute.detect(storage);
        auto ug_pairs = grid.detect(storage);

        REQUIRE(bf_pairs.size() == 0);
        REQUIRE(ug_pairs.size() == 0);
    }
}
