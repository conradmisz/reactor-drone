#pragma once

#include <cmath>

/**
 * trail_math.hpp — v3 Tier 7: position-history trails.
 *
 * Engine-free and SDL-free, like line_mesh_math: a trail is just a list of
 * world points plus a taper, so sampling and shaping are unit-testable
 * without a window.
 *
 * The points feed line_mesh::build_ribbon directly — a trail IS a glow line
 * whose points are where the entity has been. There is no separate "trail
 * renderer".
 *
 * Storage order: oldest first, newest last. That matches build_ribbon's u
 * (0 at the first point, 1 at the last), so u doubles as the head-ness of a
 * vertex: fade alpha with u and the tail dissolves for free.
 */

#include <cstddef>
#include <vector>

#include "line_mesh_math.hpp"

namespace trail {

using line_mesh::P2;

/**
 * Append `p` to a history buffer, dropping the oldest when it exceeds
 * `max_points`.
 *
 * A sample closer than `min_spacing` to the current head is IGNORED rather
 * than appended. That guard is load-bearing in two places:
 *   - a stationary entity would otherwise pack the buffer with identical
 *     points, collapsing the ribbon to nothing and feeding degenerate
 *     (zero-length) segments to the miter solver;
 *   - during hit-stop delta_time is 0, so every entity is stationary for K
 *     frames and every trail would eat itself.
 *
 * Returns true if the sample was appended.
 */
inline bool push_sample(std::vector<P2>& pts, P2 p, float min_spacing,
                        std::size_t max_points) {
    if (max_points == 0) return false;
    if (!pts.empty()) {
        const float dx = p.x - pts.back().x;
        const float dy = p.y - pts.back().y;
        if (dx * dx + dy * dy < min_spacing * min_spacing) return false;
    }
    pts.push_back(p);
    // ponytail: vector + erase(begin), not a real ring buffer. max_points is
    // ~8-24, so the shift is a few dozen bytes; swap in a deque or an index
    // ring only if trails ever get long enough to show up in a profile.
    while (pts.size() > max_points) pts.erase(pts.begin());
    return true;
}

/**
 * Per-point widths for a trail ribbon: `head_width` at the newest point,
 * tapering to `tail_width` at the oldest. n < 1 → empty; n == 1 → just the
 * head (build_ribbon rejects it anyway, which is intended — a one-point trail
 * has no length to draw).
 *
 * `exponent` shapes the taper: 1.0 is the original straight line, and > 1
 * concentrates the width at the head so the tail thins out fast (v3 Tier 10 —
 * that head-heavy profile is what reads as a tracer instead of a smear).
 * Endpoints are pinned either way.
 */
inline std::vector<float> taper_widths(std::size_t n, float head_width,
                                       float tail_width = 0.0f,
                                       float exponent = 1.0f) {
    std::vector<float> w;
    if (n == 0) return w;
    w.reserve(n);
    if (n == 1) { w.push_back(head_width); return w; }
    for (std::size_t i = 0; i < n; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(n - 1);
        if (exponent != 1.0f) t = std::pow(t, exponent);
        w.push_back(tail_width + (head_width - tail_width) * t);
    }
    return w;
}

/**
 * How many trail points fit in a per-frame vertex budget.
 *
 * build_ribbon emits 2 verts per point, so a trail of k points costs 2k.
 * Callers walk their trails in priority order and pass what is left; a trail
 * that cannot afford at least 2 points is dropped entirely rather than drawn
 * as a stub. Tail points are the ones sacrificed, since the head is what
 * reads as the projectile.
 */
inline std::size_t points_within_budget(std::size_t want_points,
                                        std::size_t verts_remaining) {
    const std::size_t affordable = verts_remaining / 2;
    if (affordable < 2) return 0;
    return want_points < affordable ? want_points : affordable;
}

}  // namespace trail
