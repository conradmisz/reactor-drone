/**
 * Property-based tests for v2 Phase 6 pure logic.
 *
 * P1 (active_arena_index): the selected index is always in range and monotone
 *    non-decreasing as the wave advances — you never drop back to an earlier
 *    arena. Thresholds are randomised; the wave axis is swept densely.
 *
 * P2 (push_circle_out_of_aabb): the resolved centre never penetrates the box —
 *    its distance to the box is >= r (within epsilon) for any circle/box — and a
 *    circle that already clears the box is returned unchanged.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "game/arena_config.hpp"
#include "game/obstacles.hpp"

constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

// Distance from a point to an AABB (0 if inside).
static float dist_point_aabb(float px, float py, float ax, float ay, float aw, float ah) {
    float qx = std::clamp(px, ax, ax + aw);
    float qy = std::clamp(py, ay, ay + ah);
    float dx = px - qx, dy = py - qy;
    return std::sqrt(dx * dx + dy * dy);
}

TEST_CASE("active_arena_index is in-range and monotone in the wave",
          "[Game][arena6][property]") {
    auto base = GENERATE(take(NUM_OUTER_TESTS, random(1, 6)));

    // Build arenas with strictly increasing activation waves from a random base.
    std::vector<ArenaDef> arenas;
    int fw = base;
    for (int i = 0; i <= NUM_INNER_TESTS; ++i) {
        ArenaDef d; d.first_wave = fw; arenas.push_back(d);
        fw += 1 + (i % 3);  // increasing gaps
    }
    const int n = static_cast<int>(arenas.size());

    int prev = active_arena_index(arenas, -100);
    for (int w = -5; w <= 40; ++w) {
        int idx = active_arena_index(arenas, w);
        CHECK(idx >= 0);
        CHECK(idx < n);
        CHECK(idx >= prev);   // never regress to an earlier arena
        prev = idx;
    }
}

TEST_CASE("push_circle_out_of_aabb never leaves the circle penetrating the box",
          "[Game][arena6][property]") {
    auto cx = GENERATE(take(NUM_OUTER_TESTS, random(-50.0f, 90.0f)));

    // Fixed box; sweep the circle centre around/through it with several radii.
    const float ax = 0.0f, ay = 0.0f, aw = 40.0f, ah = 40.0f;
    for (int i = 0; i <= NUM_INNER_TESTS; ++i) {
        for (int j = 0; j <= NUM_INNER_TESTS; ++j) {
            float cy = -50.0f + 140.0f * (static_cast<float>(j) / NUM_INNER_TESTS);
            float r = 5.0f + 8.0f * i;

            float before = dist_point_aabb(cx, cy, ax, ay, aw, ah);
            Vec2 out = push_circle_out_of_aabb(cx, cy, r, ax, ay, aw, ah);
            float after = dist_point_aabb(out.x, out.y, ax, ay, aw, ah);

            // Resolved centre clears the box (tangent allowed).
            CHECK(after >= r - 1e-3f);

            // Already-clear circles are returned unchanged.
            if (before >= r) {
                CHECK(std::abs(out.x - cx) < 1e-4f);
                CHECK(std::abs(out.y - cy) < 1e-4f);
            }
        }
    }
}
