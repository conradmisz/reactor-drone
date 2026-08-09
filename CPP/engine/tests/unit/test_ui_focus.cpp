/**
 * Unit tests for keyboard focus navigation in UISystem — Option-040 Phase 7.
 *
 * Drives UISystem::update headlessly with the ui.tab_pressed / ui.enter_pressed
 * Blackboard edges (the same ones InputSystem publishes) and asserts the focus
 * model: Tab cycles UIState.focused across the enabled interactive widgets of
 * the active screen in entity-id order (wrapping); labels, panels, and disabled
 * widgets are skipped; Enter activates the focused widget (checkbox toggle +
 * on_click_fn callback); leaving all screens inactive drops focus.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "engine/lua_manager.hpp"
#include "engine/ecs/systems/ui_system.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/components.hpp"

#include <string>

namespace {

constexpr int kWindowHeight = 600;

struct FocusEnv {
    LuaManager       lua;
    EntityManager    em;
    ComponentStorage cs;
    Blackboard       bb;
    UISystem         ui;

    FocusEnv() : ui(lua, kWindowHeight) {
        bb.set<int>("window_height", kWindowHeight);
        // Mouse defaults: off-screen, no edges (focus tests are keyboard-only).
        bb.set<bool>("mouse.down", false);
        bb.set<bool>("mouse.up", false);
        bb.set<float>("mouse.screen_x", -1e9f);
        bb.set<float>("mouse.screen_y", -1e9f);
        bb.set<bool>("ui.tab_pressed", false);
        bb.set<bool>("ui.enter_pressed", false);
    }

    Entity add_screen(const std::string& name, bool active) {
        Entity s = em.create_entity();
        cs.add_component<UIScreen>(s, UIScreen{name, active});
        return s;
    }

    Entity add_widget(const std::string& screen, const std::string& type,
                      const std::string& on_click_fn = "", bool disabled = false,
                      float value = 0.0f) {
        Entity w = em.create_entity();
        UIElement el;
        el.element_type = type;
        el.rect = UIRect{0.0f, 0.0f, 100.0f, 20.0f};
        el.on_click_fn = on_click_fn;
        el.z_order = 10;
        cs.add_component<UIElement>(w, el);
        UIState st;
        st.disabled = disabled;
        st.value = value;
        cs.add_component<UIState>(w, st);
        cs.add_component<ScreenMembership>(w, ScreenMembership{screen});
        return w;
    }

    void tick() { ui.update(cs, em, bb); }

    // One Tab press: set the edge, tick (advance + reconcile), clear the edge.
    void press_tab() {
        bb.set<bool>("ui.tab_pressed", true);
        tick();
        bb.set<bool>("ui.tab_pressed", false);
    }

    void press_enter() {
        bb.set<bool>("ui.enter_pressed", true);
        tick();
        bb.set<bool>("ui.enter_pressed", false);
    }

    bool focused(Entity w) {
        auto st = cs.get_component<UIState>(w);
        REQUIRE(st.has_value());
        return st->get().focused;
    }
    float value_of(Entity w) {
        auto st = cs.get_component<UIState>(w);
        REQUIRE(st.has_value());
        return st->get().value;
    }
    void set_screen_active(Entity screen, bool active) {
        auto sc = cs.get_component<UIScreen>(screen);
        REQUIRE(sc.has_value());
        sc->get().active = active;
    }
};

}  // namespace

TEST_CASE("UISystem: Tab cycles focus across interactive widgets in id order",
          "[Engine][ui_focus][unit]") {
    FocusEnv f;
    f.add_screen("menu", true);
    Entity label  = f.add_widget("menu", "label");      // not focusable
    Entity btn    = f.add_widget("menu", "button");
    Entity slider = f.add_widget("menu", "slider");
    Entity chk    = f.add_widget("menu", "checkbox");

    // No focus before any Tab.
    f.tick();
    CHECK_FALSE(f.focused(btn));

    f.press_tab();   // -> first focusable (button)
    CHECK(f.focused(btn));
    CHECK_FALSE(f.focused(slider));
    CHECK_FALSE(f.focused(label));

    f.press_tab();   // -> slider
    CHECK(f.focused(slider));
    CHECK_FALSE(f.focused(btn));

    f.press_tab();   // -> checkbox
    CHECK(f.focused(chk));
    CHECK_FALSE(f.focused(slider));

    f.press_tab();   // wraps back to button
    CHECK(f.focused(btn));
    CHECK_FALSE(f.focused(chk));
}

TEST_CASE("UISystem: Tab skips labels and disabled widgets",
          "[Engine][ui_focus][unit]") {
    FocusEnv f;
    f.add_screen("menu", true);
    Entity label    = f.add_widget("menu", "label");
    Entity disabled = f.add_widget("menu", "button", "", /*disabled=*/true);
    Entity btn      = f.add_widget("menu", "button");

    f.press_tab();
    CHECK(f.focused(btn));
    CHECK_FALSE(f.focused(label));
    CHECK_FALSE(f.focused(disabled));

    f.press_tab();   // only one focusable -> stays on the button
    CHECK(f.focused(btn));
}

