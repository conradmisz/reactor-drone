/**
 * Property tests for fade_overlay_alpha (ui_fade_math.hpp) — Option-040 Phase 6.
 *
 * Property 1: the fade alpha is bounded, symmetric about 0.5, and single-peaked
 * (non-decreasing on [0, 0.5], non-increasing on [0.5, 1]). Bounded 10x5.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include "engine/ui_fade_math.hpp"

#include <algorithm>

// Bounded property-test iteration counts (property-test-bounds steering).
constexpr int NUM_OUTER_TESTS = 10;  // progress samples across the full range
constexpr int NUM_INNER_TESTS = 5;   // paired offsets for monotonicity checks

TEST_CASE("fade_overlay_alpha: bounded in [0, FADE_MAX_ALPHA] for any input",
          "[Engine][ui_fade][property]") {
    auto p = GENERATE(take(NUM_OUTER_TESTS, random(-2.0f, 3.0f)));
    int a = fade_overlay_alpha(p);
    REQUIRE(a >= 0);
    REQUIRE(a <= FADE_MAX_ALPHA);
}

TEST_CASE("fade_overlay_alpha: symmetric about 0.5",
          "[Engine][ui_fade][property]") {
    auto p = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 1.0f)));
    REQUIRE(fade_overlay_alpha(p) == fade_overlay_alpha(1.0f - p));
}

TEST_CASE("fade_overlay_alpha: non-decreasing on [0, 0.5], non-increasing on [0.5, 1]",
          "[Engine][ui_fade][property]") {
    // Two points on the same half: the one closer to 0.5 has >= alpha.
    auto base = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 0.5f)));
    auto step = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 0.5f)));

    SECTION("rising half [0, 0.5]") {
        float lo = base;
        float hi = std::min(0.5f, base + step);   // hi is >= lo, still <= 0.5
        REQUIRE(fade_overlay_alpha(hi) >= fade_overlay_alpha(lo));
    }
    SECTION("falling half [0.5, 1]") {
        float lo = 0.5f + base;                    // in [0.5, 1.0]
        float hi = std::min(1.0f, lo + step);      // hi >= lo, moving toward 1
        REQUIRE(fade_overlay_alpha(hi) <= fade_overlay_alpha(lo));
    }
}
