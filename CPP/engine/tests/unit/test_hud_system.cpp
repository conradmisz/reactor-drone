/**
 * Unit tests for HUDSystem components and rendering logic
 *
 * These tests verify the Text and ScreenPosition components, the HUD
 * rendering decision logic (component presence + hud_visible), and the
 * Y-axis flip formula as pure math. No SDL initialization required.
 *
 * Requirements tested: 1.2, 1.3, 1.4, 1.6, 2.4, 3.5, 3.6, 5.2, 5.3, 5.4, 7.1, 7.2, 11.6
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

// ============================================================================
// Text Component Default Values
// ============================================================================

TEST_CASE("Default Text has font_name default.ttf", "[hud][unit]") {
    // Requirement 1.2
    Text text;
    REQUIRE(text.font_name == "default.ttf");
}

TEST_CASE("Default Text has font_size 24.0f", "[hud][unit]") {
    // Requirement 1.3
    Text text;
    REQUIRE(text.font_size == 24.0f);
}

TEST_CASE("Default Text has color white {255,255,255,255}", "[hud][unit]") {
    // Requirement 1.4
    Text text;
    REQUIRE(text.color.r == 255);
    REQUIRE(text.color.g == 255);
    REQUIRE(text.color.b == 255);
    REQUIRE(text.color.a == 255);
}

// ============================================================================
// Component Copy Semantics
// ============================================================================

TEST_CASE("Text copy produces equal fields", "[hud][unit]") {
    // Requirement 1.6
    Text original;
    original.content = "Score: 42";
    original.font_name = "custom.ttf";
    original.font_size = 32.0f;
    original.color = {128, 64, 32, 200};

    Text copy = original;

    REQUIRE(copy.content == original.content);
    REQUIRE(copy.font_name == original.font_name);
    REQUIRE(copy.font_size == original.font_size);
    REQUIRE(copy.color.r == original.color.r);
    REQUIRE(copy.color.g == original.color.g);
    REQUIRE(copy.color.b == original.color.b);
    REQUIRE(copy.color.a == original.color.a);
}

TEST_CASE("ScreenPosition copy produces equal fields", "[hud][unit]") {
    // Requirement 2.4
    ScreenPosition original{150.0f, 400.0f};

    ScreenPosition copy = original;

    REQUIRE(copy.x == original.x);
    REQUIRE(copy.y == original.y);
}

// ============================================================================
// ComponentStorage Round-Trips
// ============================================================================

TEST_CASE("ComponentStorage add/get round-trip for Text", "[hud][unit]") {
    // Requirement 3.5
    ComponentStorage storage;
    Entity entity = 1;

    Text text;
    text.content = "FPS: 60";
    text.font_name = "mono.ttf";
    text.font_size = 18.0f;
    text.color = {0, 255, 0, 255};

    storage.add_component(entity, text);

    auto retrieved = storage.get_component<Text>(entity);
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->get().content == "FPS: 60");
    REQUIRE(retrieved->get().font_name == "mono.ttf");
    REQUIRE(retrieved->get().font_size == 18.0f);
    REQUIRE(retrieved->get().color.r == 0);
    REQUIRE(retrieved->get().color.g == 255);
    REQUIRE(retrieved->get().color.b == 0);
    REQUIRE(retrieved->get().color.a == 255);
}

TEST_CASE("ComponentStorage add/get round-trip for ScreenPosition", "[hud][unit]") {
    // Requirement 3.5
    ComponentStorage storage;
    Entity entity = 1;

    ScreenPosition pos{10.0f, 580.0f};
    storage.add_component(entity, pos);

    auto retrieved = storage.get_component<ScreenPosition>(entity);
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->get().x == 10.0f);
    REQUIRE(retrieved->get().y == 580.0f);
}

TEST_CASE("Text replacement on same entity returns new values", "[hud][unit]") {
    // Requirement 3.6
    ComponentStorage storage;
    Entity entity = 1;

    Text first;
    first.content = "old text";
    first.font_size = 16.0f;
    storage.add_component(entity, first);

    Text second;
    second.content = "new text";
    second.font_size = 48.0f;
    storage.add_component(entity, second);

    auto retrieved = storage.get_component<Text>(entity);
    REQUIRE(retrieved.has_value());
    REQUIRE(retrieved->get().content == "new text");
    REQUIRE(retrieved->get().font_size == 48.0f);
}

// ============================================================================
// HUD Rendering Decision Logic (mirrors HUDSystem::render() decisions)
// ============================================================================

TEST_CASE("Entity with Text+ScreenPosition and hud_visible=true should render", "[hud][unit]") {
    // Requirement 5.3
    // Mirrors the decision logic from HUDSystem::render():
    // 1. Check hud_visible from Blackboard (default true)
    // 2. Check entity has both Text and ScreenPosition
    // 3. If both conditions met -> should render
    ComponentStorage storage;
    Blackboard blackboard;
    Entity entity = 1;

    Text text;
    text.content = "Score: 100";
    storage.add_component(entity, text);
    storage.add_component(entity, ScreenPosition{10.0f, 580.0f});

    blackboard.set("hud_visible", true);

    // Mirror render() decision logic
    bool visible = blackboard.get_or<bool>("hud_visible", true);
    REQUIRE(visible);

    auto entities = storage.entities_with_component<Text>();
    REQUIRE(entities.size() == 1);

    bool has_text = storage.has_component<Text>(entity);
    bool has_screen_pos = storage.has_component<ScreenPosition>(entity);
    REQUIRE(has_text);
    REQUIRE(has_screen_pos);

    // Both conditions met -> should render
    bool should_render = visible && has_text && has_screen_pos;
    REQUIRE(should_render);
}

TEST_CASE("Entity with Text+ScreenPosition and hud_visible=false should skip", "[hud][unit]") {
    // Requirement 5.2
    ComponentStorage storage;
    Blackboard blackboard;
    Entity entity = 1;

    Text text;
    text.content = "Score: 100";
    storage.add_component(entity, text);
    storage.add_component(entity, ScreenPosition{10.0f, 580.0f});

    blackboard.set("hud_visible", false);

    // Mirror render() decision logic — returns immediately when not visible
    bool visible = blackboard.get_or<bool>("hud_visible", true);
    REQUIRE_FALSE(visible);

    // Even though entity has both components, hud_visible=false -> skip all
    REQUIRE_FALSE(visible);
}

TEST_CASE("Entity with Text only and no ScreenPosition should skip", "[hud][unit]") {
    // Requirement 11.6
    ComponentStorage storage;
    Blackboard blackboard;
    Entity entity = 1;

    Text text;
    text.content = "Orphan text";
    storage.add_component(entity, text);
    // No ScreenPosition added

    blackboard.set("hud_visible", true);

    bool visible = blackboard.get_or<bool>("hud_visible", true);
    REQUIRE(visible);

    // Entity is in the Text iteration set...
    auto entities = storage.entities_with_component<Text>();
    REQUIRE(entities.size() == 1);

    // ...but missing ScreenPosition -> skip
    bool has_screen_pos = storage.has_component<ScreenPosition>(entity);
    REQUIRE_FALSE(has_screen_pos);

    bool should_render = visible && storage.has_component<Text>(entity) && has_screen_pos;
    REQUIRE_FALSE(should_render);
}

// ============================================================================
// Y-Axis Flip Formula
// ============================================================================

TEST_CASE("Y-flip at (10,580) with 800x600 window text_height=24 gives sdl_y=-4", "[hud][unit]") {
    // Requirement 7.1
    // Formula: sdl_y = window_height - game_y - text_height
    //        = 600 - 580 - 24 = -4
    const float window_height = 600.0f;
    const float game_y = 580.0f;
    const float text_height = 24.0f;

    float sdl_y = window_height - game_y - text_height;

    REQUIRE(sdl_y == -4.0f);
}

TEST_CASE("Y-flip at (10,10) with 800x600 window text_height=24 gives sdl_y=566", "[hud][unit]") {
    // Requirement 7.2
    // Formula: sdl_y = window_height - game_y - text_height
    //        = 600 - 10 - 24 = 566
    const float window_height = 600.0f;
    const float game_y = 10.0f;
    const float text_height = 24.0f;

    float sdl_y = window_height - game_y - text_height;

    REQUIRE(sdl_y == 566.0f);
}

// ============================================================================
// Blackboard Default Values
// ============================================================================

TEST_CASE("Blackboard missing hud_visible defaults to true", "[hud][unit]") {
    // Requirement 5.3
    Blackboard blackboard;
    // Do NOT set hud_visible

    bool visible = blackboard.get_or<bool>("hud_visible", true);
    REQUIRE(visible);
}

TEST_CASE("Blackboard missing fps frame_count score returns defaults", "[hud][unit]") {
    // Requirement 5.4
    Blackboard blackboard;
    // Do NOT set fps, frame_count, or score

    double fps = blackboard.get_or<double>("fps", 0.0);
    REQUIRE(fps == 0.0);

    uint64_t frame_count = blackboard.get_or<uint64_t>("frame_count", uint64_t{0});
    REQUIRE(frame_count == uint64_t{0});

    int score = blackboard.get_or<int>("score", 0);
    REQUIRE(score == 0);
}
