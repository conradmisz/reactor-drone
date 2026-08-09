/**
 * ui_render_math.hpp — Pure render-decision helpers for UIRenderSystem.
 *
 * Header-only, inline, and deliberately free of any SDL dependency so that
 * every input-varying rendering decision (widget-state resolution, centered
 * text origin, the bottom-left-to-top-left Y-flip, and z-order sorting) is a
 * pure function that property and unit tests can exercise directly, without a
 * window. UIRenderSystem is a thin SDL shell that calls these helpers.
 *
 * Bottom-left origin convention is preserved: to_sdl_y() is the ONLY Y-flip
 * UIRenderSystem performs, using the same formula as RenderSystem::draw_entity()
 * (sdl_y = window_height - y - height), with no branch for zero-size rects.
 *
 * Added in Phase 2 (o-040-02-widget-rendering).
 */

#pragma once

#include "engine/ecs/components.hpp"   // UIRect, UIState, Entity
#include "engine/ui_style.hpp"         // WidgetState

#include <algorithm>
#include <cmath>     // std::isfinite
#include <cstdint>
#include <vector>

/**
 * Resolve the interaction state used to select a widget's colors.
 *
 * Precedence (highest first): disabled > pressed > hovered > normal.
 * UIState is never mutated in Phase 2, so this returns Normal in practice;
 * the precedence is implemented now so Phase 3 interaction code reuses it
 * unchanged.
 */
inline WidgetState resolve_widget_state(const UIState& s) {
    if (s.disabled) return WidgetState::Disabled;
    if (s.pressed)  return WidgetState::Pressed;
    if (s.hovered)  return WidgetState::Hovered;
    return WidgetState::Normal;
}

/**
 * Overload for widgets that carry no UIState component — always Normal.
 */
inline WidgetState resolve_widget_state() {
    return WidgetState::Normal;
}

/**
 * Centered text origin in BOTTOM-LEFT coordinates.
 *
 * x is the text left edge, y is the text bottom edge. The result is exact
 * with no clamping: a negative offset is permitted (and returned) when the
 * text is wider or taller than the rect.
 */
struct TextOrigin {
    float x;  // left edge   (bottom-left origin)
    float y;  // bottom edge (bottom-left origin)
};

inline TextOrigin compute_centered_text_origin(const UIRect& rect,
                                               float text_w, float text_h) {
    return { rect.x + (rect.w - text_w) / 2.0f,
             rect.y + (rect.h - text_h) / 2.0f };
}

/**
 * The one and only Y-flip formula used by UIRenderSystem.
 *
 * Converts a bottom-left-origin Y (with the drawable's height) to the SDL
 * top-left-origin Y. There is intentionally NO branch for zero-width or
 * zero-height rects: a zero-height widget at y yields window_height - y.
 */
inline float to_sdl_y(float window_height, float y, float height) {
    return window_height - y - height;
}

/**
 * Map an SDL top-left screen-pixel Y to the bottom-left-origin UI Y.
 *
 * Used by UISystem to bring the SDL pointer into the same bottom-left space
 * the widget rects live in. The X axis is identity (ui_x = screen_x), so no
 * X helper is needed. No camera/zoom/lookat transform is applied here — this
 * is the pointer Y-inversion only.
 *
 * Added in Phase 3 (o-040-03-button-interaction).
 */
inline float to_ui_y(float window_height, float screen_y) {
    return window_height - screen_y;
}

/**
 * Inclusive point-in-rect test in the bottom-left-origin space.
 *
 * A point exactly on any of the four edges (or corners) is INSIDE. Pure and
 * total: no clamping, no special-casing of zero-size rects.
 *
 * Added in Phase 3 (o-040-03-button-interaction).
 */
inline bool point_in_rect(float px, float py, const UIRect& rect) {
    return px >= rect.x && px <= rect.x + rect.w &&
           py >= rect.y && py <= rect.y + rect.h;
}

/**
 * Window-size independence: the fixed DESIGN CANVAS the UI screens are authored
 * in. Every widget rect in GameData.json is expressed in these bottom-left-origin
 * coordinates. At render and hit-test time the canvas is uniformly scaled to fit
 * the live window and centered (letterbox), so the menus look identical at any
 * window size instead of sitting off-center when the window differs from 800x600.
 *
 * Added in the Class-090 window-size sync follow-up.
 */
constexpr float UI_DESIGN_WIDTH  = 800.0f;
constexpr float UI_DESIGN_HEIGHT = 600.0f;

/**
 * Uniform fit-and-center mapping from the design canvas to the live window.
 * scale is the per-axis minimum (so the whole canvas fits and aspect ratio is
 * preserved); the scaled canvas is centered, leaving equal margins.
 */
struct UICanvasTransform {
    float scale = 1.0f;
    float offset_x = 0.0f;  // bottom-left origin, window space
    float offset_y = 0.0f;
};

/**
 * Compute the canvas transform for a window of (window_w, window_h). At the
 * design size (800x600) this is the identity (scale 1, zero offset). Degenerate
 * (non-positive) window dimensions yield the identity, so a frame before the
 * window size is known is a safe no-op. Pure and SDL-free.
 */
inline UICanvasTransform ui_canvas_transform(float window_w, float window_h) {
    UICanvasTransform t;
    if (window_w <= 0.0f || window_h <= 0.0f) return t;
    const float sx = window_w / UI_DESIGN_WIDTH;
    const float sy = window_h / UI_DESIGN_HEIGHT;
    t.scale = sx < sy ? sx : sy;
    t.offset_x = (window_w - UI_DESIGN_WIDTH  * t.scale) * 0.5f;
    t.offset_y = (window_h - UI_DESIGN_HEIGHT * t.scale) * 0.5f;
    return t;
}

