/**
 * Unit tests for UISystem (ui_system.hpp/cpp) — Option-040 Phase 3.
 *
 * Deterministic, headless (no SDL window): each test constructs a LuaManager,
 * EntityManager, ComponentStorage, and Blackboard, builds an active screen plus
 * a widget (UIElement + UIState + ScreenMembership), drives synthetic mouse
 * state on the Blackboard, calls UISystem::update, and asserts UIState mutation
 * and/or the callback's observable Blackboard write.
 *
 * The engine.* bindings (engine.set_blackboard) are registered by UISystem
 * itself on the first update() and the live engine pointers are stored each
 * frame, so a Lua callback defined via execute_string can write a Blackboard
 * key that the test reads back.
 *
 * Coordinate mapping (R7): the UI pointer is in bottom-left-origin space,
 * derived from the SDL top-left pointer as ui_y = window_height - screen_y,
 * ui_x = screen_x. To place the UI pointer at (px, py), set
 * mouse.screen_x = px and mouse.screen_y = window_height - py.
 *
 * Requirements tested: R10.1–R10.6, plus R5.5 and R5.6.
 */

#include <catch2/catch_test_macros.hpp>

#include "engine/lua_manager.hpp"
#include "engine/ecs/systems/ui_system.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/components.hpp"

#include <string>

namespace {

constexpr int   kWindowHeight  = 600;
constexpr float kWindowHeightF = 600.0f;

// A fresh interaction environment per test — no cross-test bleed.
struct UITestEnv {
    LuaManager      lua;
    EntityManager   em;
    ComponentStorage cs;
    Blackboard      bb;
    UISystem        ui;

    UITestEnv() : ui(lua, kWindowHeight) {
        // Live window height (the system prefers this Blackboard key).
        bb.set<int>("window_height", kWindowHeight);
        // Default per-frame mouse edges to "no event".
        bb.set<bool>("mouse.down", false);
        bb.set<bool>("mouse.up", false);
    }

    // Add an active (or inactive) screen container.
    Entity add_screen(const std::string& name, bool active) {
        Entity s = em.create_entity();
        cs.add_component<UIScreen>(s, UIScreen{name, active});
        return s;
    }

    // Add a widget on the named screen with the given rect and callback name.
    Entity add_widget(const std::string& screen, const UIRect& rect,
                      const std::string& on_click_fn, bool disabled = false) {
        Entity w = em.create_entity();
        UIElement el;
        el.element_type = "button";
        el.rect         = rect;
        el.label_text   = "Btn";
        el.style_id     = "default_button";
        el.on_click_fn  = on_click_fn;
        el.z_order      = 10;
        cs.add_component<UIElement>(w, el);

        UIState st;
        st.disabled = disabled;
        cs.add_component<UIState>(w, st);

        cs.add_component<ScreenMembership>(w, ScreenMembership{screen});
        return w;
    }

