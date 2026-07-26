/**
 * Property-based tests for v2 Phase 7 pathfinding (enemy_path:: + engine A*).
 *
 * P1 (path avoids obstacle cells): over random obstacle grids, any path find_path
 *    returns steps only through walkable (PATH) cells and never through a cell an
 *    obstacle blocked, and consecutive steps are 4-adjacent.
 *
 * P2 (LOS agrees with A*): when line_of_sight_clear is true between two walkable
 *    cells, the straight route is itself a valid path, so find_path returns a
 *    non-empty path (a route provably exists).
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include <cmath>
#include <vector>

#include "game/enemy_path.hpp"
#include "engine/pathfinding.hpp"

constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

TEST_CASE("find_path steps only through walkable cells on random obstacle grids",
          "[Game][enemy_path7][property]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(1, 100000)));

    const int cols = 12, rows = 12, ts = 40;

    // Deterministic LCG so the grid is reproducible from `seed` (no <random> churn).
    unsigned int state = static_cast<unsigned int>(seed);
    auto next = [&state]() {
        state = state * 1103515245u + 12345u;
        return (state >> 16) & 0x7fff;
    };

    // Sprinkle a handful of small obstacle boxes (clearance 0 so cell coverage is
    // exact and the assertion is crisp).
    std::vector<ObstacleDef> obs;
    for (int i = 0; i < 8; ++i) {
        float ox = static_cast<float>(next() % (cols - 1)) * ts;
        float oy = static_cast<float>(next() % (rows - 1)) * ts;
        obs.push_back({ox, oy, static_cast<float>(ts), static_cast<float>(ts)});
    }
    auto grid = enemy_path::build_obstacle_grid(cols, rows, ts, obs, 0.0f);
    PathGrid pg(grid.get());

    for (int t = 0; t < NUM_INNER_TESTS; ++t) {
        GridCoord start{static_cast<int>(next()) % cols, static_cast<int>(next()) % rows};
        GridCoord goal{static_cast<int>(next()) % cols, static_cast<int>(next()) % rows};

        auto path = find_path(pg, start, goal);
        if (path.empty()) continue;  // no route on this grid — nothing to assert

        // Endpoints match, and every step is walkable + 4-adjacent.
        CHECK(path.front().col == start.col);
        CHECK(path.front().row == start.row);
        CHECK(path.back().col == goal.col);
        CHECK(path.back().row == goal.row);
        for (size_t i = 0; i < path.size(); ++i) {
            CHECK(pg.is_walkable(path[i].col, path[i].row));
            CHECK(grid->get_tile(path[i].col, path[i].row) == TileType::PATH);
            if (i > 0) {
                int md = std::abs(path[i].col - path[i - 1].col) +
                         std::abs(path[i].row - path[i - 1].row);
                CHECK(md == 1);
            }
        }
    }
}

TEST_CASE("clear line-of-sight implies a path exists",
          "[Game][enemy_path7][property]") {
    auto ox = GENERATE(take(NUM_OUTER_TESTS, random(120.0f, 240.0f)));

    const int cols = 12, rows = 12, ts = 40;
    // A single interior obstacle; probe pairs of points and cross-check LOS vs A*.
    std::vector<ObstacleDef> obs = {{ox, 160.0f, 80.0f, 80.0f}};
    auto grid = enemy_path::build_obstacle_grid(cols, rows, ts, obs, 0.0f);
    PathGrid pg(grid.get());

    for (int i = 0; i <= NUM_INNER_TESTS; ++i) {
        for (int j = 0; j <= NUM_INNER_TESTS; ++j) {
            // Two walkable-ish sample points along the arena edges.
            float ax = 20.0f + 40.0f * i, ay = 20.0f;
            float bx = 20.0f + 40.0f * j, by = 440.0f;
            GridCoord ca = grid->world_to_tile(ax, ay);
            GridCoord cb = grid->world_to_tile(bx, by);
            if (!pg.is_walkable(ca.col, ca.row) || !pg.is_walkable(cb.col, cb.row)) continue;

            if (enemy_path::line_of_sight_clear(ax, ay, bx, by, obs, 0.0f)) {
                // A clear straight shot means a route trivially exists.
                CHECK_FALSE(find_path(pg, ca, cb).empty());
            }
        }
    }
}
