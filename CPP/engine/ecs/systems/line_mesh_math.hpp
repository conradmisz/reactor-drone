/**
 * line_mesh_math.hpp — pure geometry for the neon line renderer (v3 Tier 5, D211).
 *
 * Turns a polyline into a triangle-strip ribbon whose cross-section UV drives a
 * soft-falloff glow texture. Engine-free and SDL-free: verts are plain structs,
 * so every join/width/UV decision is unit-testable without a window.
 *
 * Convention: points are WORLD-space (bottom-left origin). The render system
 * applies camera transform + Y-flip when it converts to SDL_Vertex — this
 * header never sees screen space.
 */
#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

namespace line_mesh {

struct P2 { float x = 0.0f, y = 0.0f; };

/** One ribbon vertex: world position + cross-section coordinate v in [0,1]
 *  (0 = left edge, 0.5 = spine, 1 = right edge). u runs along the line. */
struct RibbonVert {
    float x, y;
    float u, v;
};

/** Unit normal of segment a->b (left-hand side). Zero-length segments yield
 *  {0,0} so a degenerate polyline never divides by zero. */
inline P2 segment_normal(P2 a, P2 b) {
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0f) return P2{0.0f, 0.0f};
    return P2{-dy / len, dx / len};
}

/**
 * Per-point miter normals: the average of adjacent segment normals, scaled so
 * the ribbon keeps its width through the corner (1/cos(theta/2)), clamped to
 * `miter_limit` so a hairpin cannot spike to infinity. Endpoints use their
 * single segment's normal. Fewer than 2 points → empty.
 */
inline std::vector<P2> miter_normals(const std::vector<P2>& pts,
                                     float miter_limit = 3.0f) {
    std::vector<P2> out;
    const std::size_t n = pts.size();
    if (n < 2) return out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        P2 nrm;
        if (i == 0) {
            nrm = segment_normal(pts[0], pts[1]);
        } else if (i == n - 1) {
            nrm = segment_normal(pts[n - 2], pts[n - 1]);
        } else {
            P2 n0 = segment_normal(pts[i - 1], pts[i]);
            P2 n1 = segment_normal(pts[i], pts[i + 1]);
            float mx = n0.x + n1.x, my = n0.y + n1.y;
            float len = std::sqrt(mx * mx + my * my);
            if (len <= 1e-6f) {
                // 180° hairpin: fall back to the incoming normal at full width.
                nrm = n0;
            } else {
                mx /= len; my /= len;
                // Width preservation: dot(miter, segment normal) = cos(theta/2).
                float cos_half = mx * n0.x + my * n0.y;
                float scale = cos_half > 1e-6f ? 1.0f / cos_half : miter_limit;
                if (scale > miter_limit) scale = miter_limit;
                nrm = P2{mx * scale, my * scale};
            }
        }
        out.push_back(nrm);
    }
    return out;
}

/**
 * Build the ribbon as a triangle-strip vertex sequence: for each polyline
 * point, a left-edge vert (v=0) then a right-edge vert (v=1), offset by the
 * miter normal at ±width/2. u accumulates normalized arc length (0 at the
 * first point, 1 at the last; a zero-length polyline puts u=0 everywhere).
 * Fewer than 2 points or width <= 0 → empty.
 */
inline std::vector<RibbonVert> build_ribbon(const std::vector<P2>& pts,
                                            const std::vector<float>& widths,
                                            float miter_limit = 3.0f);

inline std::vector<RibbonVert> build_ribbon(const std::vector<P2>& pts,
                                            float width,
                                            float miter_limit = 3.0f) {
    if (pts.size() < 2 || width <= 0.0f) return {};
    return build_ribbon(pts, std::vector<float>(pts.size(), width), miter_limit);
}

/**
 * v3 Tier 7: the per-point-width form, for trails that taper from a wide
 * head to a vanishing tail. `widths` must have one entry per point; a size
 * mismatch, fewer than 2 points, or a non-positive width at EVERY point
 * yields an empty ribbon. Individual zero widths are legal and are what a
 * taper's tail end looks like.
 */
inline std::vector<RibbonVert> build_ribbon(const std::vector<P2>& pts,
                                            const std::vector<float>& widths,
                                            float miter_limit) {
    std::vector<RibbonVert> out;
    const std::size_t n = pts.size();
    if (n < 2 || widths.size() != n) return out;
    bool any_positive = false;
    for (float w : widths) if (w > 0.0f) { any_positive = true; break; }
    if (!any_positive) return out;

    // Arc lengths for u.
    std::vector<float> arc(n, 0.0f);
    float total = 0.0f;
    for (std::size_t i = 1; i < n; ++i) {
        float dx = pts[i].x - pts[i - 1].x, dy = pts[i].y - pts[i - 1].y;
        total += std::sqrt(dx * dx + dy * dy);
        arc[i] = total;
    }

    const auto normals = miter_normals(pts, miter_limit);
    out.reserve(n * 2);
    for (std::size_t i = 0; i < n; ++i) {
        float u = total > 0.0f ? arc[i] / total : 0.0f;
        const float half = widths[i] * 0.5f;
        out.push_back(RibbonVert{pts[i].x + normals[i].x * half,
                                 pts[i].y + normals[i].y * half, u, 0.0f});
        out.push_back(RibbonVert{pts[i].x - normals[i].x * half,
                                 pts[i].y - normals[i].y * half, u, 1.0f});
    }
    return out;
}

/**
 * Expand a triangle-strip vertex sequence into an index list of discrete
 * triangles (what SDL_RenderGeometry consumes). 2k verts -> 2(k-1) triangles.
 * Winding alternates as in a standard strip; SDL does not cull, so winding
 * order is cosmetic. Fewer than 4 verts → empty.
 */
inline std::vector<int> strip_indices(std::size_t vert_count) {
    std::vector<int> idx;
    if (vert_count < 4) return idx;
    idx.reserve((vert_count - 2) * 3);
    for (std::size_t i = 0; i + 2 < vert_count; ++i) {
        idx.push_back(static_cast<int>(i));
        idx.push_back(static_cast<int>(i + 1));
        idx.push_back(static_cast<int>(i + 2));
    }
    return idx;
}

/**
 * Sample a closed circle as a polyline (first point repeated at the end so
 * build_ribbon closes the loop visually). `segments` < 3 → empty.
 */
inline std::vector<P2> circle_points(float cx, float cy, float radius,
                                     int segments) {
    std::vector<P2> pts;
    if (segments < 3 || radius <= 0.0f) return pts;
    pts.reserve(static_cast<std::size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i) {
        float a = 6.28318530717958647692f * static_cast<float>(i) /
                  static_cast<float>(segments);
        pts.push_back(P2{cx + radius * std::cos(a), cy + radius * std::sin(a)});
    }
    return pts;
}

} // namespace line_mesh
