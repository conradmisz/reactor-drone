/**
 * Unit tests for SpriteSheet component and source rectangle calculation
 *
 * These tests verify:
 * - compute_source_rect produces correct pixel coordinates for known frame indices
 * - SpriteSheet default field values
 * - SpriteSheet component storage round-trip (add, get, has, remove, entities_with)
 *
 * Requirements tested: 1.7, 3.1, 3.2, 3.3, 11.1, 11.2, 11.3, 11.4,
 *                      12.1, 12.2, 12.3, 12.4
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/sprite_sheet_math.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"

#include <algorithm>
#include <vector>

// -----------------------------------------------------------------------
// 1. Frame 0 in 8-col 64×64 atlas → (0, 0, 64, 64)
// Validates: Requirement 11.1
// -----------------------------------------------------------------------
TEST_CASE("Frame0SourceRect", "[sprite_sheet]") {
    SDL_FRect rect = compute_source_rect(0, 8, 64, 64);
    CHECK(rect.x == 0.0f);
    CHECK(rect.y == 0.0f);
    CHECK(rect.w == 64.0f);
    CHECK(rect.h == 64.0f);
}

// -----------------------------------------------------------------------
// 2. Frame 7 (last column first row) → (448, 0, 64, 64)
// Validates: Requirement 11.2
// -----------------------------------------------------------------------
TEST_CASE("Frame7SourceRect", "[sprite_sheet]") {
    SDL_FRect rect = compute_source_rect(7, 8, 64, 64);
    CHECK(rect.x == 448.0f);
    CHECK(rect.y == 0.0f);
    CHECK(rect.w == 64.0f);
    CHECK(rect.h == 64.0f);
}

// -----------------------------------------------------------------------
// 3. Frame 8 (first column second row) → (0, 64, 64, 64)
// Validates: Requirement 11.3
// -----------------------------------------------------------------------
TEST_CASE("Frame8SourceRect", "[sprite_sheet]") {
    SDL_FRect rect = compute_source_rect(8, 8, 64, 64);
    CHECK(rect.x == 0.0f);
    CHECK(rect.y == 64.0f);
    CHECK(rect.w == 64.0f);
    CHECK(rect.h == 64.0f);
}

// -----------------------------------------------------------------------
// 4. Frame 47 (last frame in 8×6 atlas) → (448, 320, 64, 64)
// Validates: Requirement 11.4
// -----------------------------------------------------------------------
TEST_CASE("Frame47SourceRect", "[sprite_sheet]") {
    SDL_FRect rect = compute_source_rect(47, 8, 64, 64);
    CHECK(rect.x == 448.0f);
    CHECK(rect.y == 320.0f);
    CHECK(rect.w == 64.0f);
    CHECK(rect.h == 64.0f);
}

// -----------------------------------------------------------------------
// 5. Default-constructed SpriteSheet has current_frame == 0
// Validates: Requirement 1.7
// -----------------------------------------------------------------------
TEST_CASE("DefaultCurrentFrame", "[sprite_sheet]") {
    SpriteSheet ss{};
    CHECK(ss.current_frame == 0);
}

// -----------------------------------------------------------------------
// 6. add_component<SpriteSheet>() then has_component returns true
// Validates: Requirement 12.1
// -----------------------------------------------------------------------
TEST_CASE("ComponentAddHas", "[sprite_sheet]") {
    EntityManager entity_manager;
    ComponentStorage storage;

    Entity e = entity_manager.create_entity();
    storage.add_component<SpriteSheet>(e, SpriteSheet{"atlas.png", 64, 64, 8, 48, 0});

    CHECK(storage.has_component<SpriteSheet>(e) == true);
}

// -----------------------------------------------------------------------
// 7. add then get returns matching fields for all 6 fields
// Validates: Requirement 12.2
// -----------------------------------------------------------------------
TEST_CASE("ComponentRoundTrip", "[sprite_sheet]") {
    EntityManager entity_manager;
    ComponentStorage storage;

    Entity e = entity_manager.create_entity();
    SpriteSheet original{"gems.png", 32, 48, 10, 60, 5};
    storage.add_component<SpriteSheet>(e, original);

    auto retrieved = storage.get_component<SpriteSheet>(e);
    REQUIRE(retrieved.has_value());

    const auto& ss = retrieved->get();
    CHECK(ss.atlas_filename == "gems.png");
    CHECK(ss.frame_width == 32);
    CHECK(ss.frame_height == 48);
    CHECK(ss.columns == 10);
    CHECK(ss.total_frames == 60);
    CHECK(ss.current_frame == 5);
}

// -----------------------------------------------------------------------
// 8. remove then has_component returns false
// Validates: Requirement 12.3
// -----------------------------------------------------------------------
TEST_CASE("ComponentRemove", "[sprite_sheet]") {
    EntityManager entity_manager;
    ComponentStorage storage;

    Entity e = entity_manager.create_entity();
    storage.add_component<SpriteSheet>(e, SpriteSheet{"atlas.png", 64, 64, 8, 48, 0});
    REQUIRE(storage.has_component<SpriteSheet>(e) == true);

    storage.remove_component<SpriteSheet>(e);
    CHECK(storage.has_component<SpriteSheet>(e) == false);
}

// -----------------------------------------------------------------------
// 9. entities_with_component returns all expected IDs
// Validates: Requirement 12.4
// -----------------------------------------------------------------------
TEST_CASE("EntitiesWithComponent", "[sprite_sheet]") {
    EntityManager entity_manager;
    ComponentStorage storage;

    Entity e1 = entity_manager.create_entity();
    Entity e2 = entity_manager.create_entity();
    Entity e3 = entity_manager.create_entity();
    CHECK_FALSE(storage.has_component<SpriteSheet>(e2));

    storage.add_component<SpriteSheet>(e1, SpriteSheet{"atlas.png", 64, 64, 8, 48, 0});
    storage.add_component<SpriteSheet>(e3, SpriteSheet{"gems.png", 32, 32, 4, 16, 0});
    // e2 intentionally has no SpriteSheet

    std::vector<Entity> entities = storage.entities_with_component<SpriteSheet>();

    CHECK(entities.size() == 2);

    // Sort for deterministic comparison (unordered_map iteration order is unspecified)
    std::sort(entities.begin(), entities.end());
    std::vector<Entity> expected = {e1, e3};
    std::sort(expected.begin(), expected.end());

    CHECK(entities == expected);
}
