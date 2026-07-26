/**
 * Unit tests for enemy_path:: — the pure pathfinding helpers (v2, Phase 7).
 *
 * - line_of_sight_clear: a straight segment is blocked iff it crosses an
 *   (inflated) obstacle; empty obstacle lists are always clear.
 * - build_obstacle_grid + find_path: an enemy still reaches the player when a
 *   route around a wall exists, and the returned path is a connected chain of
 *   walkable cells.
 *
 * All pure — no game loop, no SDL, no Blackboard.
 */
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "game/enemy_path.hpp"
#include "engine/pathfinding.hpp"

using enemy_path::line_of_sight_clear;
using enemy_path::segment_hits_aabb;

TEST_CASE("line_of_sight_clear: empty obstacle list is always clear",
          "[Game][enemy_path][unit]") {
    std::vector<ObstacleDef> none;
    CHECK(line_of_sight_clear(0.0f, 0.0f, 100.0f, 100.0f, none, 0.0f));
}

TEST_CASE("line_of_sight_clear: a wall between enemy and player blocks LOS",
          "[Game][enemy_path][unit]") {
    // Obstacle straddling x=50; a horizontal shot from x=0 to x=100 must cross it.
    std::vector<ObstacleDef> obs = {{40.0f, 20.0f, 20.0f, 60.0f}};
    CHECK_FALSE(line_of_sight_clear(0.0f, 50.0f, 100.0f, 50.0f, obs, 0.0f));

    // Same wall, but the segment passes clearly above it -> LOS is clear.
    CHECK(line_of_sight_clear(0.0f, 200.0f, 100.0f, 200.0f, obs, 0.0f));
}

TEST_CASE("line_of_sight_clear: inflation blocks a segment grazing the corner",
          "[Game][enemy_path][unit]") {
    std::vector<ObstacleDef> obs = {{40.0f, 40.0f, 20.0f, 20.0f}};  // box [40,60]^2
    // A segment along y=70 clears the raw box...
    CHECK(line_of_sight_clear(0.0f, 70.0f, 100.0f, 70.0f, obs, 0.0f));
    // ...but with a radius-15 body it clips the grown box (top now at 75).
    CHECK_FALSE(line_of_sight_clear(0.0f, 70.0f, 100.0f, 70.0f, obs, 15.0f));
}

TEST_CASE("segment_hits_aabb: endpoint inside the box counts as a hit",
          "[Game][enemy_path][unit]") {
    CHECK(segment_hits_aabb(50.0f, 50.0f, 200.0f, 200.0f, 40.0f, 40.0f, 40.0f, 40.0f));
}

TEST_CASE("build_obstacle_grid blocks exactly the covered cells",
          "[Game][enemy_path][unit]") {
    // 5x5 grid of 40px cells; one obstacle covering cell (2,2) with no clearance.
    std::vector<ObstacleDef> obs = {{80.0f, 80.0f, 40.0f, 40.0f}};
    auto grid = enemy_path::build_obstacle_grid(5, 5, 40, obs, 0.0f);
    REQUIRE(grid);
    CHECK(grid->get_tile(2, 2) == TileType::GRASS);   // blocked
    CHECK(grid->get_tile(0, 0) == TileType::PATH);    // clear
    CHECK(grid->get_tile(4, 4) == TileType::PATH);
}

TEST_CASE("enemy reaches player: A* finds a connected route around a wall",
          "[Game][enemy_path][unit]") {
    // A vertical wall down column 2, rows 0..3, leaving row 4 open to go around.
    // Cells are 40px; wall occupies world x in [80,120), y in [0,160).
    std::vector<ObstacleDef> obs = {{80.0f, 0.0f, 40.0f, 160.0f}};
    auto grid = enemy_path::build_obstacle_grid(6, 6, 40, obs, 0.0f);
    PathGrid pg(grid.get());

    GridCoord start{0, 0};   // left of the wall
    GridCoord goal{4, 0};    // right of the wall
    auto path = find_path(pg, start, goal);

    REQUIRE_FALSE(path.empty());                       // a route exists
    CHECK(path.front().col == start.col);
    CHECK(path.front().row == start.row);
    CHECK(path.back().col == goal.col);
    CHECK(path.back().row == goal.row);

    // Every step is walkable and 4-adjacent to the previous one.
    for (size_t i = 0; i < path.size(); ++i) {
        CHECK(pg.is_walkable(path[i].col, path[i].row));
        if (i > 0) {
            int md = std::abs(path[i].col - path[i - 1].col) +
                     std::abs(path[i].row - path[i - 1].row);
            CHECK(md == 1);
        }
    }
}

TEST_CASE("enemy reaches player: no route when the wall seals the arena",
          "[Game][enemy_path][unit]") {
    // Wall spanning the full height of a 3-wide grid -> left and right disconnected.
    std::vector<ObstacleDef> obs = {{40.0f, 0.0f, 40.0f, 240.0f}};
    auto grid = enemy_path::build_obstacle_grid(3, 6, 40, obs, 0.0f);
    PathGrid pg(grid.get());
    auto path = find_path(pg, GridCoord{0, 0}, GridCoord{2, 0});
    CHECK(path.empty());
}
