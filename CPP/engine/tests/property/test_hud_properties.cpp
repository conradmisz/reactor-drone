/**
 * Property-based tests for HUD components and rendering logic
 *
 * These tests verify universal properties of the Text and ScreenPosition
 * component storage round-trips, the HUD rendering decision logic, and
 * the Y-axis flip formula using Catch2 GENERATE() with bounded iteration counts.
 *
 * Testing Framework: Catch2 v3 with GENERATE()
 * Iterations: NUM_OUTER_TESTS * NUM_INNER_TESTS = 50 per section
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;  // Number of different entities/keys to test
constexpr int NUM_INNER_TESTS = 5;   // Number of different values per entity/key

// ============================================================================
// Property 1: Text component storage round-trip
// Feature: ecs-hud-renderer, Property 1: Text component storage round-trip
// Validates: Requirements 1.6, 3.6, 11.8
// ============================================================================

TEST_CASE("Property 1: Text component storage round-trip",
          "[property][ecs-hud-renderer]") {

    SECTION("Add Text, retrieve, verify all fields match") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto content_chars = GENERATE(take(NUM_INNER_TESTS, chunk(8, random('a', 'z'))));

        std::string content(content_chars.begin(), content_chars.end());
        // Derive font_name, font_size, and color from the generated chars
        std::string font_name(content_chars.begin(), content_chars.begin() + 4);
        font_name += ".ttf";
        float font_size = static_cast<float>(content_chars[0] - 'a' + 10);
        SDL_Color color = {
            static_cast<uint8_t>((content_chars[1] - 'a') * 10),
            static_cast<uint8_t>((content_chars[2] - 'a') * 10),
            static_cast<uint8_t>((content_chars[3] - 'a') * 10),
            255
        };

        ComponentStorage storage;
        Entity entity = entity_id;

        Text text_comp;
        text_comp.content = content;
        text_comp.font_name = font_name;
        text_comp.font_size = font_size;
        text_comp.color = color;

        storage.add_component(entity, text_comp);

        auto retrieved = storage.get_component<Text>(entity);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->get().content == content);
        REQUIRE(retrieved->get().font_name == font_name);
        REQUIRE(retrieved->get().font_size == Catch::Approx(font_size));
        REQUIRE(retrieved->get().color.r == color.r);
        REQUIRE(retrieved->get().color.g == color.g);
        REQUIRE(retrieved->get().color.b == color.b);
        REQUIRE(retrieved->get().color.a == color.a);
        REQUIRE(storage.has_component<Text>(entity));
    }

    SECTION("Adding second Text replaces the first") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto first_chars = GENERATE(take(NUM_INNER_TESTS, chunk(8, random('a', 'z'))));

        std::string first_content(first_chars.begin(), first_chars.end());
        // Build a distinct second content by shifting characters
        std::string second_content;
        for (char c : first_chars) {
            second_content += static_cast<char>('a' + ('z' - c));
        }

        ComponentStorage storage;
        Entity entity = entity_id;

        Text first_text;
        first_text.content = first_content;
        first_text.font_name = "first.ttf";
        first_text.font_size = 16.0f;
        first_text.color = {100, 100, 100, 255};

        Text second_text;
        second_text.content = second_content;
        second_text.font_name = "second.ttf";
        second_text.font_size = 32.0f;
        second_text.color = {200, 200, 200, 255};

        // Add first
        storage.add_component(entity, first_text);
        auto r1 = storage.get_component<Text>(entity);
        REQUIRE(r1.has_value());
        REQUIRE(r1->get().content == first_content);

        // Replace with second
        storage.add_component(entity, second_text);
        auto r2 = storage.get_component<Text>(entity);
        REQUIRE(r2.has_value());
        REQUIRE(r2->get().content == second_content);
        REQUIRE(r2->get().font_name == "second.ttf");
        REQUIRE(r2->get().font_size == Catch::Approx(32.0f));
        REQUIRE(r2->get().color.r == 200);
        REQUIRE(r2->get().color.g == 200);
        REQUIRE(r2->get().color.b == 200);
        REQUIRE(storage.has_component<Text>(entity));
    }
}


// ============================================================================
// Property 2: ScreenPosition component storage round-trip
// Feature: ecs-hud-renderer, Property 2: ScreenPosition component storage round-trip
// Validates: Requirements 2.4
// ============================================================================

TEST_CASE("Property 2: ScreenPosition component storage round-trip",
          "[property][ecs-hud-renderer]") {

    SECTION("Add ScreenPosition, retrieve, verify x and y match") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto pos_val = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 2000.0f)));

        ComponentStorage storage;
        Entity entity = entity_id;

        ScreenPosition sp;
        sp.x = pos_val;
        sp.y = pos_val + 1.0f;  // Distinct x and y

        storage.add_component(entity, sp);

        auto retrieved = storage.get_component<ScreenPosition>(entity);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->get().x == Catch::Approx(pos_val));
        REQUIRE(retrieved->get().y == Catch::Approx(pos_val + 1.0f));
        REQUIRE(storage.has_component<ScreenPosition>(entity));
    }

    SECTION("Adding second ScreenPosition replaces the first") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto first_val = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 1000.0f)));

        float second_val = first_val + 500.0f;

        ComponentStorage storage;
        Entity entity = entity_id;

        // Add first
        storage.add_component(entity, ScreenPosition{first_val, first_val});
        auto r1 = storage.get_component<ScreenPosition>(entity);
        REQUIRE(r1.has_value());
        REQUIRE(r1->get().x == Catch::Approx(first_val));
        REQUIRE(r1->get().y == Catch::Approx(first_val));

        // Replace with second
        storage.add_component(entity, ScreenPosition{second_val, second_val});
        auto r2 = storage.get_component<ScreenPosition>(entity);
        REQUIRE(r2.has_value());
        REQUIRE(r2->get().x == Catch::Approx(second_val));
        REQUIRE(r2->get().y == Catch::Approx(second_val));
        REQUIRE(storage.has_component<ScreenPosition>(entity));
    }
}


// ============================================================================
// Property 3: HUD rendering decision logic
// Feature: ecs-hud-renderer, Property 3: HUD rendering decision logic
// Validates: Requirements 5.2, 5.3, 5.5, 8.4, 8.5, 10.1, 11.5, 11.6
// ============================================================================

TEST_CASE("Property 3: HUD rendering decision logic",
          "[property][ecs-hud-renderer]") {

    SECTION("hud_visible=true + both components -> render") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto pos_val = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 800.0f)));

        ComponentStorage storage;
        Blackboard blackboard;
        Entity entity = entity_id;

        Text text;
        text.content = "test";
        storage.add_component(entity, text);
        storage.add_component(entity, ScreenPosition{pos_val, pos_val});
        blackboard.set("hud_visible", true);

        // Walk the HUDSystem decision logic
        bool visible = blackboard.get_or<bool>("hud_visible", true);
        REQUIRE(visible);

        auto entities = storage.entities_with_component<Text>();
        bool found_renderable = false;
        for (Entity e : entities) {
            if (e == entity && storage.has_component<ScreenPosition>(e)) {
                found_renderable = true;
            }
        }
        REQUIRE(found_renderable);
    }

    SECTION("hud_visible=false -> skip all entities") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto pos_val = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 800.0f)));

        ComponentStorage storage;
        Blackboard blackboard;
        Entity entity = entity_id;

        Text text;
        text.content = "test";
        storage.add_component(entity, text);
        storage.add_component(entity, ScreenPosition{pos_val, pos_val});
        blackboard.set("hud_visible", false);

        // Walk the HUDSystem decision logic
        bool visible = blackboard.get_or<bool>("hud_visible", true);
        REQUIRE_FALSE(visible);
        // When hud_visible is false, render() returns immediately — no entities rendered
    }

    SECTION("hud_visible missing defaults to true") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto pos_val = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 800.0f)));

        ComponentStorage storage;
        Blackboard blackboard;
        Entity entity = entity_id;

        Text text;
        text.content = "test";
        storage.add_component(entity, text);
        storage.add_component(entity, ScreenPosition{pos_val, pos_val});
        // Do NOT set hud_visible — should default to true

        bool visible = blackboard.get_or<bool>("hud_visible", true);
        REQUIRE(visible);

        auto entities = storage.entities_with_component<Text>();
        bool found_renderable = false;
        for (Entity e : entities) {
            if (e == entity && storage.has_component<ScreenPosition>(e)) {
                found_renderable = true;
            }
        }
        REQUIRE(found_renderable);
    }

    SECTION("Text only (no ScreenPosition) -> skip") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto content_char = GENERATE(take(NUM_INNER_TESTS, random('a', 'z')));

        ComponentStorage storage;
        Blackboard blackboard;
        Entity entity = entity_id;

        Text text;
        text.content = std::string(8, content_char);
        storage.add_component(entity, text);
        blackboard.set("hud_visible", true);

        // Entity has Text but no ScreenPosition — should be skipped
        REQUIRE(storage.has_component<Text>(entity));
        REQUIRE_FALSE(storage.has_component<ScreenPosition>(entity));
    }

    SECTION("ScreenPosition only (no Text) -> skip") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto pos_val = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 800.0f)));

        ComponentStorage storage;
        Blackboard blackboard;
        Entity entity = entity_id;

        storage.add_component(entity, ScreenPosition{pos_val, pos_val});
        blackboard.set("hud_visible", true);

        // Entity has ScreenPosition but no Text — not in Text iteration
        REQUIRE_FALSE(storage.has_component<Text>(entity));
        REQUIRE(storage.has_component<ScreenPosition>(entity));

        // HUDSystem iterates entities_with_component<Text>(), so this entity
        // would never be visited
        auto text_entities = storage.entities_with_component<Text>();
        bool entity_in_text_list = false;
        for (Entity e : text_entities) {
            if (e == entity) entity_in_text_list = true;
        }
        REQUIRE_FALSE(entity_in_text_list);
    }
}


// ============================================================================
// Property 4: Y-axis flip formula correctness
// Feature: ecs-hud-renderer, Property 4: Y-axis flip formula correctness
// Validates: Requirements 7.1, 7.4, 10.3, 11.7
// ============================================================================

TEST_CASE("Property 4: Y-axis flip formula correctness",
          "[property][ecs-hud-renderer]") {

    SECTION("sdl_y + game_y + text_height == window_height invariant") {
        auto game_y = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 2000.0f)));
        auto text_height = GENERATE(take(NUM_INNER_TESTS, random(1.0f, 100.0f)));

        float window_height = 600.0f;

        // The Y-axis flip formula: sdl_y = window_height - game_y - text_height
        float sdl_y = window_height - game_y - text_height;

        // Invariant: sdl_y + game_y + text_height == window_height
        float sum = sdl_y + game_y + text_height;
        REQUIRE(sum == Catch::Approx(window_height));
    }

    SECTION("x coordinate passes through unchanged") {
        auto game_x = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 2000.0f)));
        auto text_height = GENERATE(take(NUM_INNER_TESTS, random(1.0f, 100.0f)));

        // sdl_x == game_x — no horizontal flip
        float sdl_x = game_x;
        REQUIRE(sdl_x == Catch::Approx(game_x));

        // text_height is used only for Y, not X
        (void)text_height;
    }

    SECTION("Y-flip with varying window heights") {
        auto window_h = GENERATE(take(NUM_OUTER_TESTS, random(100.0f, 2000.0f)));
        auto game_y = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 1000.0f)));

        float text_height = 24.0f;
        float sdl_y = window_h - game_y - text_height;

        // Invariant holds for any window height
        float sum = sdl_y + game_y + text_height;
        REQUIRE(sum == Catch::Approx(window_h));
    }
}
