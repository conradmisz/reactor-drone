/**
 * Unit tests for ui_render_math.hpp — pure render-decision helpers.
 *
 * Exercises the four input-varying rendering decisions UIRenderSystem relies
 * on, as pure functions with NO SDL and no window:
 *   - compute_centered_text_origin (button label centering)
 *   - sort_widgets_by_draw_order   (z_order ascending, entity-id tiebreak)
 *   - to_sdl_y                      (the one bottom-left -> top-left Y-flip)
 *   - resolve_widget_state          (disabled > pressed > hovered > normal)
 *
 * Requirements tested: R12.2, R12.3, R4.3, R4.4, R9, R11.3, R11.4
 */

#include <catch2/catch_test_macros.hpp>

#include "engine/ecs/systems/ui_render_math.hpp"

#include <cmath>
#include <vector>

/**
 * Test: Button label centering produces equal margins on both axes.
 * Requirement R12.2
 *
 * For a representative rect and text size, the centered text bounding box must
 * have equal left/right margins and equal top/bottom margins (within 1.0px),
 * and must match the exact centering formula.
 */
TEST_CASE("compute_centered_text_origin: label is centered with equal margins",
          "[Engine][ui][unit]") {
    const UIRect rect{300.0f, 230.0f, 200.0f, 50.0f};
    const float text_w = 80.0f;
    const float text_h = 24.0f;

    TextOrigin origin = compute_centered_text_origin(rect, text_w, text_h);

    // Horizontal margins.
    const float left_margin  = origin.x - rect.x;
    const float right_margin = (rect.x + rect.w) - (origin.x + text_w);
    CHECK(std::abs(left_margin - right_margin) < 1.0f);

    // Vertical margins.
    const float bottom_margin = origin.y - rect.y;
    const float top_margin    = (rect.y + rect.h) - (origin.y + text_h);
    CHECK(std::abs(bottom_margin - top_margin) < 1.0f);

    // Exact centering formula.
    CHECK(origin.x == rect.x + (rect.w - text_w) / 2.0f);
    CHECK(origin.y == rect.y + (rect.h - text_h) / 2.0f);
}

/**
 * Test: z_order orders widgets ascending — higher z_order is drawn later.
 * Requirement R12.3
 *
 * A panel with z_order 10 must appear AFTER a panel with z_order 5 in the
 * sorted list (lower z_order drawn first / underneath).
 */
TEST_CASE("sort_widgets_by_draw_order: ascending z_order, higher drawn later",
          "[Engine][ui][unit]") {
    // Insert the higher z_order first to prove the sort reorders it.
    std::vector<DrawItem> items{
        {10, /*entity*/ 1},
        {5,  /*entity*/ 2},
    };

    sort_widgets_by_draw_order(items);

    // Find the positions of each z_order in the sorted list.
    int pos_z5 = -1;
    int pos_z10 = -1;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (items[i].z_order == 5)  pos_z5 = i;
        if (items[i].z_order == 10) pos_z10 = i;
    }

    REQUIRE(pos_z5 != -1);
    REQUIRE(pos_z10 != -1);
    CHECK(pos_z10 > pos_z5);  // z_order 10 drawn AFTER z_order 5
}

/**
 * Test: equal z_order breaks ties by ascending entity id.
 * Requirement R12.3
 */
TEST_CASE("sort_widgets_by_draw_order: equal z_order tiebreaks by entity id",
          "[Engine][ui][unit]") {
    // Same z_order, higher entity id inserted first.
    std::vector<DrawItem> items{
        {7, /*entity*/ 42},
        {7, /*entity*/ 17},
    };

    sort_widgets_by_draw_order(items);

    CHECK(items[0].entity == 17u);  // lower entity id first
    CHECK(items[1].entity == 42u);
}

/**
 * Test: to_sdl_y applies the bottom-left -> top-left flip with no special case.
 * Requirements R4.3, R4.4, R11.4
 *
 * Formula: sdl_y = window_height - y - height (same as RenderSystem).
 * Zero-height case is NOT special-cased: 600 - 100 - 0 == 500.
 */
TEST_CASE("to_sdl_y: flips bottom-left Y, no zero-height special case",
          "[Engine][ui][unit]") {
    CHECK(to_sdl_y(600.0f, 100.0f, 50.0f) == 450.0f);
    CHECK(to_sdl_y(600.0f, 100.0f, 0.0f) == 500.0f);
}

/**
 * Test: resolve_widget_state precedence is disabled > pressed > hovered > normal.
 * Requirements R9, R11.3
 */
TEST_CASE("resolve_widget_state: precedence disabled > pressed > hovered > normal",
          "[Engine][ui][unit]") {
    SECTION("disabled wins over everything") {
        UIState s;
        s.disabled = true;
        s.pressed  = true;
        s.hovered  = true;
        CHECK(resolve_widget_state(s) == WidgetState::Disabled);
    }

    SECTION("pressed wins over hovered when not disabled") {
        UIState s;
        s.pressed = true;
        s.hovered = true;
        CHECK(resolve_widget_state(s) == WidgetState::Pressed);
    }

    SECTION("hovered wins over normal when not pressed/disabled") {
        UIState s;
        s.hovered = true;
        CHECK(resolve_widget_state(s) == WidgetState::Hovered);
    }

    SECTION("no flags set resolves to Normal") {
        UIState s;
        CHECK(resolve_widget_state(s) == WidgetState::Normal);
    }

    SECTION("no-arg overload resolves to Normal") {
        CHECK(resolve_widget_state() == WidgetState::Normal);
    }
}
