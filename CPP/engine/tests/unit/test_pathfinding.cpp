/**
 * Unit tests for PathGrid and A* pathfinding
 *
 * Tests verify PathGrid walkability, neighbor queries, and A* algorithm
 * correctness on known map configurations.
 *
 * Testing Framework: Catch2 v3
 * Tags: [pathfinding][unit]
 *
 * Validates: Requirements 1.1-1.6, 2.1-2.7, 3.1-3.5, 4.1-4.2, 5.1-5.8,
 *            14.1-14.4, 15.1-15.4, 16.1-16.4
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/pathfinding.hpp"
#include <algorithm>
#include <memory>

// Helper: create a simple 5x4 TileMap for PathGrid testing
// Layout (col-major, tiles[col][row]):
//   Row 3: GRASS  GRASS  GRASS  TOWER_SLOT GRASS
//   Row 2: PATH   PATH   PATH   PATH       PATH    <- walkable row
//   Row 1: GRASS  TOWER_SLOT GRASS GRASS   GRASS
//   Row 0: GRASS  GRASS  GRASS  GRASS      GRASS
static TileMap make_pathgrid_test_map() {
    TileMap tm;
    tm.cols = 5;
    tm.rows = 4;
    tm.tile_size = 64;
    tm.spawn = {0, 2};
    tm.destination = {4, 2};

    tm.tiles.resize(tm.cols);
    for (int c = 0; c < tm.cols; ++c) {
        tm.tiles[c].resize(tm.rows, TileType::GRASS);
    }
    // PATH across row 2
    for (int c = 0; c < tm.cols; ++c) {
        tm.tiles[c][2] = TileType::PATH;
    }
    // TOWER_SLOT at (1,1) and (3,3)
    tm.tiles[1][1] = TileType::TOWER_SLOT;
    tm.tiles[3][3] = TileType::TOWER_SLOT;

    return tm;
}

// Helper: create a 3x3 all-PATH grid for neighbor testing
static TileMap make_all_path_3x3() {
    TileMap tm;
    tm.cols = 3;
    tm.rows = 3;
    tm.tile_size = 64;
    tm.spawn = {0, 0};
    tm.destination = {2, 2};

    tm.tiles.resize(tm.cols);
    for (int c = 0; c < tm.cols; ++c) {
        tm.tiles[c].resize(tm.rows, TileType::PATH);
    }
    return tm;
}

// ---------------------------------------------------------------------------
// PathGrid::is_walkable — PATH tile
// ---------------------------------------------------------------------------
TEST_CASE("PathGrid is_walkable returns true for PATH tile", "[pathfinding][unit]") {
    auto tm = make_pathgrid_test_map();
    PathGrid grid(&tm);

    REQUIRE(grid.is_walkable(0, 2) == true);
    REQUIRE(grid.is_walkable(2, 2) == true);
    REQUIRE(grid.is_walkable(4, 2) == true);
}

// ---------------------------------------------------------------------------
// PathGrid::is_walkable — GRASS tile
// ---------------------------------------------------------------------------
TEST_CASE("PathGrid is_walkable returns false for GRASS tile", "[pathfinding][unit]") {
    auto tm = make_pathgrid_test_map();
    PathGrid grid(&tm);

    REQUIRE(grid.is_walkable(0, 0) == false);
    REQUIRE(grid.is_walkable(2, 3) == false);
    REQUIRE(grid.is_walkable(4, 0) == false);
}

// ---------------------------------------------------------------------------
// PathGrid::is_walkable — TOWER_SLOT tile
// ---------------------------------------------------------------------------
TEST_CASE("PathGrid is_walkable returns false for TOWER_SLOT tile", "[pathfinding][unit]") {
    auto tm = make_pathgrid_test_map();
    PathGrid grid(&tm);

    REQUIRE(grid.is_walkable(1, 1) == false);
    REQUIRE(grid.is_walkable(3, 3) == false);
}

// ---------------------------------------------------------------------------
// PathGrid::is_walkable — out-of-bounds
// ---------------------------------------------------------------------------
TEST_CASE("PathGrid is_walkable returns false for out-of-bounds", "[pathfinding][unit]") {
    auto tm = make_pathgrid_test_map();
    PathGrid grid(&tm);

    REQUIRE(grid.is_walkable(-1, 0) == false);
    REQUIRE(grid.is_walkable(0, -1) == false);
    REQUIRE(grid.is_walkable(5, 2) == false);
    REQUIRE(grid.is_walkable(2, 4) == false);
    REQUIRE(grid.is_walkable(-10, -10) == false);
    REQUIRE(grid.is_walkable(100, 100) == false);
}

// ---------------------------------------------------------------------------
// PathGrid::neighbors — PATH tile surrounded by four PATH tiles
// ---------------------------------------------------------------------------
TEST_CASE("PathGrid neighbors for center PATH tile returns 4 neighbors", "[pathfinding][unit]") {
    auto tm = make_all_path_3x3();
    PathGrid grid(&tm);

    // Center tile (1,1) surrounded by PATH on all 4 sides
    auto nbrs = grid.neighbors(1, 1);
    REQUIRE(nbrs.size() == 4);
}

// ---------------------------------------------------------------------------
// PathGrid::neighbors — PATH tile at grid edge
// ---------------------------------------------------------------------------
TEST_CASE("PathGrid neighbors for edge PATH tile returns fewer neighbors", "[pathfinding][unit]") {
    auto tm = make_all_path_3x3();
    PathGrid grid(&tm);

    // Corner tile (0,0) — only 2 in-bounds walkable neighbors
    auto nbrs = grid.neighbors(0, 0);
    REQUIRE(nbrs.size() == 2);

    // Edge tile (1,0) — 3 in-bounds walkable neighbors
    auto nbrs2 = grid.neighbors(1, 0);
    REQUIRE(nbrs2.size() == 3);
}

// ---------------------------------------------------------------------------
// PathGrid::neighbors — PATH tile adjacent to non-walkable
// ---------------------------------------------------------------------------
TEST_CASE("PathGrid neighbors excludes non-walkable tiles", "[pathfinding][unit]") {
    auto tm = make_pathgrid_test_map();
    PathGrid grid(&tm);

    // Tile (2, 2) is PATH. Neighbors: (1,2)=PATH, (3,2)=PATH, (2,1)=GRASS, (2,3)=GRASS
    auto nbrs = grid.neighbors(2, 2);
    REQUIRE(nbrs.size() == 2);

    // Verify the neighbors are the left and right PATH tiles
    bool has_left = false, has_right = false;
    for (const auto& n : nbrs) {
        if (n.col == 1 && n.row == 2) has_left = true;
        if (n.col == 3 && n.row == 2) has_right = true;
    }
    REQUIRE(has_left);
    REQUIRE(has_right);
}

// ---------------------------------------------------------------------------
// PathGrid::neighbors — non-walkable tile returns empty
// ---------------------------------------------------------------------------
TEST_CASE("PathGrid neighbors for non-walkable tile returns empty", "[pathfinding][unit]") {
    auto tm = make_pathgrid_test_map();
    PathGrid grid(&tm);

    // GRASS tile
    auto nbrs = grid.neighbors(0, 0);
    REQUIRE(nbrs.empty());

    // TOWER_SLOT tile
    auto nbrs2 = grid.neighbors(1, 1);
    REQUIRE(nbrs2.empty());
}

// ---------------------------------------------------------------------------
// PathGrid::width() and height()
// ---------------------------------------------------------------------------
TEST_CASE("PathGrid width and height match TileMap dimensions", "[pathfinding][unit]") {
    auto tm = make_pathgrid_test_map();
    PathGrid grid(&tm);

    REQUIRE(grid.width() == 5);
    REQUIRE(grid.height() == 4);
}

// ---------------------------------------------------------------------------
// A* — straight horizontal path
// ---------------------------------------------------------------------------
TEST_CASE("A* finds straight horizontal path", "[pathfinding][unit]") {
    // 15x10 grid with PATH across row 5
    TileMap tm;
    tm.cols = 15;
    tm.rows = 10;
    tm.tile_size = 64;
    tm.spawn = {0, 5};
    tm.destination = {14, 5};

    tm.tiles.resize(tm.cols);
    for (int c = 0; c < tm.cols; ++c) {
        tm.tiles[c].resize(tm.rows, TileType::GRASS);
        tm.tiles[c][5] = TileType::PATH;
    }

    PathGrid grid(&tm);
    auto path = find_path(grid, {0, 5}, {14, 5});

    REQUIRE(path.size() == 15);
    REQUIRE(path.front().col == 0);
    REQUIRE(path.front().row == 5);
    REQUIRE(path.back().col == 14);
    REQUIRE(path.back().row == 5);
}

// ---------------------------------------------------------------------------
// A* — L-shaped path
// ---------------------------------------------------------------------------
TEST_CASE("A* finds shortest L-shaped path", "[pathfinding][unit]") {
    // 5x5 grid with L-shaped PATH: row 0 cols 0-4, then col 4 rows 0-4
    TileMap tm;
    tm.cols = 5;
    tm.rows = 5;
    tm.tile_size = 64;
    tm.spawn = {0, 0};
    tm.destination = {4, 4};

    tm.tiles.resize(tm.cols);
    for (int c = 0; c < tm.cols; ++c) {
        tm.tiles[c].resize(tm.rows, TileType::GRASS);
    }
    // Horizontal part: row 0, cols 0-4
    for (int c = 0; c < 5; ++c) {
        tm.tiles[c][0] = TileType::PATH;
    }
    // Vertical part: col 4, rows 0-4
    for (int r = 0; r < 5; ++r) {
        tm.tiles[4][r] = TileType::PATH;
    }

    PathGrid grid(&tm);
    auto path = find_path(grid, {0, 0}, {4, 4});

    // Shortest path: (0,0)->(1,0)->(2,0)->(3,0)->(4,0)->(4,1)->(4,2)->(4,3)->(4,4) = 9 steps
    REQUIRE(path.size() == 9);
    REQUIRE(path.front().col == 0);
    REQUIRE(path.front().row == 0);
    REQUIRE(path.back().col == 4);
    REQUIRE(path.back().row == 4);
}

// ---------------------------------------------------------------------------
// A* — start equals goal
// ---------------------------------------------------------------------------
TEST_CASE("A* returns single element when start equals goal", "[pathfinding][unit]") {
    auto tm = make_pathgrid_test_map();
    PathGrid grid(&tm);

    auto path = find_path(grid, {2, 2}, {2, 2});
    REQUIRE(path.size() == 1);
    REQUIRE(path[0].col == 2);
    REQUIRE(path[0].row == 2);
}

// ---------------------------------------------------------------------------
// A* — unreachable goal
// ---------------------------------------------------------------------------
TEST_CASE("A* returns empty vector when goal is unreachable", "[pathfinding][unit]") {
    // 5x5 grid: PATH at (0,0) and (4,4) but no connection
    TileMap tm;
    tm.cols = 5;
    tm.rows = 5;
    tm.tile_size = 64;
    tm.spawn = {0, 0};
    tm.destination = {4, 4};

    tm.tiles.resize(tm.cols);
    for (int c = 0; c < tm.cols; ++c) {
        tm.tiles[c].resize(tm.rows, TileType::GRASS);
    }
    tm.tiles[0][0] = TileType::PATH;
    tm.tiles[4][4] = TileType::PATH;

    PathGrid grid(&tm);
    auto path = find_path(grid, {0, 0}, {4, 4});
    REQUIRE(path.empty());
}

// ---------------------------------------------------------------------------
// PathfindingSystem::compute — stores pathgrid and computed_path
// ---------------------------------------------------------------------------
TEST_CASE("PathfindingSystem compute stores pathgrid and computed_path", "[pathfinding][unit]") {
    // Create a simple tilemap with a path
    auto tilemap = std::make_shared<TileMap>();
    tilemap->cols = 5;
    tilemap->rows = 4;
    tilemap->tile_size = 64;
    tilemap->spawn = {0, 2};
    tilemap->destination = {4, 2};

    tilemap->tiles.resize(tilemap->cols);
    for (int c = 0; c < tilemap->cols; ++c) {
        tilemap->tiles[c].resize(tilemap->rows, TileType::GRASS);
        tilemap->tiles[c][2] = TileType::PATH;
    }

    Blackboard bb;
    bb.set<std::shared_ptr<TileMap>>("tilemap", tilemap);

    PathfindingSystem::compute(bb);

    REQUIRE(bb.has("pathgrid"));
    REQUIRE(bb.has("computed_path"));

    auto pathgrid = bb.get<std::shared_ptr<PathGrid>>("pathgrid");
    REQUIRE(pathgrid != nullptr);
    REQUIRE(pathgrid->width() == 5);
    REQUIRE(pathgrid->height() == 4);

    auto computed_path = bb.get<std::shared_ptr<std::vector<GridCoord>>>("computed_path");
    REQUIRE(computed_path != nullptr);
    REQUIRE(computed_path->size() == 5);
    REQUIRE((*computed_path)[0].col == 0);
    REQUIRE((*computed_path)[0].row == 2);
    REQUIRE((*computed_path)[4].col == 4);
    REQUIRE((*computed_path)[4].row == 2);
}

// ---------------------------------------------------------------------------
// PathfindingSystem::compute — no tilemap on Blackboard
// ---------------------------------------------------------------------------
TEST_CASE("PathfindingSystem compute skips without error when no tilemap", "[pathfinding][unit]") {
    Blackboard bb;

    // Should not throw or crash
    PathfindingSystem::compute(bb);

    REQUIRE_FALSE(bb.has("pathgrid"));
    REQUIRE_FALSE(bb.has("computed_path"));
}
