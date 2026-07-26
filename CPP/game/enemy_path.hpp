/**
 * enemy_path.hpp — pure helpers wiring the engine A* into enemy seeking (v2, Phase 7).
 *
 * Phase-6 obstacles are world-space AABBs, but the engine's find_path (090's A*)
 * walks a TileMap where only PATH tiles are walkable. So we rasterise the obstacle
 * layout into a TileMap once per arena (build_obstacle_grid) — cells that an
 * obstacle (inflated by a clearance so enemies don't clip corners) overlaps become
 * GRASS/unwalkable — and hand that straight to the existing find_path.
 *
 * line_of_sight_clear is the cheap fast-path test: when nothing blocks the straight
 * segment to the player, EnemySeekSystem skips A* and steers directly. Everything
 * here is pure + SDL/ECS-free so it unit/property-tests without a game loop.
 */
#ifndef ENEMY_PATH_HPP
#define ENEMY_PATH_HPP

#include "engine/tile_map.hpp"
#include "game/arena_config.hpp"   // ObstacleDef

#include <cmath>
#include <memory>
#include <vector>

namespace enemy_path {

// Do axis-aligned boxes A and B overlap? Edge-touch counts as clear (matches the
// obstacles.hpp "tangent counts as clear" convention).
inline bool aabb_overlap(float ax, float ay, float aw, float ah,
                         float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

// Segment (x0,y0)->(x1,y1) vs the AABB [ax,ax+aw] x [ay,ay+ah], slab clip.
// Returns true if the segment intersects the box interior.
inline bool segment_hits_aabb(float x0, float y0, float x1, float y1,
                              float ax, float ay, float aw, float ah) {
    float dx = x1 - x0, dy = y1 - y0;
    float tmin = 0.0f, tmax = 1.0f;
    const float eps = 1e-9f;

    // X slab
    if (std::fabs(dx) < eps) {
        if (x0 <= ax || x0 >= ax + aw) return false;  // parallel and outside
    } else {
        float t1 = (ax - x0) / dx, t2 = (ax + aw - x0) / dx;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin >= tmax) return false;
    }
    // Y slab
    if (std::fabs(dy) < eps) {
        if (y0 <= ay || y0 >= ay + ah) return false;
    } else {
        float t1 = (ay - y0) / dy, t2 = (ay + ah - y0) / dy;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin >= tmax) return false;
    }
    return true;
}

/**
 * Is the straight line from (x0,y0) to (x1,y1) unblocked by every obstacle,
 * each grown by `inflate` (pass the mover's radius so it starts pathing before a
 * corner clips)? Empty obstacle list is always clear.
 */
inline bool line_of_sight_clear(float x0, float y0, float x1, float y1,
                                const std::vector<ObstacleDef>& obstacles,
                                float inflate) {
    for (const auto& o : obstacles) {
        if (segment_hits_aabb(x0, y0, x1, y1,
                              o.x - inflate, o.y - inflate,
                              o.w + 2.0f * inflate, o.h + 2.0f * inflate)) {
            return false;
        }
    }
    return true;
}

/**
 * Rasterise obstacles into a walkability TileMap for find_path. cols x rows cells
 * of `tile_size`, cell (c,r) anchored at world (c*ts, r*ts) — matching TileMap's
 * world<->tile convention (world origin bottom-left). A cell is GRASS (blocked)
 * if any obstacle grown by `clearance` overlaps it, else PATH. spawn/destination
 * are unused here (find_path takes explicit start/goal).
 */
inline std::shared_ptr<TileMap> build_obstacle_grid(
    int cols, int rows, int tile_size,
    const std::vector<ObstacleDef>& obstacles, float clearance) {
    auto tm = std::make_shared<TileMap>();
    tm->cols = cols;
    tm->rows = rows;
    tm->tile_size = tile_size;
    tm->spawn = GridCoord{0, 0};
    tm->destination = GridCoord{0, 0};
    tm->tiles.assign(static_cast<size_t>(cols),
                     std::vector<TileType>(static_cast<size_t>(rows), TileType::PATH));

    const float ts = static_cast<float>(tile_size);
    for (const auto& o : obstacles) {
        float ox = o.x - clearance, oy = o.y - clearance;
        float ow = o.w + 2.0f * clearance, oh = o.h + 2.0f * clearance;
        for (int c = 0; c < cols; ++c) {
            for (int r = 0; r < rows; ++r) {
                if (aabb_overlap(c * ts, r * ts, ts, ts, ox, oy, ow, oh)) {
                    tm->tiles[static_cast<size_t>(c)][static_cast<size_t>(r)] = TileType::GRASS;
                }
            }
        }
    }
    return tm;
}

// World-space centre of tile (col,row).
inline void cell_center(int col, int row, int tile_size, float& wx, float& wy) {
    wx = (static_cast<float>(col) + 0.5f) * static_cast<float>(tile_size);
    wy = (static_cast<float>(row) + 0.5f) * static_cast<float>(tile_size);
}

}  // namespace enemy_path

#endif  // ENEMY_PATH_HPP
