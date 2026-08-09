/**
 * Unit tests for UI component cleanup in the destruction pipeline
 *
 * These tests verify that destroy_marked_entities() removes the three UI
 * component stubs (UIElement, UIState, UIScreen) from entities marked with
 * DestroyRequest, and that unmarked entities retain their UI components.
 *
 * Mirrors the idiom established in test_destruction_pipeline.cpp:
 * create entities via EntityManager, attach components via ComponentStorage,
 * mark with DestroyRequest, then call destroy_marked_entities(em, storage).
 *
 * Requirements tested: 9.2, 9.4, 7.1, 7.2, 7.3, 7.4
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/destruction.hpp"

/**
 * Test: Marked entity carrying all three UI components has them all removed
 *
 * An entity carrying UIElement + UIState + UIScreen plus DestroyRequest has
 * all three UI components removed after destroy_marked_entities() and is no
 * longer alive.
 *
 * Validates: Requirements 9.2, 7.1, 7.2, 7.3, 7.4
 */
TEST_CASE("Marked entity with all three UI components has them removed", "[Engine][ui][unit]") {
    EntityManager em;
    ComponentStorage storage;

    Entity entity = em.create_entity();
    storage.add_component(entity, UIElement{"button", UIRect{1.0f, 2.0f, 3.0f, 4.0f}, "Play", "default", "on_play", 5});
    storage.add_component(entity, UIState{true, false, true, 0.5f});
    storage.add_component(entity, UIScreen{"main_menu", true});
    storage.add_component(entity, DestroyRequest{});

    destroy_marked_entities(em, storage);

    REQUIRE_FALSE(storage.has_component<UIElement>(entity));
    REQUIRE_FALSE(storage.has_component<UIState>(entity));
    REQUIRE_FALSE(storage.has_component<UIScreen>(entity));
    REQUIRE_FALSE(em.is_alive(entity));
}

/**
 * Test: Unmarked entity retains its UI components after the pipeline runs
 *
 * An entity carrying the three UI components but no DestroyRequest still has
 * all three after destroy_marked_entities() runs (only marked entities are
 * cleaned), while a sibling entity marked with DestroyRequest is destroyed.
 *
 * Validates: Requirements 9.4, 7.1, 7.2, 7.3, 7.4
 */
TEST_CASE("Unmarked entity retains UI components after pipeline runs", "[Engine][ui][unit]") {
    EntityManager em;
    ComponentStorage storage;

    Entity keep = em.create_entity();
    storage.add_component(keep, UIElement{"slider", UIRect{10.0f, 20.0f, 30.0f, 40.0f}, "Volume", "slider_style", "on_change", 2});
    storage.add_component(keep, UIState{false, true, false, 0.75f});
    storage.add_component(keep, UIScreen{"settings", false});

    Entity doomed = em.create_entity();
    storage.add_component(doomed, UIElement{"label", UIRect{0.0f, 0.0f, 0.0f, 0.0f}, "Gone", "", "", 0});
    storage.add_component(doomed, DestroyRequest{});

    destroy_marked_entities(em, storage);

    // Unmarked entity still alive and retains all three UI components
    REQUIRE(em.is_alive(keep));
    REQUIRE(storage.has_component<UIElement>(keep));
    REQUIRE(storage.has_component<UIState>(keep));
    REQUIRE(storage.has_component<UIScreen>(keep));

    // Marked entity is cleaned up
    REQUIRE_FALSE(em.is_alive(doomed));
    REQUIRE_FALSE(storage.has_component<UIElement>(doomed));
}
