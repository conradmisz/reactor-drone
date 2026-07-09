/**
 * Unit tests for InputSystem mouse coordinate conversion
 *
 * Since we can't call SDL_GetMouseState without an SDL window, these tests
 * verify the MATH of the coordinate conversion formula only:
 *   world_x = screen_x - win_w / 2.0
 *   world_y = (win_h - screen_y) - win_h / 2.0
 *
 * Testing Framework: Catch2 v3
 * Tags: [input][mouse]
 *
 * Validates: Requirements 2.1, 2.2, 2.3, 2.4, 14.4
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Pure-math conversion matching InputSystem::process_events logic
static double mouse_world_x(float screen_x, int win_w) {
    return static_cast<double>(screen_x) - win_w / 2.0;
}

static double mouse_world_y(float screen_y, int win_h) {
    return static_cast<double>(win_h - screen_y) - win_h / 2.0;
}

TEST_CASE("Mouse coordinate conversion math", "[input][mouse]") {
    // Standard 800x600 window
    constexpr int WIN_W = 800;
    constexpr int WIN_H = 600;

    SECTION("Center of window maps to world origin (0, 0)") {
        // Screen (400, 300) → world (0, 0)
        double wx = mouse_world_x(400.0f, WIN_W);
        double wy = mouse_world_y(300.0f, WIN_H);

        REQUIRE(wx == Catch::Approx(0.0));
        REQUIRE(wy == Catch::Approx(0.0));
    }

    SECTION("Top-left corner maps to world (-400, 300)") {
        // Screen (0, 0) → world (-400, 300)
        double wx = mouse_world_x(0.0f, WIN_W);
        double wy = mouse_world_y(0.0f, WIN_H);

        REQUIRE(wx == Catch::Approx(-400.0));
        REQUIRE(wy == Catch::Approx(300.0));
    }

    SECTION("Bottom-right corner maps to world (400, -300)") {
        // Screen (800, 600) → world (400, -300)
        double wx = mouse_world_x(800.0f, WIN_W);
        double wy = mouse_world_y(600.0f, WIN_H);

        REQUIRE(wx == Catch::Approx(400.0));
        REQUIRE(wy == Catch::Approx(-300.0));
    }
}
