/**
 * Property-based tests for OBB collision detection
 *
 * Five properties:
 *   1. OBB-OBB overlap symmetry
 *   2. Max-AABB never misses true OBB-OBB collision
 *   3. Max-AABB never misses true OBB-circle collision
 *   4. Strategy equivalence with OBB entities
 *   5. OBB at zero rotation equals AABB
 *
 * Requirements tested: 13.1, 14.1, 2.3, 15.1, 10.1, 16.1, 6.4, 17.1
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/systems/collision_math.hpp"
#include "engine/ecs/systems/brute_force_strategy.hpp"
#include "engine/ecs/systems/uniform_grid_strategy.hpp"
#include "engine/ecs/systems/quadtree_strategy.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include <algorithm>
#include <cmath>

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

// World bounds matching GameData.json
constexpr float WORLD_X = -400.0f;
constexpr float WORLD_Y = -300.0f;
constexpr float WORLD_W = 800.0f;
constexpr float WORLD_H = 600.0f;
constexpr int   CELL_SIZE = 256;
constexpr float PI = 3.14159265f;

// Feature: 060-07-obb-collision, Property 1: OBB-OBB overlap symmetry
// **Validates: Requirements 13.1**
TEST_CASE("OBB-OBB overlap symmetry",
          "[Feature: 060-07-obb-collision][Property 1: OBB-OBB overlap symmetry]") {
    SECTION("obb_obb_overlap(a,b) == obb_obb_overlap(b,a)") {
        auto cx1 = GENERATE(take(NUM_OUTER_TESTS, random(-400.0f, 400.0f)));
        auto cy1 = GENERATE(take(NUM_INNER_TESTS, random(-300.0f, 300.0f)));

        float hw1 = 1.0f + std::fmod(std::abs(cx1 * 3.7f + cy1 * 2.3f), 49.0f);
        float hh1 = 1.0f + std::fmod(std::abs(cy1 * 5.1f + cx1 * 1.9f), 49.0f);
        float a1 = std::fmod(std::abs(cx1 * 2.1f), 2.0f * PI);

        float cx2 = cx1 + std::fmod(cy1 * 3.3f, 200.0f) - 100.0f;
        float cy2 = cy1 + std::fmod(cx1 * 2.7f, 200.0f) - 100.0f;
        float hw2 = 1.0f + std::fmod(std::abs(cy1 * 7.1f + cx1 * 4.3f), 49.0f);
        float hh2 = 1.0f + std::fmod(std::abs(cx1 * 6.3f + cy1 * 3.1f), 49.0f);
        float a2 = std::fmod(std::abs(cy1 * 4.7f), 2.0f * PI);

        REQUIRE(obb_obb_overlap(cx1, cy1, hw1, hh1, a1, cx2, cy2, hw2, hh2, a2) ==
                obb_obb_overlap(cx2, cy2, hw2, hh2, a2, cx1, cy1, hw1, hh1, a1));
    }
}

// Feature: 060-07-obb-collision, Property 2: Max-AABB never misses true OBB-OBB collision
// **Validates: Requirements 14.1, 2.3**
TEST_CASE("Max-AABB never misses true OBB-OBB collision",
          "[Feature: 060-07-obb-collision][Property 2: Max-AABB never misses true OBB-OBB collision]") {
    SECTION("If OBBs overlap, their Max-AABBs must also overlap") {
        auto cx1 = GENERATE(take(NUM_OUTER_TESTS, random(-400.0f, 400.0f)));
        auto cy1 = GENERATE(take(NUM_INNER_TESTS, random(-300.0f, 300.0f)));

        float hw1 = 1.0f + std::fmod(std::abs(cx1 * 3.7f + cy1 * 2.3f), 49.0f);
        float hh1 = 1.0f + std::fmod(std::abs(cy1 * 5.1f + cx1 * 1.9f), 49.0f);
        float a1 = std::fmod(std::abs(cx1 * 2.1f), 2.0f * PI);

        float cx2 = cx1 + std::fmod(cy1 * 3.3f, 100.0f) - 50.0f;
        float cy2 = cy1 + std::fmod(cx1 * 2.7f, 100.0f) - 50.0f;
        float hw2 = 1.0f + std::fmod(std::abs(cy1 * 7.1f + cx1 * 4.3f), 49.0f);
        float hh2 = 1.0f + std::fmod(std::abs(cx1 * 6.3f + cy1 * 3.1f), 49.0f);
        float a2 = std::fmod(std::abs(cy1 * 4.7f), 2.0f * PI);

        if (obb_obb_overlap(cx1, cy1, hw1, hh1, a1, cx2, cy2, hw2, hh2, a2)) {
            float ext1 = std::sqrt(hw1 * hw1 + hh1 * hh1);
            float ext2 = std::sqrt(hw2 * hw2 + hh2 * hh2);
            REQUIRE(aabb_overlap(cx1 - ext1, cy1 - ext1, 2.0f * ext1, 2.0f * ext1,
                                 cx2 - ext2, cy2 - ext2, 2.0f * ext2, 2.0f * ext2));
        }
    }
}

// Feature: 060-07-obb-collision, Property 3: Max-AABB never misses true OBB-circle collision
// **Validates: Requirements 15.1**
TEST_CASE("Max-AABB never misses true OBB-circle collision",
          "[Feature: 060-07-obb-collision][Property 3: Max-AABB never misses true OBB-circle collision]") {
    SECTION("If OBB-circle overlap, their bounding AABBs must also overlap") {
        auto cx = GENERATE(take(NUM_OUTER_TESTS, random(-400.0f, 400.0f)));
        auto cy = GENERATE(take(NUM_INNER_TESTS, random(-300.0f, 300.0f)));

        float hw = 1.0f + std::fmod(std::abs(cx * 3.7f + cy * 2.3f), 49.0f);
        float hh = 1.0f + std::fmod(std::abs(cy * 5.1f + cx * 1.9f), 49.0f);
        float angle = std::fmod(std::abs(cx * 2.1f), 2.0f * PI);

        float ccx = cx + std::fmod(cy * 3.3f, 100.0f) - 50.0f;
        float ccy = cy + std::fmod(cx * 2.7f, 100.0f) - 50.0f;
        float r = 1.0f + std::fmod(std::abs(cy * 7.1f + cx * 4.3f), 49.0f);

        if (obb_circle_overlap(cx, cy, hw, hh, angle, ccx, ccy, r)) {
            float ext = std::sqrt(hw * hw + hh * hh);
            // OBB Max-AABB
            float obb_ax = cx - ext;
            float obb_ay = cy - ext;
            float obb_aw = 2.0f * ext;
            float obb_ah = 2.0f * ext;
            // Circle bounding AABB
            float c_ax = ccx - r;
            float c_ay = ccy - r;
            float c_aw = 2.0f * r;
            float c_ah = 2.0f * r;
            REQUIRE(aabb_overlap(obb_ax, obb_ay, obb_aw, obb_ah,
                                 c_ax, c_ay, c_aw, c_ah));
        }
    }
}

/**
 * Helper: populate N entities with random positions, colliders, and randomly
 * assign OBBCollider (~33%), CircleCollider (~33%), or Collider-only (~33%).
 * OBB entities get a Rotation component with a derived angle.
 */
