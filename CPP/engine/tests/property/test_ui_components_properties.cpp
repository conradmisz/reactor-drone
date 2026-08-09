/**
 * Property-based tests for the three Phase-1 UI component stubs
 * (UIElement, UIState, UIScreen).
 *
 * Implements the four correctness properties from the o-040-01-setup design:
 *   Property 1: ComponentStorage store/get round-trip and presence
 *   Property 2: Destruction removes all UI components for any combination
 *   Property 3: Debug converter field fidelity
 *   Property 4: Entity-mapper coverage of UI-only entities
 *
 * Properties 3 and 4 drive the SAME production serialize path the game uses:
 * register_all_components() / refresh_entity_mapper() from game/debug_adapters.
 * No production logic is duplicated here.
 *
 * Feature: o-040-01-setup
 * Requirements: 9.3, 9.5
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/destruction.hpp"
#include "game/debug_adapters.hpp"

#include <algorithm>
#include <any>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Configurable test iteration counts (MANDATORY — workspace property-test-bounds policy)
constexpr int NUM_OUTER_TESTS = 10;  // Number of different entities / outer subjects
constexpr int NUM_INNER_TESTS = 5;   // Number of field-value variations per subject

// Alias for the Value variant's Object alternative (debug_adapters.hpp brings
// `using namespace engine::debug;` so Value / FieldInfo / TypeInfo are unqualified).
using ObjMap = std::unordered_map<std::string, std::unique_ptr<Value>>;

// ---------------------------------------------------------------------------
// Feature: o-040-01-setup, Property 1: ComponentStorage store/get round-trip
// and presence.
//
// For any entity and for any generated UIElement / UIState / UIScreen value,
// add_component then get_component returns a field-equal value, has_component
// is true for that entity, and has_component is false for a distinct entity
// that never received the component.
//
// **Validates: Requirements 6.4, 6.5, 6.6**
// ---------------------------------------------------------------------------
TEST_CASE("Property 1: ComponentStorage store/get round-trip and presence",
          "[Engine][ui][property]") {

    SECTION("UIElement round-trip + presence") {
        auto e_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 100000u)));
        auto fval = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));

        ComponentStorage storage;
        Entity e = static_cast<Entity>(e_id);
        Entity never = static_cast<Entity>(e_id + 200000u);  // distinct, never given

        std::string base = "s" + std::to_string(e_id) + "_" + std::to_string(static_cast<int>(fval));
        UIElement original{};
        original.element_type = base;
        original.rect = UIRect{static_cast<float>(fval), static_cast<float>(fval + 1.0),
                               static_cast<float>(fval + 2.0), static_cast<float>(fval + 3.0)};
        original.label_text = base + "_lbl";
        original.style_id = base + "_sty";
        original.on_click_fn = base + "_fn";
        original.z_order = static_cast<int>(fval);  // includes negatives

        storage.add_component(e, original);

        auto retrieved = storage.get_component<UIElement>(e);
        REQUIRE(retrieved.has_value());
        const UIElement& got = retrieved->get();
        REQUIRE(got.element_type == original.element_type);
        REQUIRE(got.rect.x == original.rect.x);
        REQUIRE(got.rect.y == original.rect.y);
        REQUIRE(got.rect.w == original.rect.w);
        REQUIRE(got.rect.h == original.rect.h);
        REQUIRE(got.label_text == original.label_text);
        REQUIRE(got.style_id == original.style_id);
        REQUIRE(got.on_click_fn == original.on_click_fn);
        REQUIRE(got.z_order == original.z_order);

        REQUIRE(storage.has_component<UIElement>(e));
        REQUIRE_FALSE(storage.has_component<UIElement>(never));
    }

    SECTION("UIState round-trip + presence") {
        auto e_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 100000u)));
        auto fval = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));

        ComponentStorage storage;
        Entity e = static_cast<Entity>(e_id);
        Entity never = static_cast<Entity>(e_id + 200000u);

        int bits = static_cast<int>(fval);
        UIState original{};
        original.hovered = (bits & 1) != 0;
        original.pressed = (bits & 2) != 0;
        original.disabled = (bits & 4) != 0;
        original.value = static_cast<float>(fval);

        storage.add_component(e, original);

        auto retrieved = storage.get_component<UIState>(e);
        REQUIRE(retrieved.has_value());
        const UIState& got = retrieved->get();
        REQUIRE(got.hovered == original.hovered);
        REQUIRE(got.pressed == original.pressed);
        REQUIRE(got.disabled == original.disabled);
        REQUIRE(got.value == original.value);

        REQUIRE(storage.has_component<UIState>(e));
        REQUIRE_FALSE(storage.has_component<UIState>(never));
    }

    SECTION("UIScreen round-trip + presence") {
        auto e_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 100000u)));
        auto fval = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));

        ComponentStorage storage;
        Entity e = static_cast<Entity>(e_id);
        Entity never = static_cast<Entity>(e_id + 200000u);

        UIScreen original{};
        original.screen_name = "screen_" + std::to_string(e_id) + "_" + std::to_string(static_cast<int>(fval));
        original.active = (static_cast<int>(fval) & 1) != 0;

        storage.add_component(e, original);

        auto retrieved = storage.get_component<UIScreen>(e);
        REQUIRE(retrieved.has_value());
        const UIScreen& got = retrieved->get();
        REQUIRE(got.screen_name == original.screen_name);
        REQUIRE(got.active == original.active);

        REQUIRE(storage.has_component<UIScreen>(e));
        REQUIRE_FALSE(storage.has_component<UIScreen>(never));
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-01-setup, Property 2: Destruction removes all UI components
// for any combination.
//
// For any entity given an arbitrary subset of {UIElement, UIState, UIScreen}
// (with random field values) plus a DestroyRequest, after
// destroy_marked_entities() runs, has_component returns false for all three
// UI component types on that entity.
//
// **Validates: Requirements 7.1, 7.2, 7.3, 7.4**
// ---------------------------------------------------------------------------
TEST_CASE("Property 2: Destruction removes all UI components for any combination",
          "[Engine][ui][property]") {

    SECTION("Any subset + DestroyRequest is fully cleaned") {
        // mask bit 0 -> UIElement, bit 1 -> UIState, bit 2 -> UIScreen
        auto mask = GENERATE(take(NUM_OUTER_TESTS, random(0, 7)));
        auto fval = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));

        EntityManager em;
        ComponentStorage storage;
        Entity e = em.create_entity();

        if (mask & 1) {
            UIElement ui{};
            ui.element_type = "t" + std::to_string(static_cast<int>(fval));
            ui.rect = UIRect{static_cast<float>(fval), static_cast<float>(fval),
                             static_cast<float>(fval), static_cast<float>(fval)};
            ui.z_order = static_cast<int>(fval);
            storage.add_component(e, ui);
        }
        if (mask & 2) {
            UIState ui{};
            ui.hovered = (mask & 1) != 0;
            ui.value = static_cast<float>(fval);
            storage.add_component(e, ui);
        }
        if (mask & 4) {
            UIScreen ui{};
            ui.screen_name = "scr" + std::to_string(static_cast<int>(fval));
            ui.active = (mask & 2) != 0;
            storage.add_component(e, ui);
        }
        storage.add_component(e, DestroyRequest{});

        destroy_marked_entities(em, storage);

        REQUIRE_FALSE(storage.has_component<UIElement>(e));
        REQUIRE_FALSE(storage.has_component<UIState>(e));
        REQUIRE_FALSE(storage.has_component<UIScreen>(e));
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-01-setup, Property 3: Debug converter field fidelity.
//
// For any generated UIElement / UIState / UIScreen value, the registered
// TypeIntrospector converter (populated by register_all_components) produces a
// JSON Object whose keys are exactly the declared FieldInfo field names and
// whose values equal the source struct fields; for UIElement the rect field is
// a nested object emitting numeric x/y/w/h equal to the source UIRect.
//
// **Validates: Requirements 8.2, 8.3**
// ---------------------------------------------------------------------------
TEST_CASE("Property 3: Debug converter field fidelity",
          "[Engine][ui][property]") {

    // Build the production registration once per generated case.
    auto require_keys_match = [](const ObjMap& obj, const TypeInfo& ti) {
        REQUIRE(obj.size() == ti.fields.size());
        for (const FieldInfo& f : ti.fields) {
            REQUIRE(obj.find(f.name) != obj.end());
        }
    };

    SECTION("UIElement converter fidelity (incl. nested rect)") {
        auto e_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 100000u)));
        auto fval = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));

        PropertyAccessorAdapter accessor;
        TypeIntrospectorAdapter introspector;
        register_all_components(accessor, introspector);

        std::string base = "u" + std::to_string(e_id) + "_" + std::to_string(static_cast<int>(fval));
        UIElement ui{};
        ui.element_type = base;
        ui.rect = UIRect{static_cast<float>(fval), static_cast<float>(fval + 1.0),
                         static_cast<float>(fval + 2.0), static_cast<float>(fval + 3.0)};
        ui.label_text = base + "_lbl";
        ui.style_id = base + "_sty";
        ui.on_click_fn = base + "_fn";
        ui.z_order = static_cast<int>(fval);

        Value v = introspector.cpp_to_value(std::any(ui), "UIElement");
        REQUIRE(v.type == Value::Type::Object);
        const ObjMap& obj = std::get<ObjMap>(v.data);

        require_keys_match(obj, introspector.get_type("UIElement"));

        REQUIRE(std::get<std::string>(obj.at("element_type")->data) == ui.element_type);
        REQUIRE(std::get<std::string>(obj.at("label_text")->data) == ui.label_text);
        REQUIRE(std::get<std::string>(obj.at("style_id")->data) == ui.style_id);
        REQUIRE(std::get<std::string>(obj.at("on_click_fn")->data) == ui.on_click_fn);
        REQUIRE(std::get<int64_t>(obj.at("z_order")->data) == static_cast<int64_t>(ui.z_order));

        // Nested rect object with numeric x/y/w/h (R8.3)
        REQUIRE(obj.at("rect")->type == Value::Type::Object);
        const ObjMap& rect = std::get<ObjMap>(obj.at("rect")->data);
        REQUIRE(rect.size() == 4);
        REQUIRE(rect.at("x")->type == Value::Type::Float);
        REQUIRE(rect.at("y")->type == Value::Type::Float);
        REQUIRE(rect.at("w")->type == Value::Type::Float);
        REQUIRE(rect.at("h")->type == Value::Type::Float);
        REQUIRE(std::get<double>(rect.at("x")->data) == static_cast<double>(ui.rect.x));
        REQUIRE(std::get<double>(rect.at("y")->data) == static_cast<double>(ui.rect.y));
        REQUIRE(std::get<double>(rect.at("w")->data) == static_cast<double>(ui.rect.w));
        REQUIRE(std::get<double>(rect.at("h")->data) == static_cast<double>(ui.rect.h));
    }

    SECTION("UIState converter fidelity") {
        auto e_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 100000u)));
        auto fval = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));

        PropertyAccessorAdapter accessor;
        TypeIntrospectorAdapter introspector;
        register_all_components(accessor, introspector);

        int bits = static_cast<int>(e_id);
        UIState ui{};
        ui.hovered = (bits & 1) != 0;
        ui.pressed = (bits & 2) != 0;
        ui.disabled = (bits & 4) != 0;
        ui.value = static_cast<float>(fval);

        Value v = introspector.cpp_to_value(std::any(ui), "UIState");
        REQUIRE(v.type == Value::Type::Object);
        const ObjMap& obj = std::get<ObjMap>(v.data);

        require_keys_match(obj, introspector.get_type("UIState"));

        REQUIRE(std::get<bool>(obj.at("hovered")->data) == ui.hovered);
        REQUIRE(std::get<bool>(obj.at("pressed")->data) == ui.pressed);
        REQUIRE(std::get<bool>(obj.at("disabled")->data) == ui.disabled);
        REQUIRE(obj.at("value")->type == Value::Type::Float);
        REQUIRE(std::get<double>(obj.at("value")->data) == static_cast<double>(ui.value));
    }

    SECTION("UIScreen converter fidelity") {
        auto e_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 100000u)));
        auto fval = GENERATE(take(NUM_INNER_TESTS, random(-1000.0, 1000.0)));

        PropertyAccessorAdapter accessor;
        TypeIntrospectorAdapter introspector;
        register_all_components(accessor, introspector);

        UIScreen ui{};
        ui.screen_name = "scr" + std::to_string(e_id) + "_" + std::to_string(static_cast<int>(fval));
        ui.active = (static_cast<int>(e_id) & 1) != 0;

        Value v = introspector.cpp_to_value(std::any(ui), "UIScreen");
        REQUIRE(v.type == Value::Type::Object);
        const ObjMap& obj = std::get<ObjMap>(v.data);

        require_keys_match(obj, introspector.get_type("UIScreen"));

        REQUIRE(std::get<std::string>(obj.at("screen_name")->data) == ui.screen_name);
        REQUIRE(std::get<bool>(obj.at("active")->data) == ui.active);
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-01-setup, Property 4: Entity-mapper coverage of UI-only
// entities.
//
// For any entity that carries only one of {UIElement, UIState, UIScreen} (and
// no other component type), after refresh_entity_mapper() runs the entity's
// actual id appears among the mapper's mapped entities, so it cannot be
// omitted from dump or trace output.
//
// **Validates: Requirements 8.4, 8.6**
// ---------------------------------------------------------------------------
TEST_CASE("Property 4: Entity-mapper coverage of UI-only entities",
          "[Engine][ui][property]") {

    SECTION("A UI-only entity is mapped after refresh") {
        auto e_id = GENERATE(take(NUM_OUTER_TESTS, random(1u, 100000u)));
        auto which = GENERATE(take(NUM_INNER_TESTS, random(0, 2)));

        ComponentStorage storage;
        EntityMapperAdapter mapper;
        Entity e = static_cast<Entity>(e_id);

        // Attach exactly one UI component (and nothing else).
        if (which == 0) {
            storage.add_component(e, UIElement{});
        } else if (which == 1) {
            storage.add_component(e, UIState{});
        } else {
            storage.add_component(e, UIScreen{});
        }

        refresh_entity_mapper(mapper, storage);

        // Collect actual ids reachable through the mapper.
        std::vector<Entity> mapped;
        for (int lid : mapper.get_logical_ids()) {
            mapped.push_back(mapper.get_actual_id(lid));
        }

        REQUIRE(std::find(mapped.begin(), mapped.end(), e) != mapped.end());
    }
}
