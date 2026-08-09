/**
 * Unit tests for the Phase 5 widget render-math helpers.
 *
 * Exercises the header-only, SDL-free helpers in ui_render_math.hpp:
 *   - slider_knob_center_x: the knob center always keeps the knob extent
 *     within the track (R12.5).
 *   - checkbox_is_checked: checked iff the value is non-zero (R12.6).
 *
 * Requirements tested: 12.5, 12.6
 * Catch2 v3 only (no GTest).
 */

#include <catch2/catch_test_macros.hpp>

#include "engine/ecs/components.hpp"               // UIRect
#include "engine/ecs/systems/ui_render_math.hpp"   // slider_knob_center_x, checkbox_is_checked

#include <cmath>     // std::abs
#include <limits>

namespace {
constexpr float kEpsilon = 1e-4f;
}

TEST_CASE("slider_knob_center_x keeps the knob extent within the track", "[Engine][ui][unit]") {
    const UIRect rect{300.0f, 320.0f, 200.0f, 24.0f};
    const float knob_w = 16.0f;
    const float half = knob_w * 0.5f;

    SECTION("knob extent stays within the track for representative and out-of-range values") {
        const float values[] = {
            0.0f, 0.5f, 1.0f, -1.0f, 2.0f,
            std::numeric_limits<float>::quiet_NaN()
        };
        for (float value : values) {
            const float cx = slider_knob_center_x(rect, value, knob_w);
            // Left edge of the knob stays at/right of the track's left edge.
            REQUIRE((cx - half) >= rect.x - kEpsilon);
            // Right edge of the knob stays at/left of the track's right edge.
            REQUIRE((cx + half) <= rect.x + rect.w + kEpsilon);
        }
    }

    SECTION("value 0 places the knob at the left limit") {
        const float cx = slider_knob_center_x(rect, 0.0f, knob_w);
        REQUIRE(std::abs(cx - (rect.x + half)) < kEpsilon);
    }

    SECTION("value 1 places the knob at the right limit") {
        const float cx = slider_knob_center_x(rect, 1.0f, knob_w);
        REQUIRE(std::abs(cx - (rect.x + rect.w - half)) < kEpsilon);
    }

    SECTION("value 0.5 places the knob at the track center") {
        const float cx = slider_knob_center_x(rect, 0.5f, knob_w);
        REQUIRE(std::abs(cx - (rect.x + rect.w * 0.5f)) < kEpsilon);
    }

    SECTION("out-of-range and non-finite values clamp within the track") {
        // value < 0 clamps to the left limit; value > 1 clamps to the right limit.
        const float cx_low = slider_knob_center_x(rect, -1.0f, knob_w);
        REQUIRE(std::abs(cx_low - (rect.x + half)) < kEpsilon);

        const float cx_high = slider_knob_center_x(rect, 2.0f, knob_w);
        REQUIRE(std::abs(cx_high - (rect.x + rect.w - half)) < kEpsilon);

        // NaN is treated as 0 -> left limit.
        const float cx_nan =
            slider_knob_center_x(rect, std::numeric_limits<float>::quiet_NaN(), knob_w);
        REQUIRE(std::abs(cx_nan - (rect.x + half)) < kEpsilon);
    }
}

TEST_CASE("slider_knob_center_x centers a knob wider than the track", "[Engine][ui][unit]") {
    const UIRect rect{300.0f, 320.0f, 200.0f, 24.0f};

    SECTION("knob_w >= rect.w returns the track midpoint") {
        // Equal width.
        const float cx_eq = slider_knob_center_x(rect, 0.5f, rect.w);
        REQUIRE(std::abs(cx_eq - (rect.x + rect.w * 0.5f)) < kEpsilon);

        // Wider than the track.
        const float cx_wide = slider_knob_center_x(rect, 0.5f, rect.w + 50.0f);
        REQUIRE(std::abs(cx_wide - (rect.x + rect.w * 0.5f)) < kEpsilon);
    }
}

TEST_CASE("checkbox_is_checked is true iff the value is non-zero", "[Engine][ui][unit]") {
    REQUIRE(checkbox_is_checked(0.0f) == false);
    REQUIRE(checkbox_is_checked(1.0f) == true);
    REQUIRE(checkbox_is_checked(0.5f) == true);
    REQUIRE(checkbox_is_checked(-1.0f) == true);
}
