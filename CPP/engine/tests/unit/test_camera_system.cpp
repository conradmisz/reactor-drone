/**
 * Unit tests for CameraSystem
 *
 * These tests verify specific examples and edge cases for the CameraSystem's
 * world-to-screen affine transform. The CameraSystem reads camera configuration
 * from the Blackboard (lookat, zoom, window dimensions) and writes ScreenPosition
 * components for each entity with Position + Size.
 *
 * Validates: Requirements 1.3–1.6, 6.1, 6.6, 6.7, 6.8
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/systems/camera_system.hpp"

// Helper: set up a standard 800×600 Blackboard with given camera params
static Blackboard make_blackboard(float lookat_x = 0.0f, float lookat_y = 0.0f,
                                  float zoom = 1.0f,
                                  int win_w = 800, int win_h = 600) {
    Blackboard bb;
    bb.set("window_width", win_w);
    bb.set("window_height", win_h);
    bb.set("camera.lookat.x", lookat_x);
    bb.set("camera.lookat.y", lookat_y);
    bb.set("camera.zoom", zoom);
    return bb;
}

TEST_CASE("CameraSystem identity transform", "[camera_system][unit]") {
    ComponentStorage storage;
    CameraSystem camera;

    SECTION("world (0,0) maps to screen center (400,300) for 800x600") {
        auto bb = make_blackboard();

        Entity e = 1;
        storage.add_component<Position>(e, Position{0.0f, 0.0f});
        storage.add_component<Size>(e, Size{50.0f, 50.0f});

        camera.update(storage, bb);

        auto sp = storage.get_component<ScreenPosition>(e);
        REQUIRE(sp.has_value());
        REQUIRE(sp->get().x == 400.0f);
        REQUIRE(sp->get().y == 300.0f);
    }

    SECTION("world (-400,-300) maps to screen (0,0) — bottom-left corner") {
        auto bb = make_blackboard();

        Entity e = 1;
        storage.add_component<Position>(e, Position{-400.0f, -300.0f});
        storage.add_component<Size>(e, Size{50.0f, 50.0f});

        camera.update(storage, bb);

        auto sp = storage.get_component<ScreenPosition>(e);
        REQUIRE(sp.has_value());
        REQUIRE(sp->get().x == 0.0f);
        REQUIRE(sp->get().y == 0.0f);
    }

    SECTION("world (400,300) maps to screen (800,600) — top-right corner") {
        auto bb = make_blackboard();

        Entity e = 1;
        storage.add_component<Position>(e, Position{400.0f, 300.0f});
        storage.add_component<Size>(e, Size{50.0f, 50.0f});

        camera.update(storage, bb);

        auto sp = storage.get_component<ScreenPosition>(e);
        REQUIRE(sp.has_value());
        REQUIRE(sp->get().x == 800.0f);
        REQUIRE(sp->get().y == 600.0f);
    }
}


TEST_CASE("CameraSystem offset lookat", "[camera_system][unit]") {
    ComponentStorage storage;
    CameraSystem camera;

    SECTION("lookat=(100,50): world (100,50) maps to screen center (400,300)") {
        auto bb = make_blackboard(100.0f, 50.0f);

        Entity e = 1;
        storage.add_component<Position>(e, Position{100.0f, 50.0f});
        storage.add_component<Size>(e, Size{50.0f, 50.0f});

        camera.update(storage, bb);

        auto sp = storage.get_component<ScreenPosition>(e);
        REQUIRE(sp.has_value());
        REQUIRE(sp->get().x == 400.0f);
        REQUIRE(sp->get().y == 300.0f);
    }
}

TEST_CASE("CameraSystem zoom 2x", "[camera_system][unit]") {
    ComponentStorage storage;
    CameraSystem camera;

    SECTION("screen offsets from center are doubled") {
        // zoom=2.0, lookat=(0,0), 800x600
        // cam_width = 400, cam_height = 300
        // cam_left = -200, cam_bottom = -150
        // world(100, 75) → screen = (100 - (-200)) * 2 = 600, (75 - (-150)) * 2 = 450
        // Offset from center: (600-400, 450-300) = (200, 150)
        // At zoom=1.0: world(100,75) → screen(500, 375), offset = (100, 75)
        // So offset is doubled: (200, 150) = 2 × (100, 75) ✓
        auto bb = make_blackboard(0.0f, 0.0f, 2.0f);

        Entity e = 1;
        storage.add_component<Position>(e, Position{100.0f, 75.0f});
        storage.add_component<Size>(e, Size{50.0f, 50.0f});

        camera.update(storage, bb);

        auto sp = storage.get_component<ScreenPosition>(e);
        REQUIRE(sp.has_value());
        REQUIRE(sp->get().x == 600.0f);
        REQUIRE(sp->get().y == 450.0f);
    }
}

TEST_CASE("CameraSystem zoom 0.5x", "[camera_system][unit]") {
    ComponentStorage storage;
    CameraSystem camera;

    SECTION("screen offsets from center are halved") {
        // zoom=0.5, lookat=(0,0), 800x600
        // cam_width = 1600, cam_height = 1200
        // cam_left = -800, cam_bottom = -600
        // world(100, 75) → screen = (100 - (-800)) * 0.5 = 450, (75 - (-600)) * 0.5 = 337.5
        // Offset from center: (450-400, 337.5-300) = (50, 37.5)
        // At zoom=1.0: offset = (100, 75)
        // So offset is halved: (50, 37.5) = 0.5 × (100, 75) ✓
        auto bb = make_blackboard(0.0f, 0.0f, 0.5f);

        Entity e = 1;
        storage.add_component<Position>(e, Position{100.0f, 75.0f});
        storage.add_component<Size>(e, Size{50.0f, 50.0f});

        camera.update(storage, bb);

        auto sp = storage.get_component<ScreenPosition>(e);
        REQUIRE(sp.has_value());
        REQUIRE(sp->get().x == 450.0f);
        REQUIRE(sp->get().y == 337.5f);
    }
}

TEST_CASE("CameraSystem skips entity without Position", "[camera_system][unit]") {
    ComponentStorage storage;
    CameraSystem camera;

    SECTION("entity with only Size — no crash, no ScreenPosition written") {
        auto bb = make_blackboard();

        Entity e = 1;
        storage.add_component<Size>(e, Size{50.0f, 50.0f});

        // Should not crash
        camera.update(storage, bb);

        // No ScreenPosition should be written
        REQUIRE_FALSE(storage.has_component<ScreenPosition>(e));
    }
}

TEST_CASE("CameraSystem skips entity without Size", "[camera_system][unit]") {
    ComponentStorage storage;
    CameraSystem camera;

    SECTION("entity with only Position — no crash, no ScreenPosition written") {
        auto bb = make_blackboard();

        Entity e = 1;
        storage.add_component<Position>(e, Position{100.0f, 200.0f});

        // Should not crash
        camera.update(storage, bb);

        // No ScreenPosition should be written
        REQUIRE_FALSE(storage.has_component<ScreenPosition>(e));
    }
}

TEST_CASE("CameraSystem skips HUD entity", "[camera_system][unit]") {
    ComponentStorage storage;
    CameraSystem camera;

    SECTION("entity with ScreenPosition only (no Position) is untouched") {
        auto bb = make_blackboard();

        Entity hud = 1;
        storage.add_component<ScreenPosition>(hud, ScreenPosition{10.0f, 20.0f});

        camera.update(storage, bb);

        // ScreenPosition should remain unchanged
        auto sp = storage.get_component<ScreenPosition>(hud);
        REQUIRE(sp.has_value());
        REQUIRE(sp->get().x == 10.0f);
        REQUIRE(sp->get().y == 20.0f);
    }
}

TEST_CASE("CameraSystem zoom clamp", "[camera_system][unit]") {
    ComponentStorage storage;
    CameraSystem camera;

    SECTION("zoom=0.0 is clamped to 0.01f — no division by zero") {
        auto bb = make_blackboard(0.0f, 0.0f, 0.0f);

        Entity e = 1;
        storage.add_component<Position>(e, Position{0.0f, 0.0f});
        storage.add_component<Size>(e, Size{50.0f, 50.0f});

        // Should not crash (zoom clamped to 0.01f)
        camera.update(storage, bb);

        auto sp = storage.get_component<ScreenPosition>(e);
        REQUIRE(sp.has_value());

        // With zoom=0.01, lookat=(0,0), 800x600:
        // cam_width = 800/0.01 = 80000, cam_height = 600/0.01 = 60000
        // cam_left = -40000, cam_bottom = -30000
        // screen_x = (0 - (-40000)) * 0.01 = 400
        // screen_y = (0 - (-30000)) * 0.01 = 300
        // Center maps to center regardless of zoom (lookat is at origin)
        REQUIRE(sp->get().x == 400.0f);
        REQUIRE(sp->get().y == 300.0f);
    }
}
