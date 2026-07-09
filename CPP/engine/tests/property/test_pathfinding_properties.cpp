/**
 * Property-based tests for A* Pathfinding
 *
 * These tests verify universal properties of the A* algorithm using
 * Catch2 GENERATE() with bounded iteration counts and seed-based
 * deterministic random grid generation.
 *
 * Testing Framework: Catch2 v3 with GENERATE()
 * Iterations: NUM_OUTER_TESTS * NUM_INNER_TESTS = 50 per section
 *
 * Properties tested:
 *   1. Walkability — all waypoints are walkable
 *   2. Adjacency — consecutive waypoints have Manhattan distance 1
 *   3. Destination — first == start, last == goal
 *   4. Optimality — A* length == BFS length
 *   5. No-Path — unreachable goal returns empty vector
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include "engine/pathfinding.hpp"

#include <random>
#include <queue>
#include <set>
#include <cmath>

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

// ============================================================================
// Helper: generate a connected grid with guaranteed PATH from spawn to dest
// ============================================================================
static TileMap generate_connected_grid(int seed) {
    std::mt19937 rng(static_cast<unsigned>(seed));

    // Random grid size: 5-15 cols, 5-15 rows
    std::uniform_int_distribution<int> size_dist(5, 15);
    int cols = size_dist(rng);
    int rows = size_dist(rng);

    // Initialize all GRASS
    std::vector<std::vector<TileType>> tiles(cols, std::vector<TileType>(rows, TileType::GRASS));

    // Spawn at left edge, destination at right edge
    std::uniform_int_distribution<int> row_dist(0, rows - 1);
    int spawn_row = row_dist(rng);
    int dest_row = row_dist(rng);

    GridCoord spawn{0, spawn_row};
    GridCoord destination{cols - 1, dest_row};

    // Random walk from spawn to destination to guarantee connectivity
    int cur_col = spawn.col;
    int cur_row = spawn.row;
    tiles[cur_col][cur_row] = TileType::PATH;

    while (cur_col != destination.col || cur_row != destination.row) {
        std::uniform_int_distribution<int> dir_dist(0, 3);
        int dir = dir_dist(rng);

        int next_col = cur_col;
        int next_row = cur_row;

        if (dir == 0 && cur_col < destination.col) {
            next_col++;
        } else if (dir == 1 && cur_col > destination.col) {
            next_col--;
        } else if (dir == 2 && cur_row < destination.row) {
            next_row++;
        } else if (dir == 3 && cur_row > destination.row) {
            next_row--;
        } else {
            // Force progress toward destination
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
        tiles[cur_col][cur_row] = TileType::PATH;
    }

    // Add some random extra PATH tiles for variety
    std::uniform_int_distribution<int> extra_dist(0, cols * rows / 3);
    int extra_count = extra_dist(rng);
    std::uniform_int_distribution<int> col_dist(0, cols - 1);
    std::uniform_int_distribution<int> row_dist2(0, rows - 1);

    for (int i = 0; i < extra_count; ++i) {
        int c = col_dist(rng);
        int r = row_dist2(rng);
        tiles[c][r] = TileType::PATH;
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

// ============================================================================
// Helper: generate a disconnected grid (spawn and goal in separate regions)
// ============================================================================
static TileMap generate_disconnected_grid(int seed) {
    std::mt19937 rng(static_cast<unsigned>(seed));

    // Fixed size for simplicity: 10 cols, 8 rows
    int cols = 10;
    int rows = 8;

    // Initialize all GRASS
    std::vector<std::vector<TileType>> tiles(cols, std::vector<TileType>(rows, TileType::GRASS));

    // Spawn region: left side (cols 0-3)
    // Destination region: right side (cols 6-9)
    // Wall of GRASS at cols 4-5 (no PATH tiles there)

    // Add PATH tiles in left region
    std::uniform_int_distribution<int> left_col_dist(0, 3);
    std::uniform_int_distribution<int> row_dist(0, rows - 1);

    // Ensure spawn is PATH
    int spawn_row = row_dist(rng);
    tiles[0][spawn_row] = TileType::PATH;

    // Add some PATH tiles in left region
    for (int i = 0; i < 8; ++i) {
        int c = left_col_dist(rng);
        int r = row_dist(rng);
        tiles[c][r] = TileType::PATH;
    }

    // Ensure destination is PATH
    std::uniform_int_distribution<int> right_col_dist(6, 9);
    int dest_col = right_col_dist(rng);
    int dest_row = row_dist(rng);
    tiles[dest_col][dest_row] = TileType::PATH;

    // Add some PATH tiles in right region
    for (int i = 0; i < 8; ++i) {
        int c = right_col_dist(rng);
        int r = row_dist(rng);
        tiles[c][r] = TileType::PATH;
    }

    // Cols 4 and 5 remain GRASS — wall separating regions

    TileMap tm;
    tm.tiles = std::move(tiles);
    tm.cols = cols;
    tm.rows = rows;
    tm.tile_size = 64;
    tm.spawn = {0, spawn_row};
    tm.destination = {dest_col, dest_row};

    return tm;
}

// ============================================================================
// Helper: BFS oracle returning shortest path length (0 if unreachable)
// ============================================================================
static int bfs_path_length(const PathGrid& grid, GridCoord start, GridCoord goal) {
    if (start.col == goal.col && start.row == goal.row) return 1;
    if (!grid.is_walkable(start.col, start.row) || !grid.is_walkable(goal.col, goal.row)) return 0;

    std::queue<int> queue;
    std::unordered_map<int, int> dist;

    auto encode = [&grid](int col, int row) -> int {
        return row * grid.width() + col;
    };

    int start_key = encode(start.col, start.row);
    int goal_key = encode(goal.col, goal.row);

    queue.push(start_key);
    dist[start_key] = 0;

    const int dx[] = {0, 0, -1, 1};
    const int dy[] = {1, -1, 0, 0};

    while (!queue.empty()) {
        int current = queue.front();
        queue.pop();

        int cur_row = current / grid.width();
        int cur_col = current % grid.width();

        for (int d = 0; d < 4; ++d) {
            int nc = cur_col + dx[d];
            int nr = cur_row + dy[d];

            if (!grid.is_walkable(nc, nr)) continue;

            int nkey = encode(nc, nr);
            if (dist.find(nkey) != dist.end()) continue;

            dist[nkey] = dist[current] + 1;

            if (nkey == goal_key) {
                return dist[nkey] + 1;  // +1 because path length includes start
            }

            queue.push(nkey);
        }
    }

    return 0;  // unreachable
}

// ============================================================================
// Property 1: Walkability
// For any valid PathGrid with a path from spawn to destination, every
// GridCoord in the path returned by find_path satisfies is_walkable == true.
//
// **Validates: Requirements 9.1, 9.2**
// ============================================================================

TEST_CASE("Property 1: Walkability",
          "[pathfinding][property][Feature: 090-03-pathfinding, Property 1: Walkability]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 999999)));
    auto inner_seed = GENERATE(take(NUM_INNER_TESTS, random(0, 999999)));

    // Combine seeds for unique grid per iteration
    int combined_seed = seed * 1000 + inner_seed;
    TileMap tm = generate_connected_grid(combined_seed);
    PathGrid grid(&tm);

    auto path = find_path(grid, tm.spawn, tm.destination);

    // Path should not be empty for connected grids
    REQUIRE_FALSE(path.empty());

    // Every waypoint must be walkable
    for (const auto& coord : path) {
        REQUIRE(grid.is_walkable(coord.col, coord.row));
    }
}

// ============================================================================
// Property 2: Adjacency
// For any valid PathGrid with a path from spawn to destination, every
// consecutive pair of GridCoords has Manhattan distance exactly 1.
//
// **Validates: Requirements 4.1, 4.2, 10.1**
// ============================================================================

TEST_CASE("Property 2: Adjacency",
          "[pathfinding][property][Feature: 090-03-pathfinding, Property 2: Adjacency]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 999999)));
    auto inner_seed = GENERATE(take(NUM_INNER_TESTS, random(0, 999999)));

    int combined_seed = seed * 1000 + inner_seed;
    TileMap tm = generate_connected_grid(combined_seed);
    PathGrid grid(&tm);

    auto path = find_path(grid, tm.spawn, tm.destination);

    REQUIRE_FALSE(path.empty());

    // Consecutive waypoints must have Manhattan distance exactly 1
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        int manhattan = std::abs(path[i].col - path[i + 1].col) +
                        std::abs(path[i].row - path[i + 1].row);
        REQUIRE(manhattan == 1);
    }
}

// ============================================================================
// Property 3: Destination
// For any valid PathGrid with a path from spawn to destination, path[0] == start
// and path.back() == goal.
//
// **Validates: Requirements 3.1, 11.1, 11.2**
// ============================================================================

TEST_CASE("Property 3: Destination",
          "[pathfinding][property][Feature: 090-03-pathfinding, Property 3: Destination]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 999999)));
    auto inner_seed = GENERATE(take(NUM_INNER_TESTS, random(0, 999999)));

    int combined_seed = seed * 1000 + inner_seed;
    TileMap tm = generate_connected_grid(combined_seed);
    PathGrid grid(&tm);

    auto path = find_path(grid, tm.spawn, tm.destination);

    REQUIRE_FALSE(path.empty());

    // First element must be start
    REQUIRE(path.front().col == tm.spawn.col);
    REQUIRE(path.front().row == tm.spawn.row);

    // Last element must be goal
    REQUIRE(path.back().col == tm.destination.col);
    REQUIRE(path.back().row == tm.destination.row);
}

// ============================================================================
// Property 4: Optimality
// For any valid PathGrid with a path from spawn to destination, the path
// length returned by find_path equals the path length found by BFS.
//
// **Validates: Requirements 2.2, 3.2, 12.1**
// ============================================================================

TEST_CASE("Property 4: Optimality",
          "[pathfinding][property][Feature: 090-03-pathfinding, Property 4: Optimality]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 999999)));
    auto inner_seed = GENERATE(take(NUM_INNER_TESTS, random(0, 999999)));

    int combined_seed = seed * 1000 + inner_seed;
    TileMap tm = generate_connected_grid(combined_seed);
    PathGrid grid(&tm);

    auto path = find_path(grid, tm.spawn, tm.destination);
    int bfs_len = bfs_path_length(grid, tm.spawn, tm.destination);

    REQUIRE_FALSE(path.empty());
    REQUIRE(bfs_len > 0);

    // A* path length must equal BFS path length (both optimal)
    REQUIRE(static_cast<int>(path.size()) == bfs_len);
}

// ============================================================================
// Property 5: No-Path
// For any PathGrid where the goal is unreachable from the start, find_path
// returns an empty vector.
//
// **Validates: Requirements 3.3, 13.1**
// ============================================================================

TEST_CASE("Property 5: No-Path",
          "[pathfinding][property][Feature: 090-03-pathfinding, Property 5: No-Path]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 999999)));
    auto inner_seed = GENERATE(take(NUM_INNER_TESTS, random(0, 999999)));

    int combined_seed = seed * 1000 + inner_seed;
    TileMap tm = generate_disconnected_grid(combined_seed);
    PathGrid grid(&tm);

    auto path = find_path(grid, tm.spawn, tm.destination);

    // Path must be empty for disconnected grids
    REQUIRE(path.empty());
}
