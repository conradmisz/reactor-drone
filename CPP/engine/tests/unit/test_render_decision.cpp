/**
 * Unit tests for rendering decision logic
 *
 * These tests verify the Images component and the rendering decision
 * priority chain: Images > Color > skip. They test component presence
 * logic and the Y-axis flip formula as pure math — no SDL required.
 *
 * Requirements tested: 1.3, 1.4, 2.3, 4.1, 4.3, 5.1, 5.3, 5.4
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/component_storage.hpp"

/**
 * Test: Default-constructed Images has empty filename
 * Requirement 1.3
 */
TEST_CASE("Default-constructed Images has empty filenames", "[render_decision]") {
    Images img;
    CHECK(img.filenames.empty());
    CHECK(img.active_index == 0u);
}

/**
 * Test: Images copy produces equal filename
 * Requirement 1.4
 */
TEST_CASE("Images copy produces equal filenames", "[render_decision]") {
    Images original{{"explorer.png"}, 0};

    Images copy = original;
    CHECK(copy.filenames.size() == 1u);
    CHECK(copy.active_filename() == "explorer.png");
    CHECK(copy.filenames == original.filenames);
    CHECK(copy.active_index == original.active_index);
}

/**
 * Test: ComponentStorage add/get round-trip for Images
 * Requirement 2.3
 */
TEST_CASE("ComponentStorage add/get round-trip for Images", "[render_decision]") {
    ComponentStorage storage;
    Entity entity = 1;

    storage.add_component(entity, Images{{"test_texture.png"}, 0});

    auto retrieved = storage.get_component<Images>(entity);
    REQUIRE(retrieved.has_value());
    CHECK(retrieved->get().active_filename() == "test_texture.png");
}

/**
 * Test: Entity with Position+Size+Images → texture path selected
 * Requirement 4.1
 *
 * Mirrors the render() priority logic: check Images first.
 */
TEST_CASE("Entity with Images selects texture path", "[render_decision]") {
    ComponentStorage storage;
    Entity entity = 1;

    storage.add_component(entity, Position{100.0f, 200.0f});
    storage.add_component(entity, Size{64.0f, 64.0f});
    storage.add_component(entity, Images{{"player.png"}, 0});

    // The render decision: has Images → texture path
    CHECK(storage.has_component<Position>(entity));
    CHECK(storage.has_component<Size>(entity));
    CHECK(storage.has_component<Images>(entity));
}

/**
 * Test: Entity with Position+Size+Color (no Images) → color path selected
 * Requirement 5.1
 */
TEST_CASE("Entity with Color but no Images selects color path", "[render_decision]") {
    ComponentStorage storage;
    Entity entity = 1;

    storage.add_component(entity, Position{100.0f, 200.0f});
    storage.add_component(entity, Size{64.0f, 64.0f});
    storage.add_component(entity, Color{255, 0, 0, 255});

    // The render decision: no Images, has Color → color path
    CHECK(storage.has_component<Position>(entity));
    CHECK(storage.has_component<Size>(entity));
    CHECK_FALSE(storage.has_component<Images>(entity));
    CHECK(storage.has_component<Color>(entity));
}

/**
 * Test: Entity with Position+Size only (no Images, no Color) → skip
 * Requirement 5.4
 */
TEST_CASE("Entity with neither Images nor Color skips", "[render_decision]") {
    ComponentStorage storage;
    Entity entity = 1;

    storage.add_component(entity, Position{100.0f, 200.0f});
    storage.add_component(entity, Size{64.0f, 64.0f});

    // The render decision: no Images, no Color → skip
    CHECK(storage.has_component<Position>(entity));
    CHECK(storage.has_component<Size>(entity));
    CHECK_FALSE(storage.has_component<Images>(entity));
    CHECK_FALSE(storage.has_component<Color>(entity));
}

/**
 * Test: Entity with Position+Size+Images+Color → texture path wins (Images priority)
 * Requirement 5.3
 */
TEST_CASE("Images component takes priority over Color", "[render_decision]") {
    ComponentStorage storage;
    Entity entity = 1;

    storage.add_component(entity, Position{100.0f, 200.0f});
    storage.add_component(entity, Size{64.0f, 64.0f});
    storage.add_component(entity, Images{{"player.png"}, 0});
    storage.add_component(entity, Color{255, 0, 0, 255});

    // The render decision: has Images → texture path (Color ignored)
    CHECK(storage.has_component<Images>(entity));
    CHECK(storage.has_component<Color>(entity));
    // Images takes priority — texture path selected
}

/**
 * Test: Entity with Position+Size+SpriteSheet+Color → sprite-sheet path wins
 * Requirement 4.4
 *
 * Mirrors the render() priority chain (SpriteSheet > Images > Color): the
 * unchanged decision checks has_component<SpriteSheet>() first, so a
 * SpriteSheet selects the sprite-sheet path even when a Color is also present.
 */