static void populate_obb_mixed_entities(EntityManager& em, ComponentStorage& storage,
                                        int count, float seed_x, float seed_y) {
    for (int i = 0; i < count; ++i) {
        Entity e = em.create_entity();
        float x = WORLD_X + std::fmod(std::abs(seed_x * 7.3f + static_cast<float>(i) * 97.1f), WORLD_W);
        float y = WORLD_Y + std::fmod(std::abs(seed_y * 11.7f + static_cast<float>(i) * 53.3f), WORLD_H);
        float sz = 10.0f + std::fmod(std::abs(seed_x * 3.1f + seed_y * 2.7f + static_cast<float>(i) * 17.9f), 54.0f);

        storage.add_component(e, Position{x, y});
        storage.add_component(e, Collider{sz, sz, 1, 1});

        int kind = i % 3;
        if (kind == 0) {
            // OBB entity
            float hw = sz / 2.0f;
            float hh = sz / 2.0f;
            storage.add_component(e, OBBCollider{hw, hh});
            float angle = std::fmod(std::abs(seed_x * 2.1f + static_cast<float>(i) * 1.3f), 2.0f * PI);
            storage.add_component(e, Rotation{angle, 0.0f});
        } else if (kind == 1) {
            // Circle entity
            storage.add_component(e, CircleCollider{sz / 2.0f, 0.0f, 0.0f});
        }
        // kind == 2: AABB-only
    }
}