/**
 * Map a design-space rect to window space (bottom-left origin) under the
 * transform. The renderer draws — and the hit-test compares against — this
 * window-space rect, so the two always agree.
 */
inline UIRect ui_apply_transform(const UICanvasTransform& t, const UIRect& r) {
    return UIRect{ t.offset_x + r.x * t.scale,
                   t.offset_y + r.y * t.scale,
                   r.w * t.scale,
                   r.h * t.scale };
}

/**
 * Alpha floor a pulsing widget dips to at the trough of its cycle. Not zero:
 * a widget that vanishes entirely reads as a rendering fault rather than as an
 * invitation to click it.
 */
constexpr float UI_PULSE_FLOOR = 0.35f;

/**
 * Alpha multiplier for a pulsing widget at elapsed time `t` seconds.
 *
 * Returns 1.0 when hz <= 0 (a static widget, the default) or when t is not
 * finite, so a widget that never opted in is bit-identical to one rendered
 * before pulsing existed. Otherwise it is a raised cosine sweeping the full
 * [UI_PULSE_FLOOR, 1] range once per 1/hz seconds, peaking at t = 0 so a widget
 * appears at full opacity on the frame its screen is pushed.
 *
 * Pure and SDL-free: the value is applied to the resolved bg/text alpha only,
 * never to a rect, so the drawn area and the hit-test area can never disagree.
 */
inline float pulse_alpha_scale(float hz, float t) {
    if (!(hz > 0.0f) || !std::isfinite(t)) return 1.0f;
    const float pi = 3.14159265358979323846f;
    // 0.5*(1+cos) is 1 at t=0 and 0 at the half-cycle; map that onto [floor, 1].
    const float wave = 0.5f * (1.0f + std::cos(2.0f * pi * hz * t));
    return UI_PULSE_FLOOR + (1.0f - UI_PULSE_FLOOR) * wave;
}

/**
 * Scale a colour's alpha by a multiplier, clamped and rounded into 0..255.
 * Separated from pulse_alpha_scale so the wave and its application are each
 * testable on their own.
 */
inline Color apply_alpha_scale(const Color& c, float scale) {
    float a = static_cast<float>(c.a) * scale;
    a = std::max(0.0f, std::min(255.0f, a));
    return Color{c.r, c.g, c.b, static_cast<uint8_t>(a + 0.5f)};
}

/**
 * A widget to be drawn, paired with the keys that order it. The renderer
 * reads the full UIElement/UIState by entity after the list is sorted.
 */
struct DrawItem {
    int    z_order;
    Entity entity;
};

/**
 * Order widgets for drawing: ascending z_order, ties broken by ascending
 * entity id. A widget with a lower z_order is drawn before (underneath) a
 * widget with a higher z_order.
 */
inline void sort_widgets_by_draw_order(std::vector<DrawItem>& items) {
    std::sort(items.begin(), items.end(), [](const DrawItem& a, const DrawItem& b) {
        if (a.z_order != b.z_order) return a.z_order < b.z_order;
        return a.entity < b.entity;
    });
}

/**
 * Horizontal CENTER x of a slider knob of width knob_w on a track equal to the
 * widget rect, given a normalized UIState value in [0, 1].
 *
 * The value is clamped to [0, 1] (and a non-finite value — NaN/inf — is treated
 * as 0) so the knob ALWAYS stays within the track. The knob center sweeps from
 * the left limit (rect.x + knob_w/2) at value 0 to the right limit
 * (rect.x + rect.w - knob_w/2) at value 1, so the knob's extent
 * [center - knob_w/2, center + knob_w/2] stays inside [rect.x, rect.x + rect.w].
 * If the knob is at least as wide as the track, the center is the track midpoint.
 * Monotonic non-decreasing in value within [0, 1].
 *
 * Added in Phase 5 (o-040-05-json-layout).
 */
inline float slider_knob_center_x(const UIRect& rect, float value, float knob_w) {
    float v = std::isfinite(value) ? value : 0.0f;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;

    if (knob_w >= rect.w) {
        return rect.x + rect.w * 0.5f;          // knob wider than track -> centered
    }
    const float left  = rect.x + knob_w * 0.5f;
    const float right = rect.x + rect.w - knob_w * 0.5f;
    return left + v * (right - left);
}

/**
 * Inverse of slider_knob_center_x: the normalized value in [0, 1] implied by a
 * pointer x over a track equal to the widget rect, given the knob width.
 *
 * The usable center range is [rect.x + knob_w/2, rect.x + rect.w - knob_w/2]; a
 * pointer left of / right of that range clamps to 0 / 1. If the knob is at least
 * as wide as the track, the value is 0 (degenerate track — no usable travel),
 * avoiding a divide-by-zero. The result is ALWAYS in [0, 1], and feeding a value
 * v in [0,1] through slider_knob_center_x and back recovers v (round-trip).
 *
 * Added in Phase 6 (o-040-06-lua-screens).
 */
inline float slider_value_from_pointer(const UIRect& rect, float ui_x, float knob_w) {
    if (knob_w >= rect.w) return 0.0f;
    const float left  = rect.x + knob_w * 0.5f;
    const float right = rect.x + rect.w - knob_w * 0.5f;
    float v = (ui_x - left) / (right - left);
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return v;
}

/**
 * Whether a checkbox is checked: true iff its UIState value is non-zero.
 *
 * A non-finite value (NaN/inf) is treated as non-zero -> checked, since the
 * comparison value != 0.0f is true for any non-zero finite value and for
 * NaN/inf alike (NaN != 0.0f is true; inf != 0.0f is true).
 *
 * Added in Phase 5 (o-040-05-json-layout).
 */
inline bool checkbox_is_checked(float value) {
    return value != 0.0f;
}
