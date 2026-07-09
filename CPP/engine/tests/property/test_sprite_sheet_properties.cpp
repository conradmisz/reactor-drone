/**
 * Property-based tests for SpriteSheet component storage, source rectangle
 * calculation, and destruction pipeline integration.
 *
 * These tests verify universal properties for the SpriteSheet/atlas infrastructure:
 * component round-trip, add/remove inverse, source rect correctness, source rect
 * bounds, and destruction pipeline cleanup.
 *
 * Testing Framework: Catch2 v3
 * Constants: NUM_OUTER_TESTS = 10, NUM_INNER_TESTS = 5
 *
 * Requirements tested: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 2.1, 3.1, 3.2, 3.3,
 *                      3.4, 7.1, 7.2, 13.5, 13.6, 14.1, 14.2
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/ecs/sprite_sheet_math.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/destruction.hpp"
#include <random>
#include <string>
#include <vector>

static constexpr int NUM_OUTER_TESTS = 10;
static constexpr int NUM_INNER_TESTS = 5;

/// Helper: generate a random lowercase string of length [min_len, max_len]
static std::string random_string(std::mt19937& gen, int min_len, int max_len) {
    std::uniform_int_distribution<int> len_dist(min_len, max_len);
    std::uniform_int_distribution<int> char_dist('a', 'z');
    int len = len_dist(gen);
    std::string result;
    result.reserve(len);
    for (int i = 0; i < len; ++i) {
        result.push_back(static_cast<char>(char_dist(gen)));
    }
    return result;
}

// ============================================================================
// Property 1: SpriteSheet component round-trip
// Feature: 080-02-sprite-sheet-atlas, Property 1: SpriteSheet component round-trip
//
// **Validates: Requirements 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 2.1, 14.1**
//
// For any valid SpriteSheet component values (non-empty atlas_filename,
// frame_width 1-256, frame_height 1-256, columns 1-16, total_frames 1-96,
// current_frame in 0..total_frames-1), adding the SpriteSheet to an entity
// and then retrieving it shall return a SpriteSheet with identical field
// values for all six fields.
// ============================================================================
TEST_CASE("Property: SpriteSheet component round-trip", "[sprite_sheet]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    std::uniform_int_distribution<int> fw_dist(1, 256);
    std::uniform_int_distribution<int> fh_dist(1, 256);
    std::uniform_int_distribution<int> col_dist(1, 16);
    std::uniform_int_distribution<int> tf_dist(1, 96);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        EntityManager em;
        ComponentStorage storage;

        std::string atlas_filename = random_string(gen, 5, 20) + ".png";
        int frame_width = fw_dist(gen);
        int frame_height = fh_dist(gen);
        int columns = col_dist(gen);
        int total_frames = tf_dist(gen);
        std::uniform_int_distribution<int> cf_dist(0, total_frames - 1);
        int current_frame = cf_dist(gen);

        Entity e = em.create_entity();
        storage.add_component<SpriteSheet>(e, SpriteSheet{
            atlas_filename, frame_width, frame_height,
            columns, total_frames, current_frame
        });

        auto retrieved = storage.get_component<SpriteSheet>(e);
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->get().atlas_filename == atlas_filename);
        REQUIRE(retrieved->get().frame_width == frame_width);
        REQUIRE(retrieved->get().frame_height == frame_height);
        REQUIRE(retrieved->get().columns == columns);
        REQUIRE(retrieved->get().total_frames == total_frames);
        REQUIRE(retrieved->get().current_frame == current_frame);
    }
}

// ============================================================================
// Property 2: SpriteSheet add/remove inverse
// Feature: 080-02-sprite-sheet-atlas, Property 2: SpriteSheet add/remove inverse
//
// **Validates: Requirements 2.1, 14.2**
//
// For any entity, adding a SpriteSheet component and then calling
// has_component<SpriteSheet>() shall return true. Subsequently removing
// the SpriteSheet and then calling has_component<SpriteSheet>() shall
// return false.
// ============================================================================
TEST_CASE("Property: SpriteSheet add/remove inverse", "[sprite_sheet]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    std::uniform_int_distribution<int> fw_dist(1, 256);
    std::uniform_int_distribution<int> fh_dist(1, 256);
    std::uniform_int_distribution<int> col_dist(1, 16);
    std::uniform_int_distribution<int> tf_dist(1, 96);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        EntityManager em;
        ComponentStorage storage;

        std::string atlas_filename = random_string(gen, 5, 20) + ".png";
        int frame_width = fw_dist(gen);
        int frame_height = fh_dist(gen);
        int columns = col_dist(gen);
        int total_frames = tf_dist(gen);
        std::uniform_int_distribution<int> cf_dist(0, total_frames - 1);
        int current_frame = cf_dist(gen);

        Entity e = em.create_entity();
        storage.add_component<SpriteSheet>(e, SpriteSheet{
            atlas_filename, frame_width, frame_height,
            columns, total_frames, current_frame
        });

        REQUIRE(storage.has_component<SpriteSheet>(e));

        storage.remove_component<SpriteSheet>(e);

        REQUIRE_FALSE(storage.has_component<SpriteSheet>(e));
    }
}

// ============================================================================
// Property 3: Source rectangle calculation correctness
// Feature: 080-02-sprite-sheet-atlas, Property 3: Source rectangle calculation correctness
//
// **Validates: Requirements 3.1, 3.2, 3.3**
//
// For any valid atlas configuration (columns 1..16, frame_width 8..256,
// frame_height 8..256) and for any valid frame index, compute_source_rect
// shall return an SDL_FRect where x = (frame_index % columns) * frame_width,
// y = (frame_index / columns) * frame_height, w = frame_width, h = frame_height.
// ============================================================================
TEST_CASE("Property: Source rectangle calculation correctness", "[sprite_sheet]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    std::uniform_int_distribution<int> col_dist(1, 16);
    std::uniform_int_distribution<int> fw_dist(8, 256);
    std::uniform_int_distribution<int> fh_dist(8, 256);

    int columns = col_dist(gen);
    int frame_width = fw_dist(gen);
    int frame_height = fh_dist(gen);

    // Generate valid frame indices for this atlas config
    int max_frame = columns * 16 - 1;  // up to 16 rows
    std::uniform_int_distribution<int> fi_dist(0, max_frame);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        int frame_index = fi_dist(gen);

        SDL_FRect rect = compute_source_rect(frame_index, columns,
                                              frame_width, frame_height);

        float expected_x = static_cast<float>((frame_index % columns) * frame_width);
        float expected_y = static_cast<float>((frame_index / columns) * frame_height);
        float expected_w = static_cast<float>(frame_width);
        float expected_h = static_cast<float>(frame_height);

        REQUIRE(rect.x == expected_x);
        REQUIRE(rect.y == expected_y);
        REQUIRE(rect.w == expected_w);
        REQUIRE(rect.h == expected_h);
    }
}

// ============================================================================
// Property 4: Source rectangle bounds
// Feature: 080-02-sprite-sheet-atlas, Property 4: Source rectangle bounds
//
// **Validates: Requirements 3.4, 13.5, 13.6**
//
// For any valid atlas configuration (columns 1..16, rows 1..16, frame_width
// 8..256, frame_height 8..256) and for any valid frame index (0 to
// columns*rows - 1), the source rectangle shall satisfy: x >= 0, y >= 0,
// x + frame_width <= columns * frame_width, y + frame_height <= rows * frame_height.
// ============================================================================
TEST_CASE("Property: Source rectangle bounds", "[sprite_sheet]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    std::uniform_int_distribution<int> col_dist(1, 16);
    std::uniform_int_distribution<int> row_dist(1, 16);
    std::uniform_int_distribution<int> fw_dist(8, 256);
    std::uniform_int_distribution<int> fh_dist(8, 256);

    int columns = col_dist(gen);
    int rows = row_dist(gen);
    int frame_width = fw_dist(gen);
    int frame_height = fh_dist(gen);
    int total_frames = columns * rows;

    std::uniform_int_distribution<int> fi_dist(0, total_frames - 1);

    float atlas_width = static_cast<float>(columns * frame_width);
    float atlas_height = static_cast<float>(rows * frame_height);

    for (int inner = 0; inner < NUM_INNER_TESTS; ++inner) {
        int frame_index = fi_dist(gen);

        SDL_FRect rect = compute_source_rect(frame_index, columns,
                                              frame_width, frame_height);

        REQUIRE(rect.x >= 0.0f);
        REQUIRE(rect.y >= 0.0f);
        REQUIRE(rect.x + rect.w <= atlas_width);
        REQUIRE(rect.y + rect.h <= atlas_height);
    }
}

// ============================================================================
// Property 5: Destruction pipeline removes SpriteSheet
// Feature: 080-02-sprite-sheet-atlas, Property 5: Destruction pipeline removes SpriteSheet
//
// **Validates: Requirements 7.1, 7.2**
//
// For any entity that has a SpriteSheet component and is marked with
// DestroyRequest, after calling destroy_marked_entities(),
// has_component<SpriteSheet>() shall return false for that entity and
// the entity shall not be alive.
// ============================================================================
TEST_CASE("Property: Destruction pipeline removes SpriteSheet", "[sprite_sheet]") {
    auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
    std::mt19937 gen(seed);

    std::uniform_int_distribution<int> fw_dist(1, 256);
    std::uniform_int_distribution<int> fh_dist(1, 256);
    std::uniform_int_distribution<int> col_dist(1, 16);
    std::uniform_int_distribution<int> tf_dist(1, 96);

    EntityManager em;
    ComponentStorage storage;

    std::string atlas_filename = random_string(gen, 5, 20) + ".png";
    int frame_width = fw_dist(gen);
    int frame_height = fh_dist(gen);
    int columns = col_dist(gen);
    int total_frames = tf_dist(gen);
    std::uniform_int_distribution<int> cf_dist(0, total_frames - 1);
    int current_frame = cf_dist(gen);

    Entity e = em.create_entity();
    storage.add_component<SpriteSheet>(e, SpriteSheet{
        atlas_filename, frame_width, frame_height,
        columns, total_frames, current_frame
    });
    storage.add_component<DestroyRequest>(e, DestroyRequest{});

    destroy_marked_entities(em, storage);

    REQUIRE_FALSE(storage.has_component<SpriteSheet>(e));
    REQUIRE_FALSE(em.is_alive(e));
}
