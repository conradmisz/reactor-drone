#ifndef MINIMAP_MATH_HPP
#define MINIMAP_MATH_HPP

#include <cmath>

/**
 * minimap_math — the arena-circle -> minimap-square mapping (#7, D58).
 *
 * Pure, header-only, and free of every engine type so the whole mapping is unit
 * testable without a window, a ComponentStorage or a GameConfig. MinimapSystem
 * is a thin shell that calls these and writes the result into widget rects.
 *
 * Spaces: world is bottom-left origin. The minimap frame is a rect in the UI
 * *design canvas* (800x600, also bottom-left origin), which is the space every
 * widget rect lives in — the design->window transform is applied downstream by
 * UIRenderSystem, so the minimap is resolution-independent for free.
 *
 * The arena is a circle inscribed in that square, so an entity's offset from the
 * arena centre maps linearly onto an offset from the frame centre, scaled by
 * (frame_size/2) / arena_radius. Anything outside the arena (an enemy that has
 * drifted past the wall, a pickup nudged out by a magnet) clamps to the RIM
 * rather than to the square's edge: clamping the vector's length keeps the
 * direction honest, where clamping x and y independently would slide an
 * off-arena blip into a corner and point the player the wrong way.
 */
namespace minimap_math {

/// A rectangle in the UI design canvas, bottom-left origin — the same shape and
/// convention as UIRect, restated here so this header stays engine-free.
struct Rect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
};

/// Unit-disc position of a world point relative to an arena circle, with the
/// length clamped to 1. Returns {0,0} for a non-positive radius (degenerate
/// arena -> everything sits on the centre rather than dividing by zero).
struct UnitOffset {
    float x = 0.0f, y = 0.0f;
    bool clamped = false;   // true when the point was outside the arena circle
};

inline UnitOffset arena_unit_offset(float wx, float wy,
                                    float center_x, float center_y, float radius) {
    UnitOffset u;
    if (!(radius > 0.0f)) return u;
    float dx = (wx - center_x) / radius;
    float dy = (wy - center_y) / radius;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len > 1.0f) {
        dx /= len;
        dy /= len;
        u.clamped = true;
    }
    u.x = dx;
    u.y = dy;
    return u;
}

/**
 * Rect of one blip of edge length `blip` centred on the world point's image in
 * `frame`. The returned rect is always fully inside `frame`: the centre is first
 * pulled in by half a blip on each axis, so a rim blip sits flush against the
 * frame instead of half-hanging outside it.
 */
inline Rect blip_rect(float wx, float wy,
                      float center_x, float center_y, float radius,
                      const Rect& frame, float blip) {
    const UnitOffset u = arena_unit_offset(wx, wy, center_x, center_y, radius);
    const float half_blip = blip * 0.5f;
    // Usable half-extent: the frame's half-size minus half a blip, floored at 0
    // so a blip wider than the frame degenerates to the centre rather than
    // inverting the mapping.
    const float ex = std::max(0.0f, frame.w * 0.5f - half_blip);
    const float ey = std::max(0.0f, frame.h * 0.5f - half_blip);
    const float cx = frame.x + frame.w * 0.5f + u.x * ex;
    const float cy = frame.y + frame.h * 0.5f + u.y * ey;
    return Rect{cx - half_blip, cy - half_blip, blip, blip};
}

/// True when `r` lies entirely within `outer` (inclusive). Used by the tests as
/// the containment invariant blip_rect promises.
inline bool contains(const Rect& outer, const Rect& r) {
    return r.x >= outer.x && r.y >= outer.y &&
           r.x + r.w <= outer.x + outer.w &&
           r.y + r.h <= outer.y + outer.h;
}

}  // namespace minimap_math

#endif  // MINIMAP_MATH_HPP
