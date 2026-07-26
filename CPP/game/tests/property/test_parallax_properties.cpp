/**
 * Property-based tests for parallax:: — the pure parallax-scroll offset (v2).
 *
 * Property P1: |offset| is monotone non-increasing in scroll_factor for a fixed
 *              camera displacement — farther layers (higher scroll_factor) never
 *              move more than nearer ones. It is also bounded by |camera| (the
 *              nearest layer, scroll_factor 0, is the fastest) and vanishes at
 *              scroll_factor 1 regardless of the camera.
 *
 * One Catch2 GENERATE drives the camera; the scroll_factor space is swept with
 * plain internal loops (nesting GENERATE multiplies section replays needlessly).
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include <algorithm>
#include <cmath>

#include "game/parallax.hpp"

// Configurable test iteration counts (MANDATORY — workspace policy)
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

TEST_CASE("parallax::parallax_offset shrinks with distance and is camera-bounded",
          "[Game][parallax][property]") {
    auto camera = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));

    for (int i = 0; i <= NUM_INNER_TESTS; ++i) {
        for (int j = 0; j <= NUM_INNER_TESTS; ++j) {
            // Two scroll factors in [0,1]; near = smaller factor, far = larger.
            float a = static_cast<float>(i) / NUM_INNER_TESTS;
            float b = static_cast<float>(j) / NUM_INNER_TESTS;
            float near_f = std::min(a, b), far_f = std::max(a, b);

            float near_off = parallax::parallax_offset(camera, near_f);
            float far_off  = parallax::parallax_offset(camera, far_f);

            // Farther layer never moves more than the nearer one.
            CHECK(std::abs(far_off) <= std::abs(near_off) + 1e-3f);
            // No layer outruns the camera itself.
            CHECK(std::abs(near_off) <= std::abs(camera) + 1e-3f);
        }
    }

    // scroll_factor 1 is glued to the camera: zero offset for any camera.
    CHECK(parallax::parallax_offset(camera, 1.0f) == 0.0f);
}