TEST_CASE("UISystem: exactly one widget is focused at a time",
          "[Engine][ui_focus][unit]") {
    FocusEnv f;
    f.add_screen("menu", true);
    Entity b1 = f.add_widget("menu", "button");
    Entity b2 = f.add_widget("menu", "button");
    Entity b3 = f.add_widget("menu", "button");

    f.press_tab();
    f.press_tab();   // focus on b2
    int count = (f.focused(b1) ? 1 : 0) + (f.focused(b2) ? 1 : 0) + (f.focused(b3) ? 1 : 0);
    CHECK(count == 1);
    CHECK(f.focused(b2));
}

TEST_CASE("UISystem: Enter activates the focused checkbox (toggle + callback)",
          "[Engine][ui_focus][unit]") {
    FocusEnv f;
    f.add_screen("menu", true);
    Entity chk = f.add_widget("menu", "checkbox", "on_chk", false, 0.0f);

    // Define the callback after the first tick registers engine.* bindings.
    f.tick();
    auto r = f.lua.execute_string("function on_chk() engine.set_blackboard('chk.fired', true) end");
    REQUIRE(r.success);

    f.press_tab();             // focus the checkbox
    CHECK(f.focused(chk));

    f.press_enter();           // activate: toggle 0 -> 1 and fire on_chk
    CHECK(f.value_of(chk) == Catch::Approx(1.0));
    CHECK(f.bb.get<bool>("chk.fired") == true);
}

TEST_CASE("UISystem: Enter on a focused button fires its callback",
          "[Engine][ui_focus][unit]") {
    FocusEnv f;
    f.add_screen("menu", true);
    f.add_widget("menu", "button", "on_btn");

    f.tick();
    auto r = f.lua.execute_string("function on_btn() engine.set_blackboard('btn.fired', true) end");
    REQUIRE(r.success);

    f.press_tab();
    f.press_enter();
    CHECK(f.bb.get<bool>("btn.fired") == true);
}

TEST_CASE("UISystem: leaving all screens inactive drops focus",
          "[Engine][ui_focus][unit]") {
    FocusEnv f;
    Entity screen = f.add_screen("menu", true);
    Entity btn = f.add_widget("menu", "button");

    f.press_tab();
    CHECK(f.focused(btn));

    // Deactivate the screen -> UISystem returns early and clears its internal
    // focus state (has_focus_). The widget's stale focused flag is invisible
    // while no screen renders.
    f.set_screen_active(screen, false);
    f.tick();

    // Reactivate -> the per-widget reconcile now runs with no focus, so the
    // button is no longer focused (focus did not survive the inactive period).
    f.set_screen_active(screen, true);
    f.tick();
    CHECK_FALSE(f.focused(btn));

    // And Tab starts cleanly from the first focusable again.
    f.press_tab();
    CHECK(f.focused(btn));
}
