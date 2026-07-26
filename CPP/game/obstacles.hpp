/**
 * obstacles.hpp — pure obstacle-collision resolution (v2, Phase 6).
 *
 * Obstacles are solid, static, axis-aligned boxes. The drone is treated as a
 * circle; `push_circle_out_of_aabb` returns the smallest-displacement circle
 * centre that no longer penetrates the box (see main.cpp, applied after the
 * arena clamp). Bottom-left origin, matching world space. Pure + side-effect
 * free so it unit/property-tests without a game loop.
 */
#ifndef OBSTACLES_HPP
#define OBSTACLES_HPP

#include <algorithm>
#include <cmath>

struct Vec2 { float x = 0.0f, y = 0.0f; };

/**
 * Move a circle (centre c, radius r) out of the AABB [ax,ax+aw]x[ay,ay+ah],
 * returning the resolved centre. If the circle does not overlap the box the
 * centre is returned unchanged (tangent counts as clear). When the centre is
 * inside the box it is ejected through the nearest face.
 */
inline Vec2 push_circle_out_of_aabb(float cx, float cy, float r,
                                    float ax, float ay, float aw, float ah) {
    float qx = std::clamp(cx, ax, ax + aw);   // closest point on the box
    float qy = std::clamp(cy, ay, ay + ah);
    float dx = cx - qx, dy = cy - qy;
    float d2 = dx * dx + dy * dy;

    if (d2 >= r * r) return {cx, cy};          // no overlap

    if (d2 > 1e-12f) {                         // centre outside: push along normal
        float d = std::sqrt(d2);
        float s = r / d;
        return {qx + dx * s, qy + dy * s};
    }

    // Centre inside the box: eject through whichever face is nearest.
    float left   = cx - ax,        right = (ax + aw) - cx;
    float bottom = cy - ay,        top   = (ay + ah) - cy;
    float m = std::min(std::min(left, right), std::min(bottom, top));
    if (m == left)   return {ax - r, cy};
    if (m == right)  return {ax + aw + r, cy};
    if (m == bottom) return {cx, ay - r};
    return {cx, ay + ah + r};
}

#endif  // OBSTACLES_HPP
