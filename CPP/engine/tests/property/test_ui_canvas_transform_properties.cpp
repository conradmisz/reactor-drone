/**
 * Property tests for the UI canvas transform (ui_render_math.hpp) — window-size
 * independence. For any window size the transform is a uniform fit-and-center
 * map of the 800x600 design canvas. Bounded 10x5.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include "engine/ecs/systems/ui_render_math.hpp"

// Bounded property-test iteration counts (property-test-bounds steering).
constexpr int NUM_OUTER_TESTS = 10;  // window sizes
constexpr int NUM_INNER_TESTS = 5;   // design points per window

TEST_CASE("ui_canvas_transform: scale is positive and aspect-preserving",
          "[Engine][ui_canvas][property]") {
    auto w = GENERATE(take(NUM_OUTER_TESTS, random(64.0f, 4000.0f)));
    auto h = GENERATE(take(NUM_INNER_TESTS, random(64.0f, 4000.0f)));
    UICanvasTransform t = ui_canvas_transform(w, h);
    REQUIRE(t.scale > 0.0f);
    // The scaled canvas never exceeds the window on either axis (it fits).
    REQUIRE(UI_DESIGN_WIDTH  * t.scale <= w + 1e-3f);
    REQUIRE(UI_DESIGN_HEIGHT * t.scale <= h + 1e-3f);
    // Offsets are non-negative (canvas centered within the window, never cropped).
    REQUIRE(t.offset_x >= -1e-3f);
    REQUIRE(t.offset_y >= -1e-3f);
}

TEST_CASE("ui_canvas_transform: the design-canvas center maps to the window center",
          "[Engine][ui_canvas][property]") {
    auto w = GENERATE(take(NUM_OUTER_TESTS, random(64.0f, 4000.0f)));
    auto h = GENERATE(take(NUM_INNER_TESTS, random(64.0f, 4000.0f)));
    UICanvasTransform t = ui_canvas_transform(w, h);
    UIRect center = ui_apply_transform(t, UIRect{UI_DESIGN_WIDTH * 0.5f,
                                                 UI_DESIGN_HEIGHT * 0.5f, 0.0f, 0.0f});
    REQUIRE(center.x == Catch::Approx(w * 0.5f).margin(1e-2));
    REQUIRE(center.y == Catch::Approx(h * 0.5f).margin(1e-2));
}

TEST_CASE("ui_canvas_transform: a design point inside the canvas stays inside the window",
          "[Engine][ui_canvas][property]") {
    auto w  = GENERATE(take(NUM_OUTER_TESTS, random(64.0f, 4000.0f)));
    auto h  = GENERATE(take(NUM_OUTER_TESTS, random(64.0f, 4000.0f)));
    auto dx = GENERATE(take(NUM_INNER_TESTS, random(0.0f, UI_DESIGN_WIDTH)));
    auto dy = GENERATE(take(NUM_INNER_TESTS, random(0.0f, UI_DESIGN_HEIGHT)));
    UICanvasTransform t = ui_canvas_transform(w, h);
    UIRect p = ui_apply_transform(t, UIRect{dx, dy, 0.0f, 0.0f});
    REQUIRE(p.x >= -1e-3f);
    REQUIRE(p.x <= w + 1e-3f);
    REQUIRE(p.y >= -1e-3f);
    REQUIRE(p.y <= h + 1e-3f);
}
