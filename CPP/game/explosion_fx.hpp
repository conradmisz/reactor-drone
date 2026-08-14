/**
 * explosion_fx.hpp — pure staged geometry for the v3 Tier 11 layered enemy
 * explosion (D203).
 *
 * The explosion is four layers on one timeline: the existing 8-frame sprite
 * clip is the flash, a shockwave RING expands out of it, debris SHARDS fly,
 * and the Tier 9 particle embers fade last. Ring and shards are drawn as
 * ordinary neon GlowLines — the Tier 5 renderer already draws exactly this
 * shape, so none of this needs a renderer of its own.
 *
 * `t` is clip progress in [0,1], taken from the effect entity's own
 * SpriteSheet (current_frame / (total_frames - 1)). Nothing here holds state
 * and nothing allocates a component: the animation the game already ticks IS
 * the clock, so the whole effect stays presentation-only by construction.
 *
 * Angles are seeded from the effect entity's id, NOT from an RNG — two
 * explosions differ, but a replay of the same run draws the same debris.
 */
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/ecs/systems/line_mesh_math.hpp"

namespace explosion_fx {

/** Ring radius at clip progress `t`: ease-OUT, so it leaps outward and settles
 *  rather than growing at a constant rate. Endpoints are exact. */
inline float ring_radius(float t, float r0, float r1) {
    const float e = 1.0f - (1.0f - t) * (1.0f - t);
    return r0 + (r1 - r0) * e;
}

/** Ring brightness at `t`: bright immediately, gone by the end of the clip.
 *  Squared so it holds its punch early and drops away late. */
inline float ring_alpha(float t) {
    const float k = 1.0f - t;
    return k * k;
}

/** A closed circle as a polyline, first point repeated last so the ribbon has
 *  no seam. `segments` >= 3. */
inline std::vector<line_mesh::P2> ring_points(float cx, float cy, float radius,
                                              int segments) {
    std::vector<line_mesh::P2> pts;
    if (segments < 3) return pts;
    pts.reserve(static_cast<std::size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) /
                        static_cast<float>(segments);
        pts.push_back(line_mesh::P2{cx + std::cos(a) * radius,
                                    cy + std::sin(a) * radius});
    }
    return pts;
}

/** Where a debris shard's near and far ends sit, as distances from the centre. */
struct Span {
    float inner = 0.0f;
    float outer = 0.0f;
};

/**
 * Shards hold off while the flash reads (t < HOLD), then stretch out of the
 * centre and travel, trailing their inner end behind so the streak keeps a
 * length instead of stretching forever.
 */
inline Span shard_span(float t, float reach = 70.0f) {
    constexpr float HOLD = 0.12f;
    if (t <= HOLD) return Span{0.0f, 0.0f};
    const float u = (t - HOLD) / (1.0f - HOLD);       // 0..1 after the hold
    const float head = reach * (1.0f - (1.0f - u) * (1.0f - u));
    // The tail follows at a fraction of the head, so the streak has body early
    // and thins as it flies.
    const float tail = head * (0.35f + 0.5f * u);
    return Span{tail, head};
}

/**
 * Angle of shard `i` of `n`, offset by a per-explosion rotation derived from
 * `seed` (the effect entity's id). Evenly spread — the offset only rotates the
 * whole fan, so no two explosions line up but every one stays balanced.
 */
inline float shard_angle(std::size_t i, std::size_t n, std::uint32_t seed) {
    if (n == 0) return 0.0f;
    // Cheap integer hash (xorshift-ish) folded into [0, 2pi).
    std::uint32_t h = seed * 2654435761u;
    h ^= h >> 15;
    const float offset = 6.2831853f * (static_cast<float>(h & 0xFFFFu) / 65536.0f);
    return offset + 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
}

}  // namespace explosion_fx
