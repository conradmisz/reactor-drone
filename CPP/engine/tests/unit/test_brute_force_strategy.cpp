/**
 * Unit tests for BruteForceStrategy collision detection
 *
 * These tests verify the brute force strategy produces correct collision pairs
 * for concrete entity arrangements with Position and Collider components.
 *
 * Requirements tested: 10.1, 10.2, 10.3, 10.4, 10.5, 10.6
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include <algorithm>

// Helper: all entities use compatible layers unless otherwise specified
static constexpr uint8_t DEFAULT_LAYER = 1;
static constexpr uint8_t DEFAULT_MASK  = 1;

TEST_CASE("BruteForceStrategy pair generation", "[collision][unit]") {
    EntityManager em;
    ComponentStorage storage;
    BruteForceStrategy strategy;

    SECTION("OneOverlappingPair — Req 10.1") {
        // A at (0,0) 100x100, B at (50,50) 100x100, C at (500,500) 100x100
        // A overlaps B, A doesn't overlap C, B doesn't overlap C
        Entity a = em.create_entity();
        Entity b = em.create_entity();
        Entity c = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{50.0f, 50.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(c, Position{500.0f, 500.0f});
        storage.add_component(c, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.size() == 1);
        // Pair should be normalized (smaller ID first)
        REQUIRE(pairs[0].first < pairs[0].second);
        REQUIRE(pairs[0] == std::make_pair(std::min(a, b), std::max(a, b)));
    }

    SECTION("AllThreePairsOverlap — Req 10.2") {
        // All three at (0,0) 100x100 — all pairwise overlap
        Entity a = em.create_entity();
        Entity b = em.create_entity();
        Entity c = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{10.0f, 10.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(c, Position{20.0f, 20.0f});
        storage.add_component(c, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.size() == 3);
    }

    SECTION("CompatibleLayersNoSpatialOverlap — Req 10.3") {
        // Compatible layers but spatially separated
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{50.0f, 50.0f, DEFAULT_LAYER, DEFAULT_MASK});

        storage.add_component(b, Position{200.0f, 200.0f});
        storage.add_component(b, Collider{50.0f, 50.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.size() == 0);
    }

    SECTION("SpatialOverlapIncompatibleLayers — Req 10.4") {
        // Overlapping AABBs but incompatible layers
        // A: layer=1, mask=1 (only collides with layer 1)
        // B: layer=2, mask=2 (only collides with layer 2)
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, 1, 1});

        storage.add_component(b, Position{50.0f, 50.0f});
        storage.add_component(b, Collider{100.0f, 100.0f, 2, 2});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.size() == 0);
    }

    SECTION("ZeroCollidableEntities — Req 10.5") {
        // Entities with Position but no Collider
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(b, Position{10.0f, 10.0f});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.size() == 0);
    }

    SECTION("ColliderWithoutPosition — Req 10.6") {
        // Entity with Collider but no Position should be excluded
        Entity a = em.create_entity();
        Entity b = em.create_entity();

        // a has both Position and Collider
        storage.add_component(a, Position{0.0f, 0.0f});
        storage.add_component(a, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        // b has only Collider, no Position
        storage.add_component(b, Collider{100.0f, 100.0f, DEFAULT_LAYER, DEFAULT_MASK});

        auto pairs = strategy.detect(storage);
        REQUIRE(pairs.size() == 0);
    }
}