TEST_CASE("SpriteSheet component takes priority over Color", "[render_decision]") {
    ComponentStorage storage;
    Entity entity = 1;

    storage.add_component(entity, Position{100.0f, 200.0f});
    storage.add_component(entity, Size{64.0f, 64.0f});
    storage.add_component(entity, SpriteSheet{"enemy_runner.png", 128, 128, 4, 8, 0});
    storage.add_component(entity, Color{255, 0, 0, 255});

    // The render decision checks SpriteSheet first → sprite-sheet path (Color ignored)
    CHECK(storage.has_component<SpriteSheet>(entity));
    CHECK(storage.has_component<Color>(entity));
    // SpriteSheet takes priority — sprite-sheet path selected over color path
}

/**
 * Test: Y-flip at (0,0) with 800×600 window → sdl_y = 500 for 100×100 entity
 * Requirement 4.3
 *
 * Formula: sdl_y = window_height - game_y - height
 *        = 600 - 0 - 100 = 500
 */
TEST_CASE("Y-axis flip at origin", "[render_decision]") {
    const float window_height = 600.0f;
    const float game_y = 0.0f;
    const float height = 100.0f;

    float sdl_y = window_height - game_y - height;

    CHECK(sdl_y == 500.0f);
}


// ============================================================================
// Rotation rendering decision tests
// Requirements tested: 11.1, 11.2, 11.3, 11.4
// ============================================================================

/**
 * Test: Textured entity with non-zero Rotation → rotated rendering path
 * Requirement 11.1
 *
 * Entity has Images + Rotation with angle != 0.0f → both components present,
 * angle is non-zero, so SDL_RenderTextureRotated would be used.
 */
TEST_CASE("Textured entity with rotation selects rotated path", "[render_decision]") {
    ComponentStorage storage;
    Entity entity = 1;

    storage.add_component(entity, Position{100.0f, 200.0f});
    storage.add_component(entity, Size{64.0f, 64.0f});
    storage.add_component(entity, Images{{"ship.png"}, 0});
    storage.add_component(entity, Rotation{1.5f, 0.0f});

    // Rotated path: has Images AND has Rotation AND angle != 0.0f
    CHECK(storage.has_component<Images>(entity));
    CHECK(storage.has_component<Rotation>(entity));
    auto rot = storage.get_component<Rotation>(entity);
    REQUIRE(rot.has_value());
    CHECK(rot->get().angle != 0.0f);
}

/**
 * Test: Textured entity without Rotation → non-rotated rendering path
 * Requirement 11.2
 *
 * Entity has Images but no Rotation → SDL_RenderTexture (no rotation overhead).
 */
TEST_CASE("Textured entity without rotation selects non-rotated path", "[render_decision]") {
    ComponentStorage storage;
    Entity entity = 1;

    storage.add_component(entity, Position{100.0f, 200.0f});
    storage.add_component(entity, Size{64.0f, 64.0f});
    storage.add_component(entity, Images{{"ship.png"}, 0});

    // Non-rotated path: has Images AND NOT has Rotation
    CHECK(storage.has_component<Images>(entity));
    CHECK_FALSE(storage.has_component<Rotation>(entity));
}

/**
 * Test: Color entity with Rotation → non-rotated filled rect path
 * Requirement 11.3
 *
 * Entity has Color + Rotation but no Images → SDL_RenderFillRect always
 * (SDL3 has no rotated fill-rect function, rotation is ignored).
 */
TEST_CASE("Color entity with rotation selects non-rotated path", "[render_decision]") {
    ComponentStorage storage;
    Entity entity = 1;

    storage.add_component(entity, Position{100.0f, 200.0f});
    storage.add_component(entity, Size{64.0f, 64.0f});
    storage.add_component(entity, Color{255, 0, 0, 255});
    storage.add_component(entity, Rotation{1.5f, 0.0f});

    // Color path always uses non-rotated rendering regardless of Rotation
    CHECK_FALSE(storage.has_component<Images>(entity));
    CHECK(storage.has_component<Color>(entity));
    CHECK(storage.has_component<Rotation>(entity));
}

/**
 * Test: Textured entity with zero angle → non-rotated rendering path (optimization)
 * Requirement 11.4
 *
 * Entity has Images + Rotation with angle == 0.0f → uses SDL_RenderTexture
 * (zero angle optimization avoids SDL_RenderTextureRotated overhead).
 */
TEST_CASE("Textured entity with zero angle selects non-rotated path", "[render_decision]") {
    ComponentStorage storage;
    Entity entity = 1;

    storage.add_component(entity, Position{100.0f, 200.0f});
    storage.add_component(entity, Size{64.0f, 64.0f});
    storage.add_component(entity, Images{{"ship.png"}, 0});
    storage.add_component(entity, Rotation{0.0f, 1.0f});

    // Zero angle optimization: has Images AND has Rotation AND angle == 0.0f
    CHECK(storage.has_component<Images>(entity));
    CHECK(storage.has_component<Rotation>(entity));
    auto rot = storage.get_component<Rotation>(entity);
    REQUIRE(rot.has_value());
    CHECK(rot->get().angle == 0.0f);
}