// Feature: 060-07-obb-collision, Property 4: Strategy equivalence with OBB entities
// **Validates: Requirements 10.1, 16.1, 6.4**
TEST_CASE("Strategy equivalence with OBB entities",
          "[Feature: 060-07-obb-collision][Property 4: Strategy equivalence with OBB entities]") {
    SECTION("All three strategies produce identical collision pairs with OBB mix") {
        auto seed_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
        auto seed_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

        int n = static_cast<int>(std::fmod(std::abs(seed_x + seed_y), 16.0f));

        EntityManager em;
        ComponentStorage storage;
        BruteForceStrategy brute;
        UniformGridStrategy grid(CELL_SIZE, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);
        QuadtreeStrategy quad(6, 8, WORLD_X, WORLD_Y, WORLD_W, WORLD_H);

        populate_obb_mixed_entities(em, storage, n, seed_x, seed_y);

        auto bf_pairs = brute.detect(storage);
        auto ug_pairs = grid.detect(storage);
        auto qt_pairs = quad.detect(storage);

        std::sort(bf_pairs.begin(), bf_pairs.end());
        std::sort(ug_pairs.begin(), ug_pairs.end());
        std::sort(qt_pairs.begin(), qt_pairs.end());

        REQUIRE(bf_pairs == ug_pairs);
        REQUIRE(bf_pairs == qt_pairs);
    }
}

// Feature: 060-07-obb-collision, Property 5: OBB at zero rotation equals AABB
// **Validates: Requirements 17.1**
TEST_CASE("OBB at zero rotation equals AABB",
          "[Feature: 060-07-obb-collision][Property 5: OBB at zero rotation equals AABB]") {
    SECTION("obb_obb_overlap with angle=0 matches aabb_overlap on equivalent AABBs") {
        auto cx1 = GENERATE(take(NUM_OUTER_TESTS, random(-400.0f, 400.0f)));
        auto cy1 = GENERATE(take(NUM_INNER_TESTS, random(-300.0f, 300.0f)));

        float hw1 = 1.0f + std::fmod(std::abs(cx1 * 3.7f + cy1 * 2.3f), 99.0f);
        float hh1 = 1.0f + std::fmod(std::abs(cy1 * 5.1f + cx1 * 1.9f), 99.0f);

        float cx2 = cx1 + std::fmod(cy1 * 3.3f, 200.0f) - 100.0f;
        float cy2 = cy1 + std::fmod(cx1 * 2.7f, 200.0f) - 100.0f;
        float hw2 = 1.0f + std::fmod(std::abs(cy1 * 7.1f + cx1 * 4.3f), 99.0f);
        float hh2 = 1.0f + std::fmod(std::abs(cx1 * 6.3f + cy1 * 3.1f), 99.0f);

        bool obb_result = obb_obb_overlap(cx1, cy1, hw1, hh1, 0.0f,
                                          cx2, cy2, hw2, hh2, 0.0f);
        bool aabb_result = aabb_overlap(cx1 - hw1, cy1 - hh1, 2.0f * hw1, 2.0f * hh1,
                                        cx2 - hw2, cy2 - hh2, 2.0f * hw2, 2.0f * hh2);

        REQUIRE(obb_result == aabb_result);
    }
}
