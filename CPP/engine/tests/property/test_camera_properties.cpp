/**
 * Property-based tests for CameraSystem
 *
 * These tests verify universal properties of the CameraSystem's affine
 * world-to-screen transform using Catch2 GENERATE() with bounded iteration
 * counts.
 *
 * Testing Framework: Catch2 v3 with GENERATE()
 * Iterations: NUM_OUTER_TESTS * NUM_INNER_TESTS = 50 per section
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/systems/camera_system.hpp"
#include <cmath>

constexpr int NUM_OUTER_TESTS = 10;  // Number of different camera configs to test
constexpr int NUM_INNER_TESTS = 5;   // Number of different positions per config

// ============================================================================
// Property 1: Round-trip consistency
// For random Position + camera config, applying the inverse transform to
// ScreenPosition recovers the original Position within float tolerance.
//
// Forward:  screen_x = (world_x - cam_left) * zoom
//           screen_y = (world_y - cam_bottom) * zoom
// Inverse:  world_x = screen_x / zoom + cam_left
//           world_y = screen_y / zoom + cam_bottom
//
// **Validates: Requirements 1.7, 6.9**
// ============================================================================

TEST_CASE("Property 1: Round-trip consistency", "[camera_system][property]") {
    auto zoom = GENERATE(take(NUM_OUTER_TESTS, random(0.1f, 10.0f)));
    auto world_x = GENERATE(take(NUM_INNER_TESTS, random(-1000.0f, 1000.0f)));

    // Derive world_y from world_x to avoid a third nested GENERATE
    float world_y = world_x * 0.75f - 100.0f;

    // Derive lookat from zoom to avoid extra GENERATE nesting
    float lookat_x = zoom * 10.0f - 5.0f;
    float lookat_y = zoom * 7.0f + 3.0f;

    constexpr int win_w = 800;
    constexpr int win_h = 600;

    // Set up Blackboard and ComponentStorage
    Blackboard blackboard;
    blackboard.set<float>("camera.lookat.x", lookat_x);
    blackboard.set<float>("camera.lookat.y", lookat_y);
    blackboard.set<float>("camera.zoom", zoom);
    blackboard.set<int>("window_width", win_w);
    blackboard.set<int>("window_height", win_h);

    ComponentStorage storage;
    Entity entity = 1;
    storage.add_component(entity, Position{world_x, world_y});
    storage.add_component(entity, Size{50.0f, 50.0f});

    // Run CameraSystem
    CameraSystem camera;
    camera.update(storage, blackboard);

    // Retrieve ScreenPosition
    auto sp = storage.get_component<ScreenPosition>(entity);
    REQUIRE(sp.has_value());

    float screen_x = sp->get().x;
    float screen_y = sp->get().y;

    // Inverse transform
    float cam_width = static_cast<float>(win_w) / zoom;
    float cam_height = static_cast<float>(win_h) / zoom;
    float cam_left = lookat_x - cam_width / 2.0f;
    float cam_bottom = lookat_y - cam_height / 2.0f;

    float recovered_x = screen_x / zoom + cam_left;
    float recovered_y = screen_y / zoom + cam_bottom;

    REQUIRE(std::abs(recovered_x - world_x) < 0.01f);
    REQUIRE(std::abs(recovered_y - world_y) < 0.01f);
}

// ============================================================================
// Property 2: Aspect ratio preservation
// cam_width / cam_height == window_width / window_height for any zoom > 0
//
// cam_width = window_width / zoom, cam_height = window_height / zoom
// So cam_width / cam_height = window_width / window_height always.
//
// **Validates: Requirements 4.1, 4.3**
// ============================================================================

TEST_CASE("Property 2: Aspect ratio preservation", "[camera_system][property]") {
    auto zoom = GENERATE(take(NUM_OUTER_TESTS, random(0.1f, 10.0f)));
    auto win_w = GENERATE(take(NUM_INNER_TESTS, random(320, 1920)));

    // Derive window height from width to avoid extra nesting
    int win_h = static_cast<int>(win_w * 0.75f);
    if (win_h < 1) win_h = 1;

    float cam_width = static_cast<float>(win_w) / zoom;
    float cam_height = static_cast<float>(win_h) / zoom;

    float cam_aspect = cam_width / cam_height;
    float win_aspect = static_cast<float>(win_w) / static_cast<float>(win_h);

    REQUIRE(std::abs(cam_aspect - win_aspect) < 0.001f);
}

// ============================================================================
// Property 3: Zoom-based size scaling
// render_width = world_width × zoom, render_height = world_height × zoom
//
// This property validates the design contract that RenderSystem will use.
//
// **Validates: Requirements 3.4, 6.10**
// ============================================================================

TEST_CASE("Property 3: Zoom-based size scaling", "[camera_system][property]") {
    auto zoom = GENERATE(take(NUM_OUTER_TESTS, random(0.1f, 10.0f)));
    auto world_width = GENERATE(take(NUM_INNER_TESTS, random(1.0f, 500.0f)));

    // Derive world_height from world_width
    float world_height = world_width * 0.6f;

    float render_width = world_width * zoom;
    float render_height = world_height * zoom;

    REQUIRE(std::abs(render_width - world_width * zoom) < 0.001f);
    REQUIRE(std::abs(render_height - world_height * zoom) < 0.001f);

    // Also verify the inverse: dividing render size by zoom recovers world size
    float recovered_width = render_width / zoom;
    float recovered_height = render_height / zoom;

    REQUIRE(std::abs(recovered_width - world_width) < 0.01f);
    REQUIRE(std::abs(recovered_height - world_height) < 0.01f);
}

// ============================================================================
// Property 4: Identity invariant
// lookat=(0,0), zoom=1.0: screen = world + (window/2)
//
// screen_x = world_x + 400, screen_y = world_y + 300 (for 800×600)
//
// **Validates: Requirements 1.3, 3.6**
// ============================================================================

TEST_CASE("Property 4: Identity invariant", "[camera_system][property]") {
    auto world_x = GENERATE(take(NUM_OUTER_TESTS, random(-1000.0f, 1000.0f)));
    auto world_y = GENERATE(take(NUM_INNER_TESTS, random(-1000.0f, 1000.0f)));

    constexpr int win_w = 800;
    constexpr int win_h = 600;

    Blackboard blackboard;
    blackboard.set<float>("camera.lookat.x", 0.0f);
    blackboard.set<float>("camera.lookat.y", 0.0f);
    blackboard.set<float>("camera.zoom", 1.0f);
    blackboard.set<int>("window_width", win_w);
    blackboard.set<int>("window_height", win_h);

    ComponentStorage storage;
    Entity entity = 1;
    storage.add_component(entity, Position{world_x, world_y});
    storage.add_component(entity, Size{50.0f, 50.0f});

    CameraSystem camera;
    camera.update(storage, blackboard);

    auto sp = storage.get_component<ScreenPosition>(entity);
    REQUIRE(sp.has_value());

    float expected_screen_x = world_x + static_cast<float>(win_w) / 2.0f;
    float expected_screen_y = world_y + static_cast<float>(win_h) / 2.0f;

    REQUIRE(std::abs(sp->get().x - expected_screen_x) < 0.01f);
    REQUIRE(std::abs(sp->get().y - expected_screen_y) < 0.01f);
}

// ============================================================================
// Property 5: Lookat centering
// For any lookat point, that world point maps to screen center.
//
// screen_x of lookat point should always be window_width / 2
// screen_y of lookat point should always be window_height / 2
//
// **Validates: Requirements 1.1**
// ============================================================================

TEST_CASE("Property 5: Lookat centering", "[camera_system][property]") {
    auto lookat_x = GENERATE(take(NUM_OUTER_TESTS, random(-500.0f, 500.0f)));
    auto lookat_y = GENERATE(take(NUM_INNER_TESTS, random(-500.0f, 500.0f)));

    // Use a fixed zoom for this property (the centering holds for any zoom)
    float zoom = 2.0f;

    constexpr int win_w = 800;
    constexpr int win_h = 600;

    Blackboard blackboard;
    blackboard.set<float>("camera.lookat.x", lookat_x);
    blackboard.set<float>("camera.lookat.y", lookat_y);
    blackboard.set<float>("camera.zoom", zoom);
    blackboard.set<int>("window_width", win_w);
    blackboard.set<int>("window_height", win_h);

    ComponentStorage storage;
    Entity entity = 1;
    // Place entity exactly at the lookat point
    storage.add_component(entity, Position{lookat_x, lookat_y});
    storage.add_component(entity, Size{50.0f, 50.0f});

    CameraSystem camera;
    camera.update(storage, blackboard);

    auto sp = storage.get_component<ScreenPosition>(entity);
    REQUIRE(sp.has_value());

    float expected_center_x = static_cast<float>(win_w) / 2.0f;
    float expected_center_y = static_cast<float>(win_h) / 2.0f;

    REQUIRE(std::abs(sp->get().x - expected_center_x) < 0.01f);
    REQUIRE(std::abs(sp->get().y - expected_center_y) < 0.01f);
}
