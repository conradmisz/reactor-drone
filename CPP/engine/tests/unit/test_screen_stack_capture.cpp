/**
 * Unit tests for the modal-capture flag — Option-040 Phase 4 (R13.3 / R13.4).
 *
 * These tests drive ScreenStackSystem and UISystem TOGETHER, headless (no SDL
 * window), to verify the end-to-end contract that pushing a screen onto the
 * stack makes UISystem publish ui_captured_input = true (depth > 1 -> modal),
 * and popping back to the base depth clears it (depth 1 -> not modal).
 *
 * UISystem publishes ui_captured_input at the very top of update() — BEFORE the
 * "no active screen" early return — so the flag is refreshed every frame from
 * the live stack depth regardless of whether any widget is hit-tested.
 *
 * Headless construction mirrors the Phase 3 test_ui_system.cpp UITestEnv
 * pattern: a fresh LuaManager / EntityManager / ComponentStorage / Blackboard /
 * ScreenStackSystem / UISystem per test, with window_height = 600 and the mouse
 * parked far off-screen with no button edges so UISystem performs no spurious
 * interaction. The flag publication happens independent of that mouse state.
 *
 * Requirements tested: 13.3, 13.4, 6.1, 6.2
 */

#include <catch2/catch_test_macros.hpp>

#include "engine/lua_manager.hpp"
#include "engine/ecs/systems/ui_system.hpp"
#include "engine/ecs/systems/screen_stack_system.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/components.hpp"

#include <string>

namespace {

constexpr int kWindowHeight = 600;

// A fresh capture-test environment per test — no cross-test bleed. Constructs
// LuaManager + UISystem headless (no window) exactly as the Phase 3 UITestEnv,
// and adds a ScreenStackSystem driving the same ComponentStorage / Blackboard.
struct CaptureTestEnv {
    LuaManager        lua;
    EntityManager     em;
    ComponentStorage  cs;
    Blackboard        bb;
    ScreenStackSystem sss;
    UISystem          ui;

    CaptureTestEnv() : ui(lua, kWindowHeight) {
        // Live window height (UISystem prefers this Blackboard key for Y-invert).
        bb.set<int>("window_height", kWindowHeight);
        // Park the pointer far off-screen with no button edges so UISystem does
        // no spurious hover/press/click. The capture flag is published regardless.
        bb.set<float>("mouse.screen_x", -1.0e9f);
        bb.set<float>("mouse.screen_y", -1.0e9f);
        bb.set<bool>("mouse.down", false);
        bb.set<bool>("mouse.up", false);
    }

    // Add a UIScreen entity (the system reconciles the active flag, so the
    // starting value here is unimportant).
    Entity add_screen(const std::string& name, bool active = false) {
        Entity s = em.create_entity();
        cs.add_component<UIScreen>(s, UIScreen{name, active});
        return s;
    }

    void tick() { ui.update(cs, em, bb); }

    bool captured() { return bb.get_or<bool>("ui_captured_input", false); }
};

}  // namespace

// ---------------------------------------------------------------------------
// R13.3 — pushing "pause" (depth 2 -> modal) makes UISystem publish
// ui_captured_input = true.
// ---------------------------------------------------------------------------
TEST_CASE("UISystem: pushing a screen sets ui_captured_input true",
          "[Engine][screen_stack][unit]") {
    CaptureTestEnv f;
    f.add_screen("pause");

    f.sss.initialize(f.bb, f.cs);          // stack = ["gameplay"], depth 1
    f.sss.push_screen("pause", f.bb, f.cs); // stack = ["gameplay", "pause"], depth 2 (modal)

    f.tick();                               // UISystem publishes the flag

    REQUIRE(f.captured() == true);
}

// ---------------------------------------------------------------------------
// R13.4 — popping back to the base depth (depth 1 -> not modal) makes UISystem
// publish ui_captured_input = false.
// ---------------------------------------------------------------------------
TEST_CASE("UISystem: popping back to base depth clears ui_captured_input",
          "[Engine][screen_stack][unit]") {
    CaptureTestEnv f;
    f.add_screen("pause");

    // From the pushed-pause modal state...
    f.sss.initialize(f.bb, f.cs);
    f.sss.push_screen("pause", f.bb, f.cs);
    f.tick();
    REQUIRE(f.captured() == true);          // precondition: modal

    // ...pop back to depth 1 and refresh the flag.
    f.sss.pop_screen(f.bb, f.cs);           // stack = ["gameplay"], depth 1 (not modal)
    f.tick();

    REQUIRE(f.captured() == false);
}
