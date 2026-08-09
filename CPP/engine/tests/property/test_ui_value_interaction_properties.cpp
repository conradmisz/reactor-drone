/**
 * Property tests for slider_value_from_pointer (ui_render_math.hpp) —
 * Option-040 Phase 6.
 *
 * Property 2: the slider value is bounded in [0,1], clamps at the track limits,
 * and inverts slider_knob_center_x (round-trip). Bounded 10x5.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include "engine/ecs/systems/ui_render_math.hpp"

// Bounded property-test iteration counts (property-test-bounds steering).
constexpr int NUM_OUTER_TESTS = 10;  // pointer positions / rect origins
constexpr int NUM_INNER_TESTS = 5;   // normalized values for the round-trip

namespace {
constexpr float KNOB_W = 16.0f;
// A track wide enough for usable travel (w > knob_w).
constexpr UIRect kTrack{300.0f, 320.0f, 200.0f, 24.0f};
}  // namespace

TEST_CASE("slider_value_from_pointer: result is always in [0,1]",
          "[Engine][ui_value][property]") {
    // Sample pointer x well beyond both edges of the track.
    auto ui_x = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 800.0f)));
    float v = slider_value_from_pointer(kTrack, ui_x, KNOB_W);
    REQUIRE(v >= 0.0f);
    REQUIRE(v <= 1.0f);
}

TEST_CASE("slider_value_from_pointer: clamps at and beyond the track limits",
          "[Engine][ui_value][property]") {
    const float left  = kTrack.x + KNOB_W * 0.5f;
    const float right = kTrack.x + kTrack.w - KNOB_W * 0.5f;

    auto over = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 500.0f)));
    REQUIRE(slider_value_from_pointer(kTrack, left - over, KNOB_W) == 0.0f);   // at/left of left limit
    REQUIRE(slider_value_from_pointer(kTrack, right + over, KNOB_W) == 1.0f);  // at/right of right limit
}

TEST_CASE("slider_value_from_pointer: round-trips with slider_knob_center_x",
          "[Engine][ui_value][property]") {
    auto v = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 1.0f)));
    float cx = slider_knob_center_x(kTrack, v, KNOB_W);
    float recovered = slider_value_from_pointer(kTrack, cx, KNOB_W);
    REQUIRE(recovered == Catch::Approx(v).margin(1e-5));
}

TEST_CASE("slider_value_from_pointer: degenerate track (knob >= width) yields 0",
          "[Engine][ui_value][property]") {
    auto ui_x = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 800.0f)));
    UIRect narrow{100.0f, 0.0f, 10.0f, 10.0f};  // knob_w (16) >= w (10)
    REQUIRE(slider_value_from_pointer(narrow, ui_x, KNOB_W) == 0.0f);
}
