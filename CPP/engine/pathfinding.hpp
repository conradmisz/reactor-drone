#ifndef PATHFINDING_HPP
#define PATHFINDING_HPP

#include "engine/tile_map.hpp"
#include "engine/ecs/blackboard.hpp"
#include <vector>
#include <queue>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <functional>
#include <memory>

/**
 * PathGrid — lightweight wrapper around TileMap for pathfinding queries.
 * Only PATH tiles are walkable; GRASS and TOWER_SLOT are not.
 */
struct PathGrid {
    const TileMap* tilemap;

    explicit PathGrid(const TileMap* tm) : tilemap(tm) {}

    int width() const { return tilemap->cols; }
    int height() const { return tilemap->rows; }

    bool is_walkable(int col, int row) const {
        if (col < 0 || col >= tilemap->cols || row < 0 || row >= tilemap->rows) {
            return false;
        }
        return tilemap->get_tile(col, row) == TileType::PATH;
    }

    std::vector<GridCoord> neighbors(int col, int row) const {
        std::vector<GridCoord> result;
        if (!is_walkable(col, row)) {
            return result;
        }
        // 4-connected: up, down, left, right
        const int dx[] = {0, 0, -1, 1};
        const int dy[] = {1, -1, 0, 0};
        for (int i = 0; i < 4; ++i) {
            int nc = col + dx[i];
            int nr = row + dy[i];
            if (is_walkable(nc, nr)) {
                result.push_back(GridCoord{nc, nr});
            }
        }
        return result;
    }
};

/**
 * A* pathfinding — pure function, no ECS/SDL dependency.
 *
 * Returns shortest path from start to goal as vector of GridCoords.
 * Uses Manhattan distance heuristic and 4-connected movement.
 * Returns empty vector if no path exists.
 * Returns single-element vector if start == goal.
 */
inline std::vector<GridCoord> find_path(
    const PathGrid& grid,
    GridCoord start,
    GridCoord goal)
{
    // start == goal: return single element
    if (start.col == goal.col && start.row == goal.row) {
        return {start};
    }

    // Verify start and goal are walkable
    if (!grid.is_walkable(start.col, start.row) ||
        !grid.is_walkable(goal.col, goal.row)) {
        return {};
    }

    // Manhattan distance heuristic
    auto heuristic = [&goal](int col, int row) -> int {
        return std::abs(goal.col - col) + std::abs(goal.row - row);
    };

    // Encode (col, row) as single int for map keys
    auto encode = [&grid](int col, int row) -> int {
        return row * grid.width() + col;
    };

    // Priority queue: (f_cost, encoded_coord)
    using PQEntry = std::pair<int, int>;
    std::priority_queue<PQEntry, std::vector<PQEntry>, std::greater<PQEntry>> open_set;

    std::unordered_map<int, int> came_from;  // encoded -> encoded
    std::unordered_map<int, int> g_cost;     // encoded -> cost from start

    int start_key = encode(start.col, start.row);
    int goal_key = encode(goal.col, goal.row);

    g_cost[start_key] = 0;
    open_set.push({heuristic(start.col, start.row), start_key});

    while (!open_set.empty()) {
        auto [f, current_key] = open_set.top();
        open_set.pop();

        if (current_key == goal_key) {
            // Reconstruct path
            std::vector<GridCoord> path;
            int key = goal_key;
            while (key != start_key) {
                int r = key / grid.width();
                int c = key % grid.width();
                path.push_back(GridCoord{c, r});
                key = came_from[key];
            }
            path.push_back(start);
            std::reverse(path.begin(), path.end());
            return path;
        }

        int current_g = g_cost[current_key];
        int cur_row = current_key / grid.width();
        int cur_col = current_key % grid.width();

        for (const auto& neighbor : grid.neighbors(cur_col, cur_row)) {
            int neighbor_key = encode(neighbor.col, neighbor.row);
            int tentative_g = current_g + 1;

            auto it = g_cost.find(neighbor_key);
            if (it == g_cost.end() || tentative_g < it->second) {
                g_cost[neighbor_key] = tentative_g;
                came_from[neighbor_key] = current_key;
                int f_cost = tentative_g + heuristic(neighbor.col, neighbor.row);
                open_set.push({f_cost, neighbor_key});
            }
        }
    }

    // No path found
    return {};
}

/**
 * PathfindingSystem — orchestrates A* at level load.
 * Reads TileMap from Blackboard, constructs PathGrid, calls find_path,
 * stores PathGrid and computed_path on Blackboard.
 */
struct PathfindingSystem {
    static void compute(Blackboard& bb) {
        if (!bb.has("tilemap")) return;

        std::shared_ptr<TileMap> tilemap;
        try {
            tilemap = bb.get<std::shared_ptr<TileMap>>("tilemap");
        } catch (...) {
            return;
        }
        if (!tilemap) return;

        // Construct PathGrid and store on Blackboard
        auto pathgrid = std::make_shared<PathGrid>(tilemap.get());
        bb.set<std::shared_ptr<PathGrid>>("pathgrid", pathgrid);

        // Run A* from spawn to destination
        auto path = find_path(*pathgrid, tilemap->spawn, tilemap->destination);

        // Store computed path on Blackboard
        auto path_ptr = std::make_shared<std::vector<GridCoord>>(std::move(path));
        bb.set<std::shared_ptr<std::vector<GridCoord>>>("computed_path", path_ptr);
    }
};

#endif // PATHFINDING_HPP
