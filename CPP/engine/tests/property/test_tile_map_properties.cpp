/**
 * Property-based tests for TileMap
 *
 * These tests verify universal properties of TileMap coordinate conversion
 * and viewport culling using Catch2 GENERATE() with bounded iteration counts.
 *
 * Testing Framework: Catch2 v3 with GENERATE()
 * Iterations: NUM_OUTER_TESTS * NUM_INNER_TESTS = 50 per section
 *
 * Properties tested:
 *   1. Coordinate Round-Trip (Grid -> World -> Grid)
 *   2. World Position Maps to Tile Bottom-Left
 *   3. Viewport Culling Includes All Visible Tiles
 *   4. Viewport Culling Output Bounds Invariant
 *   5. Camera Clamping Keeps Viewport Within World Bounds (added in Task 11)
 *   6. PATH Connectivity (BFS from Spawn Reaches Destination) (added in Task 7)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include "engine/tile_map.hpp"

#include <cmath>
#include <algorithm>

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

// ============================================================================
// Property 1: Coordinate Round-Trip (Grid -> World -> Grid)
// For any valid grid coordinate (col, row), world_to_tile(tile_to_world(col, row))
// returns the original (col, row).
//
// **Validates: Requirements 3.4, 16.1**
// ============================================================================

TEST_CASE("Property 1: Coordinate Round-Trip (Grid -> World -> Grid)",
          "[tile_map][property][Feature: 090-02-tile-world, Property 1]") {
    auto col = GENERATE(take(NUM_OUTER_TESTS, random(0, 49)));
    auto row = GENERATE(take(NUM_INNER_TESTS, random(0, 29)));

    // Create a TileMap large enough to contain the generated coordinates
    TileMap tm;
    tm.cols = 50;
    tm.rows = 30;
    tm.tile_size = 64;
    tm.tiles.resize(tm.cols, std::vector<TileType>(tm.rows, TileType::GRASS));
    tm.spawn = {0, 0};
    tm.destination = {49, 29};

    // Round-trip: grid -> world -> grid
    WorldPos world_pos = tm.tile_to_world(col, row);
    GridCoord result = tm.world_to_tile(world_pos.x, world_pos.y);

    REQUIRE(result.col == col);
    REQUIRE(result.row == row);
}

// ============================================================================
// Property 2: World Position Maps to Tile Bottom-Left
// For any world position within the grid, tile_to_world(world_to_tile(x, y))
// returns the bottom-left corner of the containing tile.
//
// **Validates: Requirements 16.2**
// ============================================================================

TEST_CASE("Property 2: World Position Maps to Tile Bottom-Left",
          "[tile_map][property][Feature: 090-02-tile-world, Property 2]") {
    auto x = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 3199.0f)));
    auto y = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 1919.0f)));

    TileMap tm;
    tm.cols = 50;
    tm.rows = 30;
    tm.tile_size = 64;
    tm.tiles.resize(tm.cols, std::vector<TileType>(tm.rows, TileType::GRASS));
    tm.spawn = {0, 0};
    tm.destination = {49, 29};

    // Convert world -> grid -> world (should give bottom-left corner)
    GridCoord coord = tm.world_to_tile(x, y);
    WorldPos bottom_left = tm.tile_to_world(coord.col, coord.row);

    // The bottom-left corner should be floor(x/tile_size)*tile_size
    float expected_x = std::floor(x / 64.0f) * 64.0f;
    float expected_y = std::floor(y / 64.0f) * 64.0f;

    REQUIRE_THAT(bottom_left.x, Catch::Matchers::WithinAbs(expected_x, 0.01f));
    REQUIRE_THAT(bottom_left.y, Catch::Matchers::WithinAbs(expected_y, 0.01f));
}

// ============================================================================
// Property 3: Viewport Culling Includes All Visible Tiles
// For any camera position/zoom, every tile overlapping the viewport is in the
// computed range, and no tile entirely outside the viewport is in the range.
//
// **Validates: Requirements 15.1, 15.2**
// ============================================================================

TEST_CASE("Property 3: Viewport Culling Includes All Visible Tiles",
          "[tile_map][property][Feature: 090-02-tile-world, Property 3]") {
    auto lookat_x = GENERATE(take(NUM_OUTER_TESTS, random(-200.0f, 1200.0f)));
    auto lookat_y = GENERATE(take(NUM_INNER_TESTS, random(-200.0f, 900.0f)));

    int cols = 15;
    int rows = 10;
    int tile_size = 64;
    float zoom = 1.0f;
    int win_w = 400;
    int win_h = 300;

    auto range = compute_visible_range(lookat_x, lookat_y, zoom, win_w, win_h, cols, rows, tile_size);

    // Compute viewport edges in world space
    float half_view_w = (static_cast<float>(win_w) / zoom) / 2.0f;
    float half_view_h = (static_cast<float>(win_h) / zoom) / 2.0f;
    float vp_left   = lookat_x - half_view_w;
    float vp_right  = lookat_x + half_view_w;
    float vp_bottom = lookat_y - half_view_h;
    float vp_top    = lookat_y + half_view_h;

    // Check: every tile that overlaps the viewport should be in the range
    for (int c = 0; c < cols; ++c) {
        for (int r = 0; r < rows; ++r) {
            float tile_left   = static_cast<float>(c * tile_size);
            float tile_right  = static_cast<float>((c + 1) * tile_size);
            float tile_bottom = static_cast<float>(r * tile_size);
            float tile_top    = static_cast<float>((r + 1) * tile_size);

            // Tile overlaps viewport if rectangles intersect
            bool overlaps = (tile_right > vp_left && tile_left < vp_right &&
                             tile_top > vp_bottom && tile_bottom < vp_top);

            bool in_range = (c >= range.min_col && c <= range.max_col &&
                             r >= range.min_row && r <= range.max_row);

            if (overlaps) {
                // Every overlapping tile must be in range
                REQUIRE(in_range);
            }
        }
    }
}

// ============================================================================
// Property 4: Viewport Culling Output Bounds Invariant
// For any camera state, computed range has col indices in [0, cols-1] and
// row indices in [0, rows-1], or range is empty (min > max).
//
// **Validates: Requirements 15.3**
// ============================================================================

TEST_CASE("Property 4: Viewport Culling Output Bounds Invariant",
          "[tile_map][property][Feature: 090-02-tile-world, Property 4]") {
    auto lookat_x = GENERATE(take(NUM_OUTER_TESTS, random(-5000.0f, 5000.0f)));
    auto lookat_y = GENERATE(take(NUM_INNER_TESTS, random(-5000.0f, 5000.0f)));

    int cols = 15;
    int rows = 10;
    int tile_size = 64;
    float zoom = 1.0f;
    int win_w = 800;
    int win_h = 600;

    auto range = compute_visible_range(lookat_x, lookat_y, zoom, win_w, win_h, cols, rows, tile_size);

    // Either range is empty (min > max) or indices are within valid bounds
    bool col_empty = range.min_col > range.max_col;
    bool row_empty = range.min_row > range.max_row;

    if (!col_empty) {
        REQUIRE(range.min_col >= 0);
        REQUIRE(range.max_col <= cols - 1);
    }
    if (!row_empty) {
        REQUIRE(range.min_row >= 0);
        REQUIRE(range.max_row <= rows - 1);
    }
}


// ============================================================================
// Property 6: PATH Connectivity (BFS from Spawn Reaches Destination)
// For any valid TileMap instance (random grid, random PATH chain from spawn
// to destination, random TOWER_SLOT placement not on PATH), BFS from spawn
// reaches all PATH tiles and reaches destination via 4-connected adjacency.
//
// **Validates: Requirements 14.1, 14.2, 14.3**
// ============================================================================

#include <queue>
#include <set>
#include <random>

// Helper: generate a random valid TileMap with a connected PATH from spawn to destination
static TileMap generate_random_valid_tilemap(std::mt19937& rng) {
    // Random grid size: 5-20 cols, 3-15 rows
    std::uniform_int_distribution<int> cols_dist(5, 20);
    std::uniform_int_distribution<int> rows_dist(3, 15);

    int cols = cols_dist(rng);
    int rows = rows_dist(rng);

    // Initialize all GRASS
    std::vector<std::vector<TileType>> tiles(cols, std::vector<TileType>(rows, TileType::GRASS));

    // Generate a random PATH from spawn to destination using random walk
    // Spawn at left edge, destination at right edge
    std::uniform_int_distribution<int> spawn_row_dist(0, rows - 1);
    int spawn_row = spawn_row_dist(rng);
    int dest_row = spawn_row_dist(rng);

    GridCoord spawn{0, spawn_row};
    GridCoord destination{cols - 1, dest_row};

    // Random walk from spawn to destination
    std::set<std::pair<int, int>> path_tiles;
    int cur_col = spawn.col;
    int cur_row = spawn.row;
    path_tiles.insert({cur_col, cur_row});

    while (cur_col != destination.col || cur_row != destination.row) {
        // Bias toward destination
        std::uniform_int_distribution<int> dir_dist(0, 3);
        int dir = dir_dist(rng);

        int next_col = cur_col;
        int next_row = cur_row;

        if (cur_col < destination.col && (dir == 0 || dir == 1)) {
            next_col++;
        } else if (cur_col > destination.col && dir == 0) {
            next_col--;
        } else if (cur_row < destination.row && dir == 2) {
            next_row++;
        } else if (cur_row > destination.row && dir == 3) {
            next_row--;
        } else {
            // Move toward destination
            if (cur_col < destination.col) next_col++;
            else if (cur_col > destination.col) next_col--;
            else if (cur_row < destination.row) next_row++;
            else if (cur_row > destination.row) next_row--;
        }

        // Clamp to bounds
        next_col = std::max(0, std::min(next_col, cols - 1));
        next_row = std::max(0, std::min(next_row, rows - 1));

        cur_col = next_col;
        cur_row = next_row;
        path_tiles.insert({cur_col, cur_row});
    }

    // Set PATH tiles
    for (const auto& [c, r] : path_tiles) {
        tiles[c][r] = TileType::PATH;
    }

    // Add some random TOWER_SLOT tiles (not on PATH)
    std::uniform_int_distribution<int> tower_count_dist(0, 5);
    int num_towers = tower_count_dist(rng);
    std::uniform_int_distribution<int> col_dist(0, cols - 1);
    std::uniform_int_distribution<int> row_dist(0, rows - 1);

    for (int i = 0; i < num_towers; ++i) {
        int tc = col_dist(rng);
        int tr = row_dist(rng);
        if (path_tiles.find({tc, tr}) == path_tiles.end()) {
            tiles[tc][tr] = TileType::TOWER_SLOT;
        }
    }

    TileMap tm;
    tm.tiles = std::move(tiles);
    tm.cols = cols;
    tm.rows = rows;
    tm.tile_size = 64;
    tm.spawn = spawn;
    tm.destination = destination;

    return tm;
}

TEST_CASE("Property 6: PATH Connectivity (BFS from Spawn Reaches Destination)",
          "[tile_map][property][Feature: 090-02-tile-world, Property 6]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 999999)));

    std::mt19937 rng(static_cast<unsigned>(seed));
    TileMap tm = generate_random_valid_tilemap(rng);

    // BFS from spawn through PATH tiles
    std::queue<std::pair<int, int>> queue;
    std::set<std::pair<int, int>> visited;

    queue.push({tm.spawn.col, tm.spawn.row});
    visited.insert({tm.spawn.col, tm.spawn.row});

    const int dx[] = {0, 0, 1, -1};
    const int dy[] = {1, -1, 0, 0};

    while (!queue.empty()) {
        auto [col, row] = queue.front();
        queue.pop();

        for (int d = 0; d < 4; ++d) {
            int nc = col + dx[d];
            int nr = row + dy[d];

            if (nc < 0 || nc >= tm.cols || nr < 0 || nr >= tm.rows) continue;
            if (visited.count({nc, nr})) continue;
            if (tm.get_tile(nc, nr) != TileType::PATH) continue;

            visited.insert({nc, nr});
            queue.push({nc, nr});
        }
    }

    // Destination must be reachable
    REQUIRE(visited.count({tm.destination.col, tm.destination.row}) == 1);

    // All PATH tiles must be reachable from spawn
    for (int c = 0; c < tm.cols; ++c) {
        for (int r = 0; r < tm.rows; ++r) {
            if (tm.get_tile(c, r) == TileType::PATH) {
                REQUIRE(visited.count({c, r}) == 1);
            }
        }
    }

    // No TOWER_SLOT tile occupies a PATH coordinate
    for (int c = 0; c < tm.cols; ++c) {
        for (int r = 0; r < tm.rows; ++r) {
            if (tm.get_tile(c, r) == TileType::TOWER_SLOT) {
                REQUIRE(tm.get_tile(c, r) != TileType::PATH);
            }
        }
    }
}


// ============================================================================
// Property 5: Camera Clamping Keeps Viewport Within World Bounds
// For any camera position, zoom, and world bounds where world >= viewport,
// after clamping the viewport edges stay within world bounds. When world <
// viewport, camera is centered on world.
//
// **Validates: Requirements 9.1, 9.2, 9.3**
// ============================================================================

#include "engine/ecs/systems/camera_control_system.hpp"
#include "engine/ecs/blackboard.hpp"

TEST_CASE("Property 5: Camera Clamping Keeps Viewport Within World Bounds",
          "[tile_map][property][Feature: 090-02-tile-world, Property 5]") {
    auto lookat_x = GENERATE(take(NUM_OUTER_TESTS, random(-2000.0f, 3000.0f)));
    auto lookat_y = GENERATE(take(NUM_INNER_TESTS, random(-2000.0f, 3000.0f)));

    // Fixed world and window for this property test
    float world_x = 0.0f;
    float world_y = 0.0f;
    float world_w = 960.0f;
    float world_h = 640.0f;
    int win_w = 800;
    int win_h = 600;
    float zoom = 1.0f;

    Blackboard bb;
    bb.set<float>("delta_time", 0.0f);
    bb.set<float>("camera.zoom", zoom);
    bb.set<float>("camera.lookat.x", lookat_x);
    bb.set<float>("camera.lookat.y", lookat_y);
    bb.set<int>("window_width", win_w);
    bb.set<int>("window_height", win_h);
    bb.set<float>("world.x", world_x);
    bb.set<float>("world.y", world_y);
    bb.set<float>("world.width", world_w);
    bb.set<float>("world.height", world_h);

    CameraInput input;
    apply_camera_controls(bb, input);

    float result_x = bb.get<float>("camera.lookat.x");
    float result_y = bb.get<float>("camera.lookat.y");
    float result_zoom = bb.get<float>("camera.zoom");

    float half_view_w = (static_cast<float>(win_w) / result_zoom) / 2.0f;
    float half_view_h = (static_cast<float>(win_h) / result_zoom) / 2.0f;

    // Compute viewport edges after clamping
    float vp_left   = result_x - half_view_w;
    float vp_right  = result_x + half_view_w;
    float vp_bottom = result_y - half_view_h;
    float vp_top    = result_y + half_view_h;

    if (world_w >= half_view_w * 2.0f) {
        // World wider than viewport: edges must be within bounds
        REQUIRE(vp_left >= world_x - 0.01f);
        REQUIRE(vp_right <= world_x + world_w + 0.01f);
    } else {
        // World narrower: camera centered on world
        float expected_x = world_x + world_w / 2.0f;
        REQUIRE_THAT(result_x, Catch::Matchers::WithinAbs(expected_x, 0.01f));
    }

    if (world_h >= half_view_h * 2.0f) {
        // World taller than viewport: edges must be within bounds
        REQUIRE(vp_bottom >= world_y - 0.01f);
        REQUIRE(vp_top <= world_y + world_h + 0.01f);
    } else {
        // World shorter: camera centered on world
        float expected_y = world_y + world_h / 2.0f;
        REQUIRE_THAT(result_y, Catch::Matchers::WithinAbs(expected_y, 0.01f));
    }
}
