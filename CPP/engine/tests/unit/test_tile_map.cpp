/**
 * Unit tests for TileMap
 *
 * Tests verify TileType enum values, get_tile boundary handling,
 * tile_to_world and world_to_tile coordinate conversions, and
 * compute_visible_range viewport culling logic.
 *
 * Testing Framework: Catch2 v3
 * Tags: [tile_map][unit]
 *
 * Validates: Requirements 1.1, 1.2, 3.1, 3.2, 3.4, 15.1, 15.2, 15.3, 16.1, 16.2
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "engine/tile_map.hpp"

using Catch::Matchers::WithinAbs;

// Helper: create a simple 5x4 TileMap for testing
static TileMap make_test_map() {
    // 5 cols, 4 rows, tile_size=64
    TileMap tm;
    tm.cols = 5;
    tm.rows = 4;
    tm.tile_size = 64;
    tm.spawn = {0, 2};
    tm.destination = {4, 2};

    // Initialize tiles[col][row]
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

// ---------------------------------------------------------------------------
// TileType enum values
// ---------------------------------------------------------------------------
TEST_CASE("TileType enum values", "[tile_map][unit]") {
    REQUIRE(static_cast<int>(TileType::GRASS) == 0);
    REQUIRE(static_cast<int>(TileType::PATH) == 1);
    REQUIRE(static_cast<int>(TileType::TOWER_SLOT) == 2);
}

// ---------------------------------------------------------------------------
// get_tile in-bounds
// ---------------------------------------------------------------------------
TEST_CASE("TileMap get_tile returns correct type for in-bounds", "[tile_map][unit]") {
    auto tm = make_test_map();

    REQUIRE(tm.get_tile(0, 0) == TileType::GRASS);
    REQUIRE(tm.get_tile(0, 2) == TileType::PATH);
    REQUIRE(tm.get_tile(4, 2) == TileType::PATH);
    REQUIRE(tm.get_tile(1, 1) == TileType::TOWER_SLOT);
    REQUIRE(tm.get_tile(3, 3) == TileType::TOWER_SLOT);
}

// ---------------------------------------------------------------------------
// get_tile out-of-bounds returns GRASS
// ---------------------------------------------------------------------------
TEST_CASE("TileMap get_tile returns GRASS for out-of-bounds", "[tile_map][unit]") {
    auto tm = make_test_map();

    // Negative indices
    REQUIRE(tm.get_tile(-1, 0) == TileType::GRASS);
    REQUIRE(tm.get_tile(0, -1) == TileType::GRASS);
    REQUIRE(tm.get_tile(-5, -5) == TileType::GRASS);

    // Beyond cols/rows
    REQUIRE(tm.get_tile(5, 0) == TileType::GRASS);
    REQUIRE(tm.get_tile(0, 4) == TileType::GRASS);
    REQUIRE(tm.get_tile(100, 100) == TileType::GRASS);
}

// ---------------------------------------------------------------------------
// tile_to_world
// ---------------------------------------------------------------------------
TEST_CASE("TileMap tile_to_world returns correct world position", "[tile_map][unit]") {
    auto tm = make_test_map();  // tile_size = 64

    SECTION("origin tile (0,0)") {
        auto pos = tm.tile_to_world(0, 0);
        REQUIRE_THAT(pos.x, WithinAbs(0.0f, 0.001f));
        REQUIRE_THAT(pos.y, WithinAbs(0.0f, 0.001f));
    }

    SECTION("tile (3, 5) with tile_size=64 -> (192, 320)") {
        TileMap tm2;
        tm2.cols = 10;
        tm2.rows = 10;
        tm2.tile_size = 64;
        tm2.tiles.resize(10, std::vector<TileType>(10, TileType::GRASS));
        tm2.spawn = {0, 0};
        tm2.destination = {9, 9};

        auto pos = tm2.tile_to_world(3, 5);
        REQUIRE_THAT(pos.x, WithinAbs(192.0f, 0.001f));
        REQUIRE_THAT(pos.y, WithinAbs(320.0f, 0.001f));
    }

    SECTION("tile (4, 3) with tile_size=64 -> (256, 192)") {
        auto pos = tm.tile_to_world(4, 3);
        REQUIRE_THAT(pos.x, WithinAbs(256.0f, 0.001f));
        REQUIRE_THAT(pos.y, WithinAbs(192.0f, 0.001f));
    }
}

// ---------------------------------------------------------------------------
// world_to_tile
// ---------------------------------------------------------------------------
TEST_CASE("TileMap world_to_tile returns correct grid coordinate", "[tile_map][unit]") {
    auto tm = make_test_map();  // tile_size = 64

    SECTION("position at tile center (192.5, 320.7) -> col=3, row=5") {
        TileMap tm2;
        tm2.cols = 10;
        tm2.rows = 10;
        tm2.tile_size = 64;
        tm2.tiles.resize(10, std::vector<TileType>(10, TileType::GRASS));
        tm2.spawn = {0, 0};
        tm2.destination = {9, 9};

        auto coord = tm2.world_to_tile(192.5f, 320.7f);
        REQUIRE(coord.col == 3);
        REQUIRE(coord.row == 5);
    }

    SECTION("position at tile boundary (64.0, 128.0) -> col=1, row=2") {
        auto coord = tm.world_to_tile(64.0f, 128.0f);
        REQUIRE(coord.col == 1);
        REQUIRE(coord.row == 2);
    }

    SECTION("position at origin (0.0, 0.0) -> col=0, row=0") {
        auto coord = tm.world_to_tile(0.0f, 0.0f);
        REQUIRE(coord.col == 0);
        REQUIRE(coord.row == 0);
    }

    SECTION("position just inside tile (63.9, 63.9) -> col=0, row=0") {
        auto coord = tm.world_to_tile(63.9f, 63.9f);
        REQUIRE(coord.col == 0);
        REQUIRE(coord.row == 0);
    }
}

// ---------------------------------------------------------------------------
// compute_visible_range — camera centered on grid
// ---------------------------------------------------------------------------
TEST_CASE("compute_visible_range with camera centered on grid", "[tile_map][unit]") {
    // 10x8 grid, tile_size=64, world = 640x512
    // Camera at center (320, 256), zoom=1.0, window 640x512
    // Viewport covers entire grid
    auto range = compute_visible_range(320.0f, 256.0f, 1.0f, 640, 512, 10, 8, 64);

    REQUIRE(range.min_col == 0);
    REQUIRE(range.max_col == 9);
    REQUIRE(range.min_row == 0);
    REQUIRE(range.max_row == 7);
}

// ---------------------------------------------------------------------------
// compute_visible_range — camera near grid edge (clamped)
// ---------------------------------------------------------------------------
TEST_CASE("compute_visible_range with camera near grid edge", "[tile_map][unit]") {
    // 10x8 grid, tile_size=64, world = 640x512
    // Camera at (100, 100), zoom=2.0, window 640x512
    // Viewport in world: width=320, height=256
    // left=100-160=-60, right=100+160=260, bottom=100-128=-28, top=100+128=228
    // min_col = max(floor(-60/64), 0) = max(-1, 0) = 0
    // max_col = min(floor(259.999/64), 9) = min(4, 9) = 4 (approx)
    // min_row = max(floor(-28/64), 0) = max(-1, 0) = 0
    // max_row = min(floor(227.999/64), 7) = min(3, 7) = 3
    auto range = compute_visible_range(100.0f, 100.0f, 2.0f, 640, 512, 10, 8, 64);

    REQUIRE(range.min_col == 0);
    REQUIRE(range.min_row == 0);
    // Verify clamped to valid bounds
    REQUIRE(range.max_col >= 0);
    REQUIRE(range.max_col <= 9);
    REQUIRE(range.max_row >= 0);
    REQUIRE(range.max_row <= 7);
}

// ---------------------------------------------------------------------------
// compute_visible_range — camera entirely outside grid (empty range)
// ---------------------------------------------------------------------------
TEST_CASE("compute_visible_range with camera entirely outside grid", "[tile_map][unit]") {
    // 10x8 grid, tile_size=64, world = 640x512
    // Camera at (-5000, -5000), zoom=1.0, window 100x100
    // Viewport: left=-5050, right=-4950, bottom=-5050, top=-4950
    // All negative -> clamped min > max
    auto range = compute_visible_range(-5000.0f, -5000.0f, 1.0f, 100, 100, 10, 8, 64);

    // Empty range: min > max
    REQUIRE(range.min_col > range.max_col);
}

// ---------------------------------------------------------------------------
// compute_visible_range — zoomed in (fewer tiles)
// ---------------------------------------------------------------------------
TEST_CASE("compute_visible_range at various zoom levels", "[tile_map][unit]") {
    // 15x10 grid, tile_size=64, world = 960x640
    // Camera at center (480, 320)

    SECTION("zoom=1.0, window 800x600 — most tiles visible") {
        auto range = compute_visible_range(480.0f, 320.0f, 1.0f, 800, 600, 15, 10, 64);
        int visible_cols = range.max_col - range.min_col + 1;
        int visible_rows = range.max_row - range.min_row + 1;
        // At zoom 1.0, viewport is 800x600 world units
        // 800/64 ≈ 12.5 tiles wide, 600/64 ≈ 9.4 tiles tall
        REQUIRE(visible_cols >= 12);
        REQUIRE(visible_rows >= 9);
    }

    SECTION("zoom=2.0, window 800x600 — fewer tiles visible") {
        auto range = compute_visible_range(480.0f, 320.0f, 2.0f, 800, 600, 15, 10, 64);
        int visible_cols = range.max_col - range.min_col + 1;
        int visible_rows = range.max_row - range.min_row + 1;
        // At zoom 2.0, viewport is 400x300 world units
        // 400/64 ≈ 6.25 tiles wide, 300/64 ≈ 4.7 tiles tall
        REQUIRE(visible_cols >= 6);
        REQUIRE(visible_cols <= 8);
        REQUIRE(visible_rows >= 4);
        REQUIRE(visible_rows <= 6);
    }

    SECTION("zoom=0.5, window 800x600 — more tiles visible (all)") {
        auto range = compute_visible_range(480.0f, 320.0f, 0.5f, 800, 600, 15, 10, 64);
        // At zoom 0.5, viewport is 1600x1200 world units — covers entire 960x640 grid
        REQUIRE(range.min_col == 0);
        REQUIRE(range.max_col == 14);
        REQUIRE(range.min_row == 0);
        REQUIRE(range.max_row == 9);
    }
}
