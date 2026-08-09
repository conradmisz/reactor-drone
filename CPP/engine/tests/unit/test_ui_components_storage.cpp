/**
 * Unit tests for UI component stubs in ComponentStorage
 *
 * Verifies that the three Phase-1 UI component stubs — UIElement, UIState,
 * and UIScreen — are fully wired through the engine's ComponentStorage:
 * - add_component stores the value
 * - get_component returns a field-equal copy (store/get round-trip)
 * - has_component reports presence correctly
 * - remove_component removes the component
 * - has_component is false for an entity that never received the component
 * - remove_component on an absent component is a safe no-op
 *
 * Also verifies the documented default-construction defaults for each stub
 * (empty strings, zeroed rect, z_order == 0; all-false state with value == 0.0f;
 * empty screen name with active == false).
 *
 * Requirements tested: 9.1, 9.4, 3.8, 4.6, 5.4, 6.4, 6.5, 6.6
 */

#include <catch2/catch_test_macros.hpp>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"

// ---------------------------------------------------------------------------
// Default construction (R3.8, R4.6, R5.4)
// ---------------------------------------------------------------------------

/**
 * Test: UIElement default construction
 *
 * A default-constructed UIElement has empty strings, a zeroed rect, and
 * z_order == 0 (R3.8).
 */
TEST_CASE("UIElement: default construction has documented defaults", "[Engine][ui][unit]") {
    UIElement e{};

    REQUIRE(e.element_type == "");
    REQUIRE(e.label_text == "");
    REQUIRE(e.style_id == "");
    REQUIRE(e.on_click_fn == "");
    REQUIRE(e.rect.x == 0.0f);
    REQUIRE(e.rect.y == 0.0f);
    REQUIRE(e.rect.w == 0.0f);
    REQUIRE(e.rect.h == 0.0f);
    REQUIRE(e.z_order == 0);
}

/**
 * Test: UIState default construction
 *
 * A default-constructed UIState has all booleans false and value == 0.0f (R4.6).
 */
TEST_CASE("UIState: default construction has documented defaults", "[Engine][ui][unit]") {
    UIState s{};

    REQUIRE_FALSE(s.hovered);
    REQUIRE_FALSE(s.pressed);
    REQUIRE_FALSE(s.disabled);
    REQUIRE(s.value == 0.0f);
}

/**
 * Test: UIScreen default construction
 *
 * A default-constructed UIScreen has an empty screen_name and active == false (R5.4).
 */
TEST_CASE("UIScreen: default construction has documented defaults", "[Engine][ui][unit]") {
    UIScreen sc{};

    REQUIRE(sc.screen_name == "");
    REQUIRE_FALSE(sc.active);
}

// ---------------------------------------------------------------------------
// UIElement storage round-trip (R6.4, R6.5, R6.6, R9.1, R9.4)
// ---------------------------------------------------------------------------

/**
 * Test: UIElement add -> get round-trip, presence, and removal
 *
 * Adds a UIElement with non-default sentinel values (including all four rect
 * subfields), verifies get_component returns a field-equal copy and
 * has_component is true, then removes it and verifies has_component is false.
 */
TEST_CASE("UIElement: add/get round-trip then remove via ComponentStorage", "[Engine][ui][unit]") {
    ComponentStorage storage;
    Entity entity = 1;

    UIElement original{};
    original.element_type = "button";
    original.rect = UIRect{12.5f, 34.0f, 56.0f, 78.5f};
    original.label_text = "Play";
    original.style_id = "primary";
    original.on_click_fn = "on_play_clicked";
    original.z_order = 7;

    storage.add_component(entity, original);

    REQUIRE(storage.has_component<UIElement>(entity));

    auto retrieved = storage.get_component<UIElement>(entity);
    REQUIRE(retrieved.has_value());
    const UIElement& got = retrieved->get();

    REQUIRE(got.element_type == "button");
    REQUIRE(got.rect.x == 12.5f);
    REQUIRE(got.rect.y == 34.0f);
    REQUIRE(got.rect.w == 56.0f);
    REQUIRE(got.rect.h == 78.5f);
    REQUIRE(got.label_text == "Play");
    REQUIRE(got.style_id == "primary");
    REQUIRE(got.on_click_fn == "on_play_clicked");
    REQUIRE(got.z_order == 7);

    storage.remove_component<UIElement>(entity);
    REQUIRE_FALSE(storage.has_component<UIElement>(entity));
    REQUIRE_FALSE(storage.get_component<UIElement>(entity).has_value());
}

