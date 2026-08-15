/**
 * particle_mesh.hpp — pure quad geometry for the batched additive particle
 * renderer (v3 Tier 9, D202).
 *
 * Every additive particle becomes ONE camera-facing quad UV-mapped across a
 * soft radial glow disc, so the sprite's own falloff reaches transparent at
 * each edge. That is the fix for bugs/004: a particle drawn with no texture
 * goes through SDL's fill-rect path and is a hard SQUARE, which then seeds the
 * bloom chain and smears into a growing box halo.
 *
 * Batching is the other half. The earlier per-particle-texture attempt cost
 * ~27x because draw_entity makes six batch-flushing SDL state calls PER
 * PARTICLE; feeding one mesh to SDL_RenderGeometry makes six per FRAME.
 *
 * Engine-free and SDL-free: verts are plain structs, so the geometry is
 * unit-testable without a window. Positions come in already camera-transformed
 * (ScreenPosition space, bottom-left origin) because that is what the render
 * walk already has in hand; the render system applies ONLY the Y-flip on the
 * way to SDL_Vertex. Note this differs from line_mesh_math.hpp, which takes
 * world space and transforms itself — lines are pushed by game code, particles
 * are read off entities the CameraSystem has already transformed.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace particle_mesh {

/** Straight RGBA, matching the engine's Color component. */
struct Rgba {
    std::uint8_t r = 255, g = 255, b = 255, a = 255;
};

/** One particle: centre, edge length, colour. Size is the FULL width, not a
 *  radius — it comes straight from the Size component. */
struct Quad {
    float cx = 0.0f, cy = 0.0f;
    float size = 0.0f;
    Rgba color{};
};

/** One mesh vertex: world position, glow-disc UV, colour. */
struct MeshVert {
    float x, y;
    float u, v;
    std::uint8_t r, g, b, a;
};

/**
 * Four corner verts per particle, in the order TL, TR, BL, BR — the winding
 * `quad_indices` expects. UVs span the full [0,1] square so the disc's soft
 * edge lands exactly on the quad's edge.
 */
inline std::vector<MeshVert> build_mesh(const std::vector<Quad>& quads) {
    std::vector<MeshVert> out;
    out.reserve(quads.size() * 4);
    for (const auto& q : quads) {
        const float h = q.size * 0.5f;   // size 0 -> all four corners coincide
        const auto& c = q.color;
        // (u,v) corners paired with (x,y) offsets: -h is u/v 0, +h is u/v 1.
        const float ox[4] = {-h, +h, -h, +h};
        const float oy[4] = {-h, -h, +h, +h};
        const float us[4] = {0.0f, 1.0f, 0.0f, 1.0f};
        const float vs[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        for (int i = 0; i < 4; ++i) {
            out.push_back(MeshVert{q.cx + ox[i], q.cy + oy[i], us[i], vs[i],
                                   c.r, c.g, c.b, c.a});
        }
    }
    return out;
}

/**
 * Triangle indices for `count` quads: two triangles over each quad's own four
 * verts, offset by 4 per quad so quads never share vertices.
 */
inline std::vector<int> quad_indices(std::size_t count) {
    std::vector<int> idx;
    idx.reserve(count * 6);
    for (std::size_t q = 0; q < count; ++q) {
        const int base = static_cast<int>(q) * 4;
        idx.push_back(base + 0); idx.push_back(base + 1); idx.push_back(base + 2);
        idx.push_back(base + 1); idx.push_back(base + 3); idx.push_back(base + 2);
    }
    return idx;
}

}  // namespace particle_mesh
