#ifndef TILE_MAP_HPP
#define TILE_MAP_HPP

#include <vector>
#include <cmath>
#include <algorithm>

/**
 * TileType enumeration
 *
 * Defines the three tile types used in the tower defense grid world.
 * Integer codes match the JSON representation for direct casting during parsing.
 */
enum class TileType : int {
    GRASS      = 0,
    PATH       = 1,
    TOWER_SLOT = 2
};

/**
 * GridCoord - A (col, row) pair identifying a tile in the grid.
 * col = horizontal index (0 = leftmost), row = vertical index (0 = bottom).
 */
struct GridCoord {
    int col;
    int row;
};

/**
 * WorldPos - A (x, y) pair in world-space pixels.
 * Tile (col, row) has its bottom-left corner at (col * tile_size, row * tile_size).
 */
struct WorldPos {
    float x;
    float y;
};

/**
 * VisibleRange - Range of tile indices visible in the camera viewport.
 * If min > max for either axis, the range is empty (viewport entirely outside grid).
 */
struct VisibleRange {
    int min_col;
    int max_col;
    int min_row;
    int max_row;
};

/**
 * TileMap struct
 *
 * Stores the 2D tile grid, dimensions, tile size, and spawn/destination coordinates.
 * Storage convention: tiles[col][row] — column-major for natural (x, y) indexing.
 */
struct TileMap {
    std::vector<std::vector<TileType>> tiles;  // tiles[col][row]
    int rows;
    int cols;
    int tile_size;
    GridCoord spawn;
    GridCoord destination;

    /**
     * Get tile type at (col, row). Returns GRASS for out-of-bounds.
     */
    TileType get_tile(int col, int row) const {
        if (col < 0 || col >= cols || row < 0 || row >= rows) {
            return TileType::GRASS;
        }
        return tiles[col][row];
    }

    /**
     * Convert grid coordinate to world position (bottom-left corner of tile).
     * World position = (col * tile_size, row * tile_size).
     */
    WorldPos tile_to_world(int col, int row) const {
        return WorldPos{
            static_cast<float>(col * tile_size),
            static_cast<float>(row * tile_size)
        };
    }

    /**
     * Convert world position to grid coordinate using floor division.
     * col = floor(x / tile_size), row = floor(y / tile_size).
     */
    GridCoord world_to_tile(float x, float y) const {
        return GridCoord{
            static_cast<int>(std::floor(x / static_cast<float>(tile_size))),
            static_cast<int>(std::floor(y / static_cast<float>(tile_size)))
        };
    }
};

/**
 * Compute which tiles are visible given camera state and grid dimensions.
 * Pure function — no SDL dependency, fully unit-testable.
 *
 * @param lookat_x  Camera center X in world space
 * @param lookat_y  Camera center Y in world space
 * @param zoom      Camera zoom level (>0)
 * @param win_w     Window width in pixels
 * @param win_h     Window height in pixels
 * @param cols      Number of grid columns
 * @param rows      Number of grid rows
 * @param tile_size Pixel size of each square tile
 * @return VisibleRange with clamped tile indices, or empty range if viewport outside grid
 */
inline VisibleRange compute_visible_range(
    float lookat_x, float lookat_y, float zoom,
    int win_w, int win_h,
    int cols, int rows, int tile_size)
{
    // Prevent division by zero
    if (zoom <= 0.0f) zoom = 0.01f;
    if (tile_size <= 0) return VisibleRange{1, 0, 1, 0};  // empty range

    float ts = static_cast<float>(tile_size);

    // Viewport edges in world space
    float half_view_w = (static_cast<float>(win_w) / zoom) / 2.0f;
    float half_view_h = (static_cast<float>(win_h) / zoom) / 2.0f;

    float viewport_left   = lookat_x - half_view_w;
    float viewport_right  = lookat_x + half_view_w;
    float viewport_bottom = lookat_y - half_view_h;
    float viewport_top    = lookat_y + half_view_h;

    // Convert to tile indices using floor
    int min_col = static_cast<int>(std::floor(viewport_left / ts));
    int max_col = static_cast<int>(std::floor((viewport_right - 0.001f) / ts));
    int min_row = static_cast<int>(std::floor(viewport_bottom / ts));
    int max_row = static_cast<int>(std::floor((viewport_top - 0.001f) / ts));

    // Clamp to grid bounds
    min_col = std::max(min_col, 0);
    max_col = std::min(max_col, cols - 1);
    min_row = std::max(min_row, 0);
    max_row = std::min(max_row, rows - 1);

    return VisibleRange{min_col, max_col, min_row, max_row};
}

#endif // TILE_MAP_HPP
