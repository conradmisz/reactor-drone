/**
 * Unit tests for slider drag + checkbox toggle in UISystem — Option-040 Phase 6.
 *
 * Drives UISystem::update headlessly with synthetic mouse state and asserts
 * UIState.value mutation: a held slider tracks the pointer x; a confirmed click
 * on a checkbox flips its value; disabled widgets are inert. Mirrors the pointer
 * mapping in test_ui_system.cpp (ui_y = window_height - screen_y; ui_x = screen_x).
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "engine/lua_manager.hpp"
#include "engine/ecs/systems/ui_system.hpp"
#include "engine/ecs/systems/ui_render_math.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/components.hpp"

#include <string>

namespace {

constexpr int   kWindowHeight  = 600;
constexpr float kWindowHeightF = 600.0f;
constexpr float kKnobW         = 16.0f;  // must match UISystem's UI_SLIDER_KNOB_W

struct ValueEnv {
    LuaManager       lua;
    EntityManager    em;
    ComponentStorage cs;
    Blackboard       bb;
    UISystem         ui;

    ValueEnv() : ui(lua, kWindowHeight) {
        bb.set<int>("window_height", kWindowHeight);
        bb.set<bool>("mouse.down", false);
        bb.set<bool>("mouse.up", false);
    }

    Entity add_screen(const std::string& name, bool active) {
        Entity s = em.create_entity();
        cs.add_component<UIScreen>(s, UIScreen{name, active});
        return s;
    }

    Entity add_widget(const std::string& screen, const std::string& type,
                      const UIRect& rect, float value, bool disabled = false) {
        Entity w = em.create_entity();
        UIElement el;
        el.element_type = type;
        el.rect = rect;
        el.z_order = 10;
        cs.add_component<UIElement>(w, el);
        UIState st;
        st.value = value;
        st.disabled = disabled;
        cs.add_component<UIState>(w, st);
        cs.add_component<ScreenMembership>(w, ScreenMembership{screen});
        return w;
    }

    void set_pointer_ui(float px, float py) {
        bb.set<float>("mouse.screen_x", px);
        bb.set<float>("mouse.screen_y", kWindowHeightF - py);
    }
    void set_edges(bool down, bool up) {
        bb.set<bool>("mouse.down", down);
        bb.set<bool>("mouse.up", up);
    }
    void tick() { ui.update(cs, em, bb); }
    const UIState& state_of(Entity w) {
        auto st = cs.get_component<UIState>(w);
        REQUIRE(st.has_value());
        return st->get();
    }
};

constexpr UIRect kSlider{300.0f, 300.0f, 200.0f, 24.0f};
constexpr UIRect kCheckbox{300.0f, 300.0f, 24.0f, 24.0f};

}  // namespace

TEST_CASE("UISystem: held slider tracks the pointer x", "[Engine][ui_value][unit]") {
    ValueEnv f;
    f.add_screen("settings", true);
    Entity s = f.add_widget("settings", "slider", kSlider, 0.5f);

    // Mouse-down inside the slider at ui_x = 400 -> pressed, value tracks pointer.
    f.set_pointer_ui(400.0f, 312.0f);  // inside (y within [300,324])
    f.set_edges(true, false);
    f.tick();

    float expected = slider_value_from_pointer(kSlider, 400.0f, kKnobW);
    CHECK(f.state_of(s).value == Catch::Approx(expected));
    CHECK(f.state_of(s).value == Catch::Approx(0.5));  // 400 is the track center
}

TEST_CASE("UISystem: dragging a held slider updates the value to a new pointer x",
          "[Engine][ui_value][unit]") {
    ValueEnv f;
    f.add_screen("settings", true);
    Entity s = f.add_widget("settings", "slider", kSlider, 0.5f);

    // Press inside.
    f.set_pointer_ui(400.0f, 312.0f);
    f.set_edges(true, false);
    f.tick();

    // Drag toward the right limit while still held (no new down edge needed).
    f.set_pointer_ui(492.0f, 312.0f);  // right usable limit -> value 1.0
    f.set_edges(false, false);
    f.tick();

    CHECK(f.state_of(s).value == Catch::Approx(1.0));
}

TEST_CASE("UISystem: confirmed click toggles a checkbox value", "[Engine][ui_value][unit]") {
    ValueEnv f;
    f.add_screen("settings", true);
    Entity c = f.add_widget("settings", "checkbox", kCheckbox, 0.0f);

    // Press inside.
    f.set_pointer_ui(310.0f, 310.0f);
    f.set_edges(true, false);
    f.tick();
    CHECK(f.state_of(c).value == Catch::Approx(0.0));  // not toggled until release

    // Release inside -> confirmed click -> toggle 0 -> 1.
    f.set_edges(false, true);
    f.tick();
    CHECK(f.state_of(c).value == Catch::Approx(1.0));
}

TEST_CASE("UISystem: a second confirmed click toggles the checkbox back",
          "[Engine][ui_value][unit]") {
    ValueEnv f;
    f.add_screen("settings", true);
    Entity c = f.add_widget("settings", "checkbox", kCheckbox, 1.0f);

    f.set_pointer_ui(310.0f, 310.0f);
    f.set_edges(true, false);
    f.tick();
    f.set_edges(false, true);
    f.tick();
    CHECK(f.state_of(c).value == Catch::Approx(0.0));  // 1 -> 0
}

TEST_CASE("UISystem: a disabled slider does not change value", "[Engine][ui_value][unit]") {
    ValueEnv f;
    f.add_screen("settings", true);
    Entity s = f.add_widget("settings", "slider", kSlider, 0.5f, /*disabled=*/true);

    f.set_pointer_ui(492.0f, 312.0f);
    f.set_edges(true, false);
    f.tick();
    CHECK(f.state_of(s).value == Catch::Approx(0.5));  // unchanged
}

TEST_CASE("UISystem: a disabled checkbox does not toggle", "[Engine][ui_value][unit]") {
    ValueEnv f;
    f.add_screen("settings", true);
    Entity c = f.add_widget("settings", "checkbox", kCheckbox, 0.0f, /*disabled=*/true);

    f.set_pointer_ui(310.0f, 310.0f);
    f.set_edges(true, false);
    f.tick();
    f.set_edges(false, true);
    f.tick();
    CHECK(f.state_of(c).value == Catch::Approx(0.0));  // unchanged
}