    // Place the UI pointer at bottom-left-origin (px, py): invert Y to SDL.
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

// A widget rect used across tests: bottom-left (300,300), 200x50.
// Inside  UI point: (400, 325)  -> screen (400, 275)
// Outside UI point: (100, 100)  -> screen (100, 500)
constexpr UIRect kRect{300.0f, 300.0f, 200.0f, 50.0f};

}  // namespace

// -----------------------------------------------------------------------
// R10.1 — Pointer inside an enabled in-scope widget sets hovered = true.
// -----------------------------------------------------------------------
TEST_CASE("UISystem: pointer inside enabled widget sets hovered true", "[Engine][ui][unit]") {
    UITestEnv f;
    f.add_screen("main_menu", true);
    Entity w = f.add_widget("main_menu", kRect, "");

    f.set_pointer_ui(400.0f, 325.0f);  // inside
    f.set_edges(false, false);
    f.tick();

    CHECK(f.state_of(w).hovered == true);
}

// -----------------------------------------------------------------------
// R10.2 — Pointer outside an enabled in-scope widget sets hovered = false.
// -----------------------------------------------------------------------
TEST_CASE("UISystem: pointer outside enabled widget sets hovered false", "[Engine][ui][unit]") {
    UITestEnv f;
    f.add_screen("main_menu", true);
    Entity w = f.add_widget("main_menu", kRect, "");

    f.set_pointer_ui(100.0f, 100.0f);  // outside
    f.set_edges(false, false);
    f.tick();

    CHECK(f.state_of(w).hovered == false);
}

// -----------------------------------------------------------------------
// R10.3 — Down-inside then up-inside the SAME widget invokes the callback,
// verified by the callback's Blackboard write.
// -----------------------------------------------------------------------
TEST_CASE("UISystem: confirmed click fires the Lua callback", "[Engine][ui][unit]") {
    UITestEnv f;
    auto def = f.lua.execute_string(
        "function cb() engine.set_blackboard('ui.play_clicked', true) end");
    REQUIRE(def.success);

    f.add_screen("main_menu", true);
    Entity w = f.add_widget("main_menu", kRect, "cb");

    // Frame 1: button-down inside -> pressed.
    f.set_pointer_ui(400.0f, 325.0f);
    f.set_edges(true, false);
    f.tick();
    CHECK(f.state_of(w).pressed == true);

    // Frame 2: button-up inside the same widget -> confirmed click.
    f.set_pointer_ui(400.0f, 325.0f);
    f.set_edges(false, true);
    f.tick();

    CHECK(f.bb.get<bool>("ui.play_clicked") == true);
    CHECK(f.state_of(w).pressed == false);  // cleared on release
}

// -----------------------------------------------------------------------
// R10.4 — Up outside after down inside: callback NOT fired, pressed cleared.
// -----------------------------------------------------------------------
TEST_CASE("UISystem: up outside does not fire callback and clears pressed", "[Engine][ui][unit]") {
    UITestEnv f;
    auto def = f.lua.execute_string(
        "function cb() engine.set_blackboard('ui.play_clicked', true) end");
    REQUIRE(def.success);

    f.add_screen("main_menu", true);
    Entity w = f.add_widget("main_menu", kRect, "cb");

    // Frame 1: down inside -> pressed.
    f.set_pointer_ui(400.0f, 325.0f);
    f.set_edges(true, false);
    f.tick();
    CHECK(f.state_of(w).pressed == true);

    // Frame 2: move outside, release -> no confirmed click.
    f.set_pointer_ui(100.0f, 100.0f);
    f.set_edges(false, true);
    f.tick();

    CHECK(f.bb.get_or<bool>("ui.play_clicked", false) == false);
    CHECK(f.state_of(w).pressed == false);
}

// -----------------------------------------------------------------------
// R10.5 — Disabled widget is inert across a down/up inside its rect.
// -----------------------------------------------------------------------
TEST_CASE("UISystem: disabled widget ignores all input", "[Engine][ui][unit]") {
    UITestEnv f;
    auto def = f.lua.execute_string(
        "function cb() engine.set_blackboard('ui.play_clicked', true) end");
    REQUIRE(def.success);

    f.add_screen("main_menu", true);
    Entity w = f.add_widget("main_menu", kRect, "cb", /*disabled=*/true);

    // Frame 1: down inside.
    f.set_pointer_ui(400.0f, 325.0f);
    f.set_edges(true, false);
    f.tick();

    // Frame 2: up inside.
    f.set_pointer_ui(400.0f, 325.0f);
    f.set_edges(false, true);
    f.tick();

    const UIState& st = f.state_of(w);
    CHECK(st.hovered == false);
    CHECK(st.pressed == false);
    CHECK(f.bb.get_or<bool>("ui.play_clicked", false) == false);
}

// -----------------------------------------------------------------------
// R10.6 — Confirmed click on a widget with empty on_click_fn: no Lua call,
// no error, pressed cleared.
// -----------------------------------------------------------------------
TEST_CASE("UISystem: empty on_click_fn confirmed click is a safe no-op", "[Engine][ui][unit]") {
    UITestEnv f;
    f.add_screen("main_menu", true);
    Entity w = f.add_widget("main_menu", kRect, "");  // empty callback

    f.set_pointer_ui(400.0f, 325.0f);
    f.set_edges(true, false);
    REQUIRE_NOTHROW(f.tick());

    f.set_pointer_ui(400.0f, 325.0f);
    f.set_edges(false, true);
    REQUIRE_NOTHROW(f.tick());

    CHECK(f.state_of(w).pressed == false);
}

// -----------------------------------------------------------------------
// R5.5 — Non-empty on_click_fn naming a missing global: no call, no
// frame-terminating error; a second enabled widget with a valid callback
// still fires (the frame continues).
// -----------------------------------------------------------------------
TEST_CASE("UISystem: missing callback global is skipped and frame continues", "[Engine][ui][unit]") {
    UITestEnv f;
    auto def = f.lua.execute_string(
        "function cb() engine.set_blackboard('ui.play_clicked', true) end");
    REQUIRE(def.success);

    f.add_screen("main_menu", true);
    // Widget A: names a global that was never defined.
    Entity wa = f.add_widget("main_menu", kRect, "missing_global_fn");
    // Widget B (same rect, same screen): valid callback.
    Entity wb = f.add_widget("main_menu", kRect, "cb");

    // Down inside (both widgets share the rect).
    f.set_pointer_ui(400.0f, 325.0f);
    f.set_edges(true, false);
    REQUIRE_NOTHROW(f.tick());

    // Up inside -> A is a no-op (missing global), B fires.
    f.set_pointer_ui(400.0f, 325.0f);
    f.set_edges(false, true);
    REQUIRE_NOTHROW(f.tick());

    CHECK(f.bb.get<bool>("ui.play_clicked") == true);  // B fired -> frame continued
    CHECK(f.state_of(wa).pressed == false);
    CHECK(f.state_of(wb).pressed == false);
}

// -----------------------------------------------------------------------
// R5.6 — A callback that raises a Lua runtime error is contained; the frame
// does not terminate.
// -----------------------------------------------------------------------
TEST_CASE("UISystem: callback runtime error is contained", "[Engine][ui][unit]") {
    UITestEnv f;
    auto def = f.lua.execute_string("function boom() error('boom') end");
    REQUIRE(def.success);

    f.add_screen("main_menu", true);
    Entity w = f.add_widget("main_menu", kRect, "boom");

    f.set_pointer_ui(400.0f, 325.0f);
    f.set_edges(true, false);
    REQUIRE_NOTHROW(f.tick());

    f.set_pointer_ui(400.0f, 325.0f);
    f.set_edges(false, true);
    REQUIRE_NOTHROW(f.tick());  // error contained, frame continues

    CHECK(f.state_of(w).pressed == false);
}

// -----------------------------------------------------------------------
// Window-size independence: at a non-identity window the hit-test must use the
// design->window canvas transform, so the clickable area follows the rendered
// (scaled+centered) widget, not the raw design rect.
// -----------------------------------------------------------------------
TEST_CASE("UISystem: hit-test follows the canvas transform at a non-identity window",
          "[Engine][ui][unit]") {
    // Bespoke 980x660 environment (scale 1.1, offset (50, 0)).
    LuaManager      lua;
    EntityManager   em;
    ComponentStorage cs;
    Blackboard      bb;
    UISystem        ui(lua, 660);
    bb.set<int>("window_width", 980);
    bb.set<int>("window_height", 660);
    bb.set<bool>("mouse.down", false);
    bb.set<bool>("mouse.up", false);

    Entity screen = em.create_entity();
    cs.add_component<UIScreen>(screen, UIScreen{"main_menu", true});
    Entity w = em.create_entity();
    UIElement el;
    el.element_type = "button";
    el.rect = kRect;              // design-space {300,300,200,50}
    cs.add_component<UIElement>(w, el);
    cs.add_component<UIState>(w, UIState{});
    cs.add_component<ScreenMembership>(w, ScreenMembership{"main_menu"});

    // Transformed rect = {380,330,220,55} (x:[380,600], y:[330,385]).
    // Place the pointer at a point INSIDE the raw rect but OUTSIDE the transformed
    // rect (350,310): with the transform applied, this must NOT hover.
    auto place = [&](float px, float py) {
        bb.set<float>("mouse.screen_x", px);
        bb.set<float>("mouse.screen_y", 660.0f - py);  // window-height Y-invert
    };
    place(350.0f, 310.0f);
    ui.update(cs, em, bb);
    CHECK(cs.get_component<UIState>(w)->get().hovered == false);

    // Place at the transformed-rect center (490, 357.5): must hover.
    place(490.0f, 357.5f);
    ui.update(cs, em, bb);
    CHECK(cs.get_component<UIState>(w)->get().hovered == true);
}
