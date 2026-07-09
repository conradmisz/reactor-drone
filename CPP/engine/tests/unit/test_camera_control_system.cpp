/**
 * Unit tests for CameraControlSystem
 *
 * These tests verify the apply_camera_controls() free function which
 * implements all camera zoom and pan logic. Tests bypass SDL keyboard
 * state by constructing CameraInput structs directly.
 *
 * Validates: Requirements REQ-1, REQ-2, REQ-3, REQ-4, REQ-5, REQ-9
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "engine/ecs/systems/camera_control_system.hpp"
#include "engine/ecs/blackboard.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("apply_camera_controls zoom", "[camera_control][unit]") {
    SECTION("zoom in increases camera.zoom") {
        Blackboard bb;
        bb.set("delta_time", 1.0f);
        bb.set("camera.zoom", 1.0f);
        bb.set("camera.lookat.x", 0.0f);
        bb.set("camera.lookat.y", 0.0f);

        CameraInput input;
        input.zoom_in = true;

        apply_camera_controls(bb, input);

        REQUIRE_THAT(bb.get<float>("camera.zoom"), WithinAbs(2.0f, 0.001));
    }

    SECTION("zoom out decreases camera.zoom") {
        Blackboard bb;
        bb.set("delta_time", 1.0f);
        bb.set("camera.zoom", 2.0f);
        bb.set("camera.lookat.x", 0.0f);
        bb.set("camera.lookat.y", 0.0f);

        CameraInput input;
        input.zoom_out = true;

        apply_camera_controls(bb, input);

        REQUIRE_THAT(bb.get<float>("camera.zoom"), WithinAbs(1.0f, 0.001));
    }

    SECTION("zoom clamp max: zoom at 4.0 stays at 4.0") {
        Blackboard bb;
        bb.set("delta_time", 1.0f);
        bb.set("camera.zoom", 4.0f);
        bb.set("camera.lookat.x", 0.0f);
        bb.set("camera.lookat.y", 0.0f);

        CameraInput input;
        input.zoom_in = true;

        apply_camera_controls(bb, input);

        REQUIRE_THAT(bb.get<float>("camera.zoom"), WithinAbs(4.0f, 0.001));
    }

    SECTION("zoom clamp min: zoom at 0.25 stays at 0.25") {
        Blackboard bb;
        bb.set("delta_time", 1.0f);
        bb.set("camera.zoom", 0.25f);
        bb.set("camera.lookat.x", 0.0f);
        bb.set("camera.lookat.y", 0.0f);

        CameraInput input;
        input.zoom_out = true;

        apply_camera_controls(bb, input);

        REQUIRE_THAT(bb.get<float>("camera.zoom"), WithinAbs(0.25f, 0.001));
    }
}


TEST_CASE("apply_camera_controls pan", "[camera_control][unit]") {
    SECTION("pan left decreases camera.lookat.x") {
        Blackboard bb;
        bb.set("delta_time", 1.0f);
        bb.set("camera.zoom", 1.0f);
        bb.set("camera.lookat.x", 0.0f);
        bb.set("camera.lookat.y", 0.0f);

        CameraInput input;
        input.pan_left = true;

        apply_camera_controls(bb, input);

        REQUIRE_THAT(bb.get<float>("camera.lookat.x"), WithinAbs(-300.0f, 0.001));
        REQUIRE_THAT(bb.get<float>("camera.lookat.y"), WithinAbs(0.0f, 0.001));
    }

    SECTION("pan right increases camera.lookat.x") {
        Blackboard bb;
        bb.set("delta_time", 1.0f);
        bb.set("camera.zoom", 1.0f);
        bb.set("camera.lookat.x", 0.0f);
        bb.set("camera.lookat.y", 0.0f);

        CameraInput input;
        input.pan_right = true;

        apply_camera_controls(bb, input);

        REQUIRE_THAT(bb.get<float>("camera.lookat.x"), WithinAbs(300.0f, 0.001));
        REQUIRE_THAT(bb.get<float>("camera.lookat.y"), WithinAbs(0.0f, 0.001));
    }

    SECTION("pan up increases camera.lookat.y (bottom-left origin)") {
        Blackboard bb;
        bb.set("delta_time", 1.0f);
        bb.set("camera.zoom", 1.0f);
        bb.set("camera.lookat.x", 0.0f);
        bb.set("camera.lookat.y", 0.0f);

        CameraInput input;
        input.pan_up = true;

        apply_camera_controls(bb, input);

        REQUIRE_THAT(bb.get<float>("camera.lookat.x"), WithinAbs(0.0f, 0.001));
        REQUIRE_THAT(bb.get<float>("camera.lookat.y"), WithinAbs(300.0f, 0.001));
    }

    SECTION("pan down decreases camera.lookat.y (bottom-left origin)") {
        Blackboard bb;
        bb.set("delta_time", 1.0f);
        bb.set("camera.zoom", 1.0f);
        bb.set("camera.lookat.x", 0.0f);
        bb.set("camera.lookat.y", 0.0f);

        CameraInput input;
        input.pan_down = true;

        apply_camera_controls(bb, input);

        REQUIRE_THAT(bb.get<float>("camera.lookat.x"), WithinAbs(0.0f, 0.001));
        REQUIRE_THAT(bb.get<float>("camera.lookat.y"), WithinAbs(-300.0f, 0.001));
    }
}

TEST_CASE("apply_camera_controls pan speed scales with zoom", "[camera_control][unit]") {
    SECTION("at zoom=2.0, pan delta is half of zoom=1.0") {
        Blackboard bb;
        bb.set("delta_time", 1.0f);
        bb.set("camera.zoom", 2.0f);
        bb.set("camera.lookat.x", 0.0f);
        bb.set("camera.lookat.y", 0.0f);

        CameraInput input;
        input.pan_right = true;

        apply_camera_controls(bb, input);

        // effective_pan = 300.0 * 1.0 / 2.0 = 150.0
        REQUIRE_THAT(bb.get<float>("camera.lookat.x"), WithinAbs(150.0f, 0.001));
    }
}

TEST_CASE("apply_camera_controls no input", "[camera_control][unit]") {
    SECTION("all false — no Blackboard changes") {
        Blackboard bb;
        bb.set("delta_time", 1.0f);
        bb.set("camera.zoom", 1.0f);
        bb.set("camera.lookat.x", 0.0f);
        bb.set("camera.lookat.y", 0.0f);

        CameraInput input;  // all false by default

        apply_camera_controls(bb, input);

        REQUIRE_THAT(bb.get<float>("camera.zoom"), WithinAbs(1.0f, 0.001));
        REQUIRE_THAT(bb.get<float>("camera.lookat.x"), WithinAbs(0.0f, 0.001));
        REQUIRE_THAT(bb.get<float>("camera.lookat.y"), WithinAbs(0.0f, 0.001));
    }
}


// ---------------------------------------------------------------------------
// Camera clamping tests
// ---------------------------------------------------------------------------

TEST_CASE("Camera clamping restricts X", "[camera_control][clamping][unit]") {
    SECTION("lookat far left is clamped to keep viewport within world") {
        Blackboard bb;
        bb.set("delta_time", 0.0f);  // no pan
        bb.set("camera.zoom", 1.0f);
        bb.set("camera.lookat.x", -1000.0f);  // far left
        bb.set("camera.lookat.y", 320.0f);
        bb.set("window_width", 800);
        bb.set("window_height", 600);
        bb.set("world.x", 0.0f);
        bb.set("world.y", 0.0f);
        bb.set("world.width", 960.0f);
        bb.set("world.height", 640.0f);

        CameraInput input;
        apply_camera_controls(bb, input);

        float result_x = bb.get<float>("camera.lookat.x");
        // half_view_w = 800/1.0/2 = 400
        // min lookat_x = 0 + 400 = 400
        REQUIRE_THAT(result_x, WithinAbs(400.0f, 0.01f));
    }

    SECTION("lookat far right is clamped to keep viewport within world") {
        Blackboard bb;
        bb.set("delta_time", 0.0f);
        bb.set("camera.zoom", 1.0f);
        bb.set("camera.lookat.x", 5000.0f);  // far right
        bb.set("camera.lookat.y", 320.0f);
        bb.set("window_width", 800);
        bb.set("window_height", 600);
        bb.set("world.x", 0.0f);
        bb.set("world.y", 0.0f);
        bb.set("world.width", 960.0f);
        bb.set("world.height", 640.0f);

        CameraInput input;
        apply_camera_controls(bb, input);

        float result_x = bb.get<float>("camera.lookat.x");
        // max lookat_x = 0 + 960 - 400 = 560
        REQUIRE_THAT(result_x, WithinAbs(560.0f, 0.01f));
    }
}

TEST_CASE("Camera clamping restricts Y", "[camera_control][clamping][unit]") {
    SECTION("lookat far below is clamped") {
        Blackboard bb;
        bb.set("delta_time", 0.0f);
        bb.set("camera.zoom", 1.0f);
        bb.set("camera.lookat.x", 480.0f);
        bb.set("camera.lookat.y", -1000.0f);  // far below
        bb.set("window_width", 800);
        bb.set("window_height", 600);
        bb.set("world.x", 0.0f);
        bb.set("world.y", 0.0f);
        bb.set("world.width", 960.0f);
        bb.set("world.height", 640.0f);

        CameraInput input;
        apply_camera_controls(bb, input);

        float result_y = bb.get<float>("camera.lookat.y");
        // half_view_h = 600/1.0/2 = 300
        // min lookat_y = 0 + 300 = 300
        REQUIRE_THAT(result_y, WithinAbs(300.0f, 0.01f));
    }

    SECTION("lookat far above is clamped") {
        Blackboard bb;
        bb.set("delta_time", 0.0f);
        bb.set("camera.zoom", 1.0f);
        bb.set("camera.lookat.x", 480.0f);
        bb.set("camera.lookat.y", 5000.0f);  // far above
        bb.set("window_width", 800);
        bb.set("window_height", 600);
        bb.set("world.x", 0.0f);
        bb.set("world.y", 0.0f);
        bb.set("world.width", 960.0f);
        bb.set("world.height", 640.0f);

        CameraInput input;
        apply_camera_controls(bb, input);

        float result_y = bb.get<float>("camera.lookat.y");
        // max lookat_y = 0 + 640 - 300 = 340
        REQUIRE_THAT(result_y, WithinAbs(340.0f, 0.01f));
    }
}

TEST_CASE("Camera clamping centers when world < viewport", "[camera_control][clamping][unit]") {
    Blackboard bb;
    bb.set("delta_time", 0.0f);
    bb.set("camera.zoom", 0.5f);  // viewport = 1600x1200, world = 960x640
    bb.set("camera.lookat.x", 100.0f);
    bb.set("camera.lookat.y", 100.0f);
    bb.set("window_width", 800);
    bb.set("window_height", 600);
    bb.set("world.x", 0.0f);
    bb.set("world.y", 0.0f);
    bb.set("world.width", 960.0f);
    bb.set("world.height", 640.0f);

    CameraInput input;
    apply_camera_controls(bb, input);

    float result_x = bb.get<float>("camera.lookat.x");
    float result_y = bb.get<float>("camera.lookat.y");
    // World smaller than viewport: center on world
    REQUIRE_THAT(result_x, WithinAbs(480.0f, 0.01f));
    REQUIRE_THAT(result_y, WithinAbs(320.0f, 0.01f));
}

TEST_CASE("No clamping without world bounds", "[camera_control][clamping][unit]") {
    Blackboard bb;
    bb.set("delta_time", 0.0f);
    bb.set("camera.zoom", 1.0f);
    bb.set("camera.lookat.x", -5000.0f);
    bb.set("camera.lookat.y", -5000.0f);
    bb.set("window_width", 800);
    bb.set("window_height", 600);
    // No world.width key — no clamping

    CameraInput input;
    apply_camera_controls(bb, input);

    float result_x = bb.get<float>("camera.lookat.x");
    float result_y = bb.get<float>("camera.lookat.y");
    // Should remain unchanged (no clamping)
    REQUIRE_THAT(result_x, WithinAbs(-5000.0f, 0.01f));
    REQUIRE_THAT(result_y, WithinAbs(-5000.0f, 0.01f));
}
