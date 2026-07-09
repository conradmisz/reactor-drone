/**
 * Property-based tests for CameraControlSystem
 *
 * These tests verify universal properties of apply_camera_controls() using
 * Catch2 GENERATE() with bounded iteration counts.
 *
 * Testing Framework: Catch2 v3 with GENERATE()
 * Iterations: NUM_OUTER_TESTS * NUM_INNER_TESTS = 50 per section
 *
 * Properties tested:
 *   1. Zoom bounds invariant — zoom always in [0.25, 4.0]
 *   2. No input identity — empty CameraInput produces no state change
 *   3. Pan-zoom inverse relationship — pan_delta * zoom = PAN_SPEED * dt
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include "engine/ecs/systems/camera_control_system.hpp"
#include "engine/ecs/blackboard.hpp"

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;
constexpr int NUM_INNER_TESTS = 5;

// ============================================================================
// Property 1: Zoom bounds invariant
// For any initial zoom in [0.01, 10.0] and any random delta_time in
// [0.001, 2.0], after applying zoom_in or zoom_out, the resulting zoom
// is always in [MIN_ZOOM, MAX_ZOOM] = [0.25, 4.0].
//
// **Validates: Requirements REQ-3**
// ============================================================================

TEST_CASE("Property 1: Zoom bounds invariant", "[camera_control][property]") {
    auto initial_zoom = GENERATE(take(NUM_OUTER_TESTS, random(0.01f, 10.0f)));
    auto dt = GENERATE(take(NUM_INNER_TESTS, random(0.001f, 2.0f)));

    SECTION("zoom_in keeps zoom within bounds") {
        Blackboard bb;
        bb.set<float>("delta_time", dt);
        bb.set<float>("camera.zoom", initial_zoom);
        bb.set<float>("camera.lookat.x", 0.0f);
        bb.set<float>("camera.lookat.y", 0.0f);

        CameraInput input;
        input.zoom_in = true;
        apply_camera_controls(bb, input);

        float result_zoom = bb.get<float>("camera.zoom");
        REQUIRE(result_zoom >= 0.25f);
        REQUIRE(result_zoom <= 4.0f);
    }

    SECTION("zoom_out keeps zoom within bounds") {
        Blackboard bb;
        bb.set<float>("delta_time", dt);
        bb.set<float>("camera.zoom", initial_zoom);
        bb.set<float>("camera.lookat.x", 0.0f);
        bb.set<float>("camera.lookat.y", 0.0f);

        CameraInput input;
        input.zoom_out = true;
        apply_camera_controls(bb, input);

        float result_zoom = bb.get<float>("camera.zoom");
        REQUIRE(result_zoom >= 0.25f);
        REQUIRE(result_zoom <= 4.0f);
    }
}


// ============================================================================
// Property 2: No input identity
// For any initial camera state (random zoom, lookat_x, lookat_y), applying
// an empty CameraInput (all flags false) produces no change to any value.
//
// **Validates: Requirements REQ-1, REQ-2, REQ-4**
// ============================================================================

TEST_CASE("Property 2: No input identity", "[camera_control][property]") {
    auto zoom = GENERATE(take(NUM_OUTER_TESTS, random(0.25f, 4.0f)));
    auto lookat = GENERATE(take(NUM_INNER_TESTS, random(-1000.0f, 1000.0f)));

    // Derive lookat_y from lookat_x to avoid a third nested GENERATE
    float lookat_x = lookat;
    float lookat_y = lookat * 0.7f - 50.0f;

    Blackboard bb;
    bb.set<float>("delta_time", 0.016f);
    bb.set<float>("camera.zoom", zoom);
    bb.set<float>("camera.lookat.x", lookat_x);
    bb.set<float>("camera.lookat.y", lookat_y);

    CameraInput input;  // all flags default to false
    apply_camera_controls(bb, input);

    float result_zoom = bb.get<float>("camera.zoom");
    float result_x = bb.get<float>("camera.lookat.x");
    float result_y = bb.get<float>("camera.lookat.y");

    REQUIRE(result_zoom == zoom);
    REQUIRE(result_x == lookat_x);
    REQUIRE(result_y == lookat_y);
}

// ============================================================================
// Property 3: Pan-zoom inverse relationship
// For any zoom > 0 and fixed delta_time, the pan delta multiplied by zoom
// equals PAN_SPEED * dt (a constant).
//
// Setup: lookat_x = 0, apply pan_right = true
// Verify: result_lookat_x * zoom ≈ 300.0 * dt
//
// **Validates: Requirements REQ-5**
// ============================================================================

TEST_CASE("Property 3: Pan-zoom inverse relationship", "[camera_control][property]") {
    auto zoom = GENERATE(take(NUM_OUTER_TESTS, random(0.25f, 4.0f)));
    auto dt = GENERATE(take(NUM_INNER_TESTS, random(0.001f, 2.0f)));

    Blackboard bb;
    bb.set<float>("delta_time", dt);
    bb.set<float>("camera.zoom", zoom);
    bb.set<float>("camera.lookat.x", 0.0f);
    bb.set<float>("camera.lookat.y", 0.0f);

    CameraInput input;
    input.pan_right = true;
    apply_camera_controls(bb, input);

    float result_x = bb.get<float>("camera.lookat.x");

    // effective_pan = PAN_SPEED * dt / zoom
    // result_x = 0 + effective_pan = PAN_SPEED * dt / zoom
    // result_x * zoom = PAN_SPEED * dt = 300.0 * dt
    float expected = 300.0f * dt;
    REQUIRE_THAT(result_x * zoom, Catch::Matchers::WithinAbs(expected, 0.01));
}
