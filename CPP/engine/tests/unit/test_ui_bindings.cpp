/**
 * Unit tests for the ui.* Lua bindings (lua_bindings.cpp) — Option-040 Phase 6.
 *
 * Each test registers the engine.* + ui.* bindings on a fresh LuaManager, stores
 * the live engine pointers, builds widget entities / a screen stack as needed,
 * runs a Lua snippet through the ui.* table, and asserts the resulting C++ state
 * (component mutation or ui.cmd.* Blackboard command). Headless (no SDL window).
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "engine/lua_manager.hpp"
#include "engine/lua_bindings.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/systems/screen_stack_system.hpp"

#include <string>
#include <vector>

namespace {

struct UiBindingsFixture {
    LuaManager lua;
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;

    UiBindingsFixture() {
        register_bindings(lua.state());
        store_engine_pointers(lua.state(), &cs, &em, &bb);
    }

    Entity make_widget(const std::string& type, const std::string& label,
                       float value = 0.0f, bool disabled = false) {
        Entity w = em.create_entity();
        UIElement el;
        el.element_type = type;
        el.label_text = label;
        cs.add_component<UIElement>(w, el);
        UIState st;
        st.value = value;
        st.disabled = disabled;
        cs.add_component<UIState>(w, st);
        return w;
    }
};

}  // namespace

TEST_CASE("ui.set_label mutates UIElement.label_text", "[Engine][ui_bindings][unit]") {
    UiBindingsFixture f;
    Entity w = f.make_widget("label", "old");
    f.bb.set<double>("test.id", static_cast<double>(w));

    auto r = f.lua.execute_string(R"(
        ui.set_label(engine.get_blackboard("test.id"), "Score: 42")
    )");
    REQUIRE(r.success);
    auto el = f.cs.get_component<UIElement>(w);
    REQUIRE(el.has_value());
    CHECK(el->get().label_text == "Score: 42");
}

TEST_CASE("ui.get_value returns UIState.value", "[Engine][ui_bindings][unit]") {
    UiBindingsFixture f;
    Entity w = f.make_widget("slider", "", 0.75f);
    f.bb.set<double>("test.id", static_cast<double>(w));

    auto r = f.lua.execute_string(R"(
        engine.set_blackboard("test.value", ui.get_value(engine.get_blackboard("test.id")))
    )");
    REQUIRE(r.success);
    CHECK(f.bb.get<double>("test.value") == Catch::Approx(0.75));
}

TEST_CASE("ui.get_value returns 0 for an entity with no UIState", "[Engine][ui_bindings][unit]") {
    UiBindingsFixture f;
    Entity e = f.em.create_entity();  // no UIState
    f.bb.set<double>("test.id", static_cast<double>(e));

    auto r = f.lua.execute_string(R"(
        engine.set_blackboard("test.value", ui.get_value(engine.get_blackboard("test.id")))
    )");
    REQUIRE(r.success);
    CHECK(f.bb.get<double>("test.value") == Catch::Approx(0.0));
}

TEST_CASE("ui.set_disabled sets UIState.disabled", "[Engine][ui_bindings][unit]") {
    UiBindingsFixture f;
    Entity w = f.make_widget("button", "Btn");
    f.bb.set<double>("test.id", static_cast<double>(w));

    auto r = f.lua.execute_string(R"(
        ui.set_disabled(engine.get_blackboard("test.id"), true)
    )");
    REQUIRE(r.success);
    auto st = f.cs.get_component<UIState>(w);
    REQUIRE(st.has_value());
    CHECK(st->get().disabled == true);
}

TEST_CASE("ui.push_screen sets ui.cmd.push", "[Engine][ui_bindings][unit]") {
    UiBindingsFixture f;
    auto r = f.lua.execute_string(R"( ui.push_screen("settings") )");
    REQUIRE(r.success);
    CHECK(f.bb.get<std::string>(ScreenStackSystem::CMD_PUSH) == "settings");
}

TEST_CASE("ui.push_screen with empty name is a no-op", "[Engine][ui_bindings][unit]") {
    UiBindingsFixture f;
    auto r = f.lua.execute_string(R"( ui.push_screen("") )");
    REQUIRE(r.success);
    CHECK_FALSE(f.bb.has(ScreenStackSystem::CMD_PUSH));
}

TEST_CASE("ui.pop_screen at base depth does not crash and queues no command",
          "[Engine][ui_bindings][unit]") {
    UiBindingsFixture f;
    // No stack -> depth 0 (<= 1): pop_screen logs a warning and is a no-op.
    auto r = f.lua.execute_string(R"( ui.pop_screen() )");
    REQUIRE(r.success);
    CHECK_FALSE(f.bb.has(ScreenStackSystem::CMD_POP));
}

TEST_CASE("ui.pop_screen above base sets ui.cmd.pop", "[Engine][ui_bindings][unit]") {
    UiBindingsFixture f;
    f.bb.set<std::vector<std::string>>(ScreenStackSystem::STACK_KEY,
        {ScreenStackSystem::BASE_SCREEN, "pause"});  // depth 2 > 1
    auto r = f.lua.execute_string(R"( ui.pop_screen() )");
    REQUIRE(r.success);
    CHECK(f.bb.get<bool>(ScreenStackSystem::CMD_POP) == true);
}

TEST_CASE("ui.widget_id round-trips a published id and returns nil for unknown",
          "[Engine][ui_bindings][unit]") {
    UiBindingsFixture f;
    f.bb.set<double>("ui.widget_id.score_label", 7.0);

    auto r = f.lua.execute_string(R"(
        local id = ui.widget_id("score_label")
        engine.set_blackboard("test.found", id)
        local missing = ui.widget_id("nope")
        engine.set_blackboard("test.missing_is_nil", missing == nil)
    )");
    REQUIRE(r.success);
    CHECK(f.bb.get<double>("test.found") == Catch::Approx(7.0));
    CHECK(f.bb.get<bool>("test.missing_is_nil") == true);
}
