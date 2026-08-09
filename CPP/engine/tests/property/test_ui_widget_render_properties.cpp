/**
 * Property-based tests for the Option-040 Phase 5 widget render-math helpers.
 *
 * Implements two correctness properties from the o-040-05-json-layout design
 * "Correctness Properties" section, one property-based TEST_CASE each:
 *   Property 3: Slider knob stays within the track and is monotonic in value
 *   Property 4: Checkbox is checked exactly when its value is non-zero
 *
 * The tested logic lives entirely in the SDL-free, header-only helpers
 * slider_knob_center_x and checkbox_is_checked in ui_render_math.hpp; no window
 * or renderer is needed and UIRenderSystem itself is not constructed here.
 *
 * Feature: o-040-05-json-layout
 * All property tests bounded per the workspace property-test-bounds policy.
 * Catch2 v3 only (no GTest).
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include "engine/ecs/components.hpp"               // UIRect
#include "engine/ecs/systems/ui_render_math.hpp"   // slider_knob_center_x, checkbox_is_checked

#include <algorithm>
#include <cmath>     // std::abs, std::isfinite, NaN/inf
#include <limits>

// Configurable test iteration counts (MANDATORY — workspace property-test-bounds policy)
constexpr int NUM_OUTER_TESTS = 10;  // Number of different rects / outer subjects
constexpr int NUM_INNER_TESTS = 5;   // Number of value variations per subject

namespace {
constexpr float kEpsilon = 1e-3f;
}

// ---------------------------------------------------------------------------
// Feature: o-040-05-json-layout, Property 3: Slider knob stays within the track
// and is monotonic in value.
//
// For any widget rect and for any value (including values below zero, above
// one, and non-finite) with a knob width no greater than the track width,
// slider_knob_center_x returns a center such that the knob's full extent
// [center - knob_w/2, center + knob_w/2] lies within the track
// [rect.x, rect.x + rect.w]; and for any two in-range values v1 <= v2 in [0, 1],
// the center for v1 is no greater than the center for v2 (monotonic
// non-decreasing).
//
// Validates: Requirements 10.2
// ---------------------------------------------------------------------------
TEST_CASE("Property 3: Slider knob stays within the track and is monotonic in value",
          "[Engine][ui][property]") {

    // Knob width is fixed; rects are generated with w >= 16 so knob_w <= rect.w.
    constexpr float knob_w = 16.0f;
    constexpr float half   = knob_w * 0.5f;

    SECTION("knob extent stays within the track for in/out-of-range and non-finite values") {
        // Random rect: x in [-200, 600], y any (wide range), w in [16, 400]
        // (>= knob_w so knob_w <= rect.w), h in [4, 60].
        auto rx = GENERATE(take(NUM_OUTER_TESTS, random(-200.0, 600.0)));
        auto ry = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));
        auto rw = GENERATE(take(NUM_INNER_TESTS, random(16.0, 400.0)));
        auto rh = GENERATE(take(NUM_INNER_TESTS, random(4.0, 60.0)));
        // Random value spanning well below zero and well above one (-5 .. 5).
        auto vraw = GENERATE(take(NUM_INNER_TESTS, random(-5.0, 5.0)));

        UIRect rect{};
        rect.x = static_cast<float>(rx);
        rect.y = static_cast<float>(ry);
        rect.w = static_cast<float>(rw);
        rect.h = static_cast<float>(rh);

        // The random in/out-of-range value plus the non-finite cases.
        const float values[] = {
            static_cast<float>(vraw),
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::infinity(),
            -std::numeric_limits<float>::infinity()
        };

        for (float value : values) {
            const float cx = slider_knob_center_x(rect, value, knob_w);
            // Knob extent [cx - half, cx + half] within [rect.x, rect.x + rect.w].
            REQUIRE((cx - half) >= rect.x - kEpsilon);
            REQUIRE((cx + half) <= rect.x + rect.w + kEpsilon);
        }
    }

    SECTION("center is monotonic non-decreasing for ordered in-range value pairs") {
        auto rx = GENERATE(take(NUM_OUTER_TESTS, random(-200.0, 600.0)));
        auto rw = GENERATE(take(NUM_INNER_TESTS, random(16.0, 400.0)));
        // Two in-range values in [0, 1]; we order them v1 <= v2.
        auto pair = GENERATE(take(NUM_INNER_TESTS, chunk(2, random(0.0, 1.0))));

        UIRect rect{};
        rect.x = static_cast<float>(rx);
        rect.y = 0.0f;
        rect.w = static_cast<float>(rw);
        rect.h = 24.0f;

        float v1 = static_cast<float>(pair[0]);
        float v2 = static_cast<float>(pair[1]);
        if (v1 > v2) std::swap(v1, v2);  // ensure v1 <= v2

        const float cx1 = slider_knob_center_x(rect, v1, knob_w);
        const float cx2 = slider_knob_center_x(rect, v2, knob_w);

        // Monotonic non-decreasing in value.
        REQUIRE(cx1 <= cx2 + kEpsilon);
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-05-json-layout, Property 4: Checkbox is checked exactly when
// its value is non-zero.
//
// For any value, checkbox_is_checked(value) returns true if and only if value
// is not equal to zero.
//
// Validates: Requirements 11.2, 11.3
// ---------------------------------------------------------------------------
TEST_CASE("Property 4: Checkbox is checked exactly when its value is non-zero",
          "[Engine][ui][property]") {

    SECTION("checkbox_is_checked(value) == (value != 0) for random values incl. zero") {
        auto outer = GENERATE(take(NUM_OUTER_TESTS, random(-1000.0, 1000.0)));
        auto inner = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));

        // Build a set of values including an explicit 0 and assorted nonzero
        // values (the random draws are nonzero with probability 1).
        const float values[] = {
            0.0f,
            static_cast<float>(outer),
            static_cast<float>(inner),
            static_cast<float>(outer + inner)
        };

        for (float value : values) {
            REQUIRE(checkbox_is_checked(value) == (value != 0.0f));
        }
    }
}
