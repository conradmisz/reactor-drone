/**
 * bullet_bounce.hpp — pure reflection math for the Ricochet Coils upgrade (D98).
 *
 * Two surfaces exist in this arena: solid obstacle AABBs and the boundary ring
 * (a circle the drone is clamped inside). Both reflections are the same three
 * steps — find the outward surface normal, put the shot back on the clear side
 * of the surface, mirror the velocity through the normal — so both live here as
 * free functions, pure and window-free like aim_math / minimap_math.
 *
 * The AABB normal is NOT derived a second time: push_circle_out_of_aabb already
 * resolves a penetrating circle to the nearest clear centre, and the direction
 * it moved the circle IS the outward normal (obstacles.hpp).
 */
#ifndef BULLET_BOUNCE_HPP
#define BULLET_BOUNCE_HPP

#include "obstacles.hpp"   // Vec2, push_circle_out_of_aabb
#include <cmath>

namespace bounce {

/// How far past the surface a bounced shot is placed, in px. Without it the shot
/// re-overlaps the same surface next frame and burns its whole bounce budget on
/// one wall. 0.5 px is under a tenth of the 6 px shot radius, so it never reads
/// as the shot skipping.
inline constexpr float CLEARANCE = 0.5f;

/// The result of a reflection: where the shot's CENTRE goes and its new velocity.
struct Result {
    float cx = 0.0f, cy = 0.0f;
    float vx = 0.0f, vy = 0.0f;
};

/// v mirrored through the unit normal n, i.e. v - 2(v.n)n.
inline void mirror(float vx, float vy, float nx, float ny, Result& out) {
    const float d = vx * nx + vy * ny;
    out.vx = vx - 2.0f * d * nx;
    out.vy = vy - 2.0f * d * ny;
}

/**
 * Reflect a circle (centre c, radius r) travelling at v off the AABB
 * [ax,ax+aw]x[ay,ay+ah]. Returns false — leaving `out` untouched — when the
 * circle does not penetrate the box, or when it is already travelling away from
 * the surface (a shot that clipped a corner and is leaving must not be bounced
 * back into the box).
 */
inline bool off_aabb(float cx, float cy, float r, float vx, float vy,
                     float ax, float ay, float aw, float ah, Result& out) {
    const Vec2 clear = push_circle_out_of_aabb(cx, cy, r, ax, ay, aw, ah);
    float nx = clear.x - cx, ny = clear.y - cy;
    const float len = std::sqrt(nx * nx + ny * ny);
    if (len < 1e-6f) return false;              // no overlap: nothing to reflect off
    nx /= len; ny /= len;
    if (vx * nx + vy * ny >= 0.0f) return false;  // already leaving
    mirror(vx, vy, nx, ny, out);
    out.cx = clear.x + nx * CLEARANCE;
    out.cy = clear.y + ny * CLEARANCE;
    return true;
}

/**
 * Reflect a circle off the INSIDE of the arena ring (centre `ox,oy`, radius R).
 * Returns false while the shot is still inside the ring or already heading back
 * in. The shot is placed just inside the ring, mirroring off_aabb's clearance.
 */
inline bool inside_circle(float cx, float cy, float r, float vx, float vy,
                          float ox, float oy, float R, Result& out) {
    const float dx = cx - ox, dy = cy - oy;
    const float d = std::sqrt(dx * dx + dy * dy);
    const float limit = R - r;
    if (d <= limit || d < 1e-6f) return false;
    const float nx = -dx / d, ny = -dy / d;      // inward normal
    if (vx * nx + vy * ny >= 0.0f) return false;
    mirror(vx, vy, nx, ny, out);
    out.cx = ox + dx / d * (limit - CLEARANCE);
    out.cy = oy + dy / d * (limit - CLEARANCE);
    return true;
}

}  // namespace bounce

#endif  // BULLET_BOUNCE_HPP