// ---------------------------------------------------------------------------
// UIState storage round-trip (R6.4, R6.5, R6.6, R9.1, R9.4)
// ---------------------------------------------------------------------------

/**
 * Test: UIState add -> get round-trip, presence, and removal
 *
 * Adds a UIState with non-default sentinel values, verifies the field-equal
 * round-trip and presence, then removes it and verifies absence.
 */
TEST_CASE("UIState: add/get round-trip then remove via ComponentStorage", "[Engine][ui][unit]") {
    ComponentStorage storage;
    Entity entity = 2;

    UIState original{};
    original.hovered = true;
    original.pressed = true;
    original.disabled = true;
    original.value = 0.75f;

    storage.add_component(entity, original);

    REQUIRE(storage.has_component<UIState>(entity));

    auto retrieved = storage.get_component<UIState>(entity);
    REQUIRE(retrieved.has_value());
    const UIState& got = retrieved->get();

    REQUIRE(got.hovered == true);
    REQUIRE(got.pressed == true);
    REQUIRE(got.disabled == true);
    REQUIRE(got.value == 0.75f);

    storage.remove_component<UIState>(entity);
    REQUIRE_FALSE(storage.has_component<UIState>(entity));
    REQUIRE_FALSE(storage.get_component<UIState>(entity).has_value());
}

// ---------------------------------------------------------------------------
// UIScreen storage round-trip (R6.4, R6.5, R6.6, R9.1, R9.4)
// ---------------------------------------------------------------------------

/**
 * Test: UIScreen add -> get round-trip, presence, and removal
 *
 * Adds a UIScreen with non-default sentinel values, verifies the field-equal
 * round-trip and presence, then removes it and verifies absence.
 */
TEST_CASE("UIScreen: add/get round-trip then remove via ComponentStorage", "[Engine][ui][unit]") {
    ComponentStorage storage;
    Entity entity = 3;

    UIScreen original{};
    original.screen_name = "main_menu";
    original.active = true;

    storage.add_component(entity, original);

    REQUIRE(storage.has_component<UIScreen>(entity));

    auto retrieved = storage.get_component<UIScreen>(entity);
    REQUIRE(retrieved.has_value());
    const UIScreen& got = retrieved->get();

    REQUIRE(got.screen_name == "main_menu");
    REQUIRE(got.active == true);

    storage.remove_component<UIScreen>(entity);
    REQUIRE_FALSE(storage.has_component<UIScreen>(entity));
    REQUIRE_FALSE(storage.get_component<UIScreen>(entity).has_value());
}

// ---------------------------------------------------------------------------
// Absence behavior (R6.6, R9.4)
// ---------------------------------------------------------------------------

/**
 * Test: has_component is false for an entity that never received the component
 *
 * Verifies R6.6: an entity that was never given a UI component reports
 * has_component == false for all three UI types, and get_component returns
 * std::nullopt.
 */
TEST_CASE("UI components: has_component is false for an entity that never received them",
          "[Engine][ui][unit]") {
    ComponentStorage storage;
    Entity given = 1;
    Entity never_given = 99;

    // Give 'given' all three UI components.
    storage.add_component(given, UIElement{});
    storage.add_component(given, UIState{});
    storage.add_component(given, UIScreen{});

    // A distinct entity that never received any UI component reports absence.
    REQUIRE_FALSE(storage.has_component<UIElement>(never_given));
    REQUIRE_FALSE(storage.has_component<UIState>(never_given));
    REQUIRE_FALSE(storage.has_component<UIScreen>(never_given));

    REQUIRE_FALSE(storage.get_component<UIElement>(never_given).has_value());
    REQUIRE_FALSE(storage.get_component<UIState>(never_given).has_value());
    REQUIRE_FALSE(storage.get_component<UIScreen>(never_given).has_value());
}

/**
 * Test: remove_component on an absent UI component is a safe no-op
 *
 * Verifies that calling remove_component for a UI component that was never
 * added does not throw or crash, and leaves the entity without the component.
 */
TEST_CASE("UI components: remove_component on an absent component is a safe no-op",
          "[Engine][ui][unit]") {
    ComponentStorage storage;
    Entity entity = 1;

    REQUIRE_NOTHROW(storage.remove_component<UIElement>(entity));
    REQUIRE_NOTHROW(storage.remove_component<UIState>(entity));
    REQUIRE_NOTHROW(storage.remove_component<UIScreen>(entity));

    REQUIRE_FALSE(storage.has_component<UIElement>(entity));
    REQUIRE_FALSE(storage.has_component<UIState>(entity));
    REQUIRE_FALSE(storage.has_component<UIScreen>(entity));
}
