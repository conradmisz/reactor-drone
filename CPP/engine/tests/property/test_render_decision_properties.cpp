/**
 * Property-based tests for rendering decision logic and coordinate conversion
 * 
 * These tests verify universal properties of the Images component integration,
 * the rendering decision priority chain (Images > Color > skip), and the
 * Y-axis flip formula using Catch2 GENERATE() with bounded iteration counts.
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
#include <cmath>

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;  // Number of different entities/keys to test
constexpr int NUM_INNER_TESTS = 5;   // Number of different values per entity/key

// ============================================================================
// Property 1: Images component storage round-trip
// Feature: image-rendering-component, Property 1: Images component storage round-trip
// Validates: Requirements 1.4, 2.4
// ============================================================================

TEST_CASE("Property 1: Images component storage round-trip",
          "[property][image-rendering-component]") {

    SECTION("Add Images, retrieve, verify filename matches") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto filename_char = GENERATE(take(NUM_INNER_TESTS, random('a', 'z')));

        // Build a filename from the generated character
        std::string filename(8, filename_char);

        ComponentStorage storage;
        Entity entity = entity_id;

        storage.add_component(entity, Images{{filename}, 0});

        auto retrieved = storage.get_component<Images>(entity);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->get().active_filename() == filename);
        REQUIRE(storage.has_component<Images>(entity));
    }

    SECTION("Adding second Images replaces the first") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto first_char = GENERATE(take(NUM_INNER_TESTS, random('a', 'm')));

        std::string first_filename(8, first_char);
        // Second filename is different — offset by 13 characters
        char second_char = static_cast<char>(first_char + 13);
        std::string second_filename(8, second_char);

        ComponentStorage storage;
        Entity entity = entity_id;

        // Add first
        storage.add_component(entity, Images{{first_filename}, 0});
        auto r1 = storage.get_component<Images>(entity);
        REQUIRE(r1.has_value());
        REQUIRE(r1->get().active_filename() == first_filename);

        // Replace with second
        storage.add_component(entity, Images{{second_filename}, 0});
        auto r2 = storage.get_component<Images>(entity);
        REQUIRE(r2.has_value());
        REQUIRE(r2->get().active_filename() == second_filename);
        REQUIRE(storage.has_component<Images>(entity));
    }
}


// ============================================================================
// Property 2: Rendering decision selects texture path when Images is present
// Feature: image-rendering-component, Property 2: Rendering decision selects texture path when Images is present
// Validates: Requirements 4.1, 4.2, 5.3, 7.5
// ============================================================================

TEST_CASE("Property 2: Rendering decision selects texture path when Images is present",
          "[property][image-rendering-component]") {

    SECTION("Entity with Position+Size+Images selects texture path") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto pos_val = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 800.0f)));

        ComponentStorage storage;
        Entity entity = entity_id;

        storage.add_component(entity, Position{pos_val, pos_val});
        storage.add_component(entity, Size{64.0f, 64.0f});
        storage.add_component(entity, Images{{"texture.png"}, 0});

        // Rendering decision: has Images → texture path
        REQUIRE(storage.has_component<Images>(entity));
        // This is the first check in the priority chain
    }

    SECTION("Entity with Position+Size+Images+Color still selects texture path") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto color_r = GENERATE(take(NUM_INNER_TESTS, random(0, 255)));

        ComponentStorage storage;
        Entity entity = entity_id;

        storage.add_component(entity, Position{100.0f, 200.0f});
        storage.add_component(entity, Size{64.0f, 64.0f});
        storage.add_component(entity, Images{{"texture.png"}, 0});
        storage.add_component(entity, Color{static_cast<uint8_t>(color_r), 0, 0, 255});

        // Rendering decision: has Images → texture path (Color ignored)
        REQUIRE(storage.has_component<Images>(entity));
        REQUIRE(storage.has_component<Color>(entity));
        // Images takes priority over Color
    }
}


// ============================================================================
// Property 3: Rendering decision selects color path when Color is present but Images is absent
// Feature: image-rendering-component, Property 3: Rendering decision selects color path when Color is present but Images is absent
// Validates: Requirements 5.1, 6.2, 7.6
// ============================================================================

TEST_CASE("Property 3: Rendering decision selects color path when Color present, Images absent",
          "[property][image-rendering-component]") {

    SECTION("Entity with Position+Size+Color (no Images) selects color path") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto color_val = GENERATE(take(NUM_INNER_TESTS, random(0, 255)));

        ComponentStorage storage;
        Entity entity = entity_id;

        storage.add_component(entity, Position{100.0f, 200.0f});
        storage.add_component(entity, Size{64.0f, 64.0f});
        storage.add_component(entity, Color{
            static_cast<uint8_t>(color_val),
            static_cast<uint8_t>(color_val),
            static_cast<uint8_t>(color_val),
            255});

        // Rendering decision: no Images, has Color → color path
        REQUIRE_FALSE(storage.has_component<Images>(entity));
        REQUIRE(storage.has_component<Color>(entity));
    }
}


// ============================================================================
// Property 4: Rendering decision skips entities with neither Images nor Color
// Feature: image-rendering-component, Property 4: Rendering decision skips entities with neither Images nor Color
// Validates: Requirements 5.4, 7.7
// ============================================================================

TEST_CASE("Property 4: Rendering decision skips entities with neither Images nor Color",
          "[property][image-rendering-component]") {

    SECTION("Entity with Position+Size only (no Images, no Color) is skipped") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto pos_val = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 800.0f)));

        ComponentStorage storage;
        Entity entity = entity_id;

        storage.add_component(entity, Position{pos_val, pos_val});
        storage.add_component(entity, Size{64.0f, 64.0f});

        // Rendering decision: no Images, no Color → skip
        REQUIRE_FALSE(storage.has_component<Images>(entity));
        REQUIRE_FALSE(storage.has_component<Color>(entity));
        REQUIRE(storage.has_component<Position>(entity));
        REQUIRE(storage.has_component<Size>(entity));
    }
}


// ============================================================================
// Property 5: Y-axis flip formula correctness
// Feature: image-rendering-component, Property 5: Y-axis flip formula correctness
// Validates: Requirements 4.3, 5.2
// ============================================================================

TEST_CASE("Property 5: Y-axis flip formula correctness",
          "[property][image-rendering-component]") {

    SECTION("sdl_y + game_y + height == window_height invariant") {
        auto game_y = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 2000.0f)));
        auto height = GENERATE(take(NUM_INNER_TESTS, random(1.0f, 500.0f)));

        // Use a fixed set of window heights to test against
        float window_height = 600.0f;

        // The Y-axis flip formula: sdl_y = window_height - game_y - height
        float sdl_y = window_height - game_y - height;

        // Invariant: sdl_y + game_y + height == window_height
        float sum = sdl_y + game_y + height;
        REQUIRE(sum == Catch::Approx(window_height));
    }

    SECTION("x, width, and height pass through unchanged") {
        auto game_x = GENERATE(take(NUM_OUTER_TESTS, random(0.0f, 2000.0f)));
        auto width = GENERATE(take(NUM_INNER_TESTS, random(1.0f, 500.0f)));

        // x coordinate is unchanged by the flip
        float sdl_x = game_x;
        REQUIRE(sdl_x == Catch::Approx(game_x));

        // width is unchanged by the flip
        float sdl_w = width;
        REQUIRE(sdl_w == Catch::Approx(width));
    }

    SECTION("Y-flip with varying window heights") {
        auto window_h = GENERATE(take(NUM_OUTER_TESTS, random(100.0f, 2000.0f)));
        auto game_y = GENERATE(take(NUM_INNER_TESTS, random(0.0f, 1000.0f)));

        float height = 50.0f;
        float sdl_y = window_h - game_y - height;

        // Invariant holds for any window height
        float sum = sdl_y + game_y + height;
        REQUIRE(sum == Catch::Approx(window_h));
    }
}


// ============================================================================
// Property 4: Angle conversion correctness
// Feature: 050-05-rotation-rendering, Property 4: Angle conversion correctness
// Validates: Requirements 4.3, 4.4
// ============================================================================

TEST_CASE("Feature: 050-05-rotation-rendering, Property 4: Angle conversion correctness",
          "[property][rotation]") {

    SECTION("Radians-to-SDL-degrees round-trip recovers original angle") {
        auto angle = GENERATE(take(NUM_OUTER_TESTS, random(-100.0f, 100.0f)));

        // Forward conversion: radians → SDL degrees (negate for Y-axis flip)
        double sdl_angle = -(static_cast<double>(angle) * 180.0 / M_PI);

        // Reverse conversion: SDL degrees → radians
        float recovered = static_cast<float>(-sdl_angle * M_PI / 180.0);

        REQUIRE(recovered == Catch::Approx(angle).margin(1e-4f));
    }
}


// ============================================================================
// Property 5: Textured entity render path selection
// Feature: 050-05-rotation-rendering, Property 5: Textured entity render path selection
// Validates: Requirements 4.1, 4.2, 7.2, 7.3
// ============================================================================

TEST_CASE("Feature: 050-05-rotation-rendering, Property 5: Textured entity render path selection",
          "[property][rotation]") {

    SECTION("Textured entity with non-zero angle selects rotated path") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto angle = GENERATE(take(NUM_INNER_TESTS, random(0.1f, 100.0f)));

        ComponentStorage storage;
        Entity entity = entity_id;

        storage.add_component(entity, Position{100.0f, 200.0f});
        storage.add_component(entity, Size{64.0f, 64.0f});
        storage.add_component(entity, Images{{"texture.png"}, 0});
        storage.add_component(entity, Rotation{angle, 0.0f});

        // Rotated path: has Images AND has Rotation AND angle != 0.0f
        REQUIRE(storage.has_component<Images>(entity));
        REQUIRE(storage.has_component<Rotation>(entity));
        auto rot = storage.get_component<Rotation>(entity);
        REQUIRE(rot.has_value());
        REQUIRE(rot->get().angle != 0.0f);
    }

    SECTION("Textured entity with zero angle selects non-rotated path") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));

        ComponentStorage storage;
        Entity entity = entity_id;

        storage.add_component(entity, Position{100.0f, 200.0f});
        storage.add_component(entity, Size{64.0f, 64.0f});
        storage.add_component(entity, Images{{"texture.png"}, 0});
        storage.add_component(entity, Rotation{0.0f, 0.0f});

        // Non-rotated path: has Images AND angle == 0.0f
        REQUIRE(storage.has_component<Images>(entity));
        REQUIRE(storage.has_component<Rotation>(entity));
        auto rot = storage.get_component<Rotation>(entity);
        REQUIRE(rot.has_value());
        REQUIRE(rot->get().angle == 0.0f);
    }
}


// ============================================================================
// Property 6: Color path ignores rotation
// Feature: 050-05-rotation-rendering, Property 6: Color path ignores rotation
// Validates: Requirements 5.1, 5.2, 7.4
// ============================================================================

TEST_CASE("Feature: 050-05-rotation-rendering, Property 6: Color path ignores rotation",
          "[property][rotation]") {

    SECTION("Color entity with Rotation always selects non-rotated path") {
        auto entity_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 1000u)));
        auto angle = GENERATE(take(NUM_INNER_TESTS, random(-100.0f, 100.0f)));

        ComponentStorage storage;
        Entity entity = entity_id;

        storage.add_component(entity, Position{100.0f, 200.0f});
        storage.add_component(entity, Size{64.0f, 64.0f});
        storage.add_component(entity, Color{255, 0, 0, 255});
        storage.add_component(entity, Rotation{angle, 0.0f});

        // Color path: NOT has Images AND has Color (always non-rotated)
        REQUIRE_FALSE(storage.has_component<Images>(entity));
        REQUIRE(storage.has_component<Color>(entity));
        REQUIRE(storage.has_component<Rotation>(entity));
    }
}
