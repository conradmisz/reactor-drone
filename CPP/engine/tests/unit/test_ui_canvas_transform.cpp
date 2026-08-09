/**
 * Unit tests for the UI canvas transform (ui_render_math.hpp) — window-size
 * independence. The UI screens are authored in a fixed 800x600 design canvas;
 * ui_canvas_transform uniformly scales that canvas to fit the live window and
 * centers it, and ui_apply_transform maps a design rect to window space.
 * Exercised headlessly (no SDL).
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "engine/ecs/systems/ui_render_math.hpp"

TEST_CASE("ui_canvas_transform: identity at the design size", "[Engine][ui_canvas][unit]") {
    UICanvasTransform t = ui_canvas_transform(UI_DESIGN_WIDTH, UI_DESIGN_HEIGHT);
    CHECK(t.scale == Catch::Approx(1.0f));
    CHECK(t.offset_x == Catch::Approx(0.0f));
    CHECK(t.offset_y == Catch::Approx(0.0f));
}

TEST_CASE("ui_canvas_transform: 980x660 scales 1.1 and centers horizontally",
          "[Engine][ui_canvas][unit]") {
    // sx = 980/800 = 1.225, sy = 660/600 = 1.1 -> scale = 1.1 (the min, fit).
    UICanvasTransform t = ui_canvas_transform(980.0f, 660.0f);
    CHECK(t.scale == Catch::Approx(1.1f));
    CHECK(t.offset_x == Catch::Approx((980.0f - 800.0f * 1.1f) * 0.5f));  // = 50
    CHECK(t.offset_y == Catch::Approx(0.0f).margin(1e-3));                // 600*1.1 ~= 660 (float)
}

TEST_CASE("ui_canvas_transform: extra-wide window letterboxes left/right",
          "[Engine][ui_canvas][unit]") {
    UICanvasTransform t = ui_canvas_transform(1600.0f, 600.0f);
    CHECK(t.scale == Catch::Approx(1.0f));            // height-limited
    CHECK(t.offset_x == Catch::Approx(400.0f));       // (1600-800)/2
    CHECK(t.offset_y == Catch::Approx(0.0f));
}

TEST_CASE("ui_canvas_transform: extra-tall window letterboxes top/bottom",
          "[Engine][ui_canvas][unit]") {
    UICanvasTransform t = ui_canvas_transform(800.0f, 1200.0f);
    CHECK(t.scale == Catch::Approx(1.0f));            // width-limited
    CHECK(t.offset_x == Catch::Approx(0.0f));
    CHECK(t.offset_y == Catch::Approx(300.0f));       // (1200-600)/2
}

TEST_CASE("ui_canvas_transform: degenerate window dims yield identity",
          "[Engine][ui_canvas][unit]") {
    UICanvasTransform a = ui_canvas_transform(0.0f, 600.0f);
    UICanvasTransform b = ui_canvas_transform(800.0f, -5.0f);
    CHECK(a.scale == Catch::Approx(1.0f));
    CHECK(a.offset_x == Catch::Approx(0.0f));
    CHECK(b.scale == Catch::Approx(1.0f));
    CHECK(b.offset_y == Catch::Approx(0.0f));
}

TEST_CASE("ui_apply_transform: maps a design rect into window space",
          "[Engine][ui_canvas][unit]") {
    UICanvasTransform t = ui_canvas_transform(980.0f, 660.0f);  // scale 1.1, off (50,0)
    UIRect r = ui_apply_transform(t, UIRect{300.0f, 150.0f, 200.0f, 300.0f});
    CHECK(r.x == Catch::Approx(50.0f + 300.0f * 1.1f));
    CHECK(r.y == Catch::Approx(0.0f + 150.0f * 1.1f));
    CHECK(r.w == Catch::Approx(200.0f * 1.1f));
    CHECK(r.h == Catch::Approx(300.0f * 1.1f));
}

TEST_CASE("ui_apply_transform: identity transform leaves the rect unchanged",
          "[Engine][ui_canvas][unit]") {
    UICanvasTransform t = ui_canvas_transform(UI_DESIGN_WIDTH, UI_DESIGN_HEIGHT);
    UIRect r = ui_apply_transform(t, UIRect{300.0f, 300.0f, 200.0f, 50.0f});
    CHECK(r.x == Catch::Approx(300.0f));
    CHECK(r.y == Catch::Approx(300.0f));
    CHECK(r.w == Catch::Approx(200.0f));
    CHECK(r.h == Catch::Approx(50.0f));
}
