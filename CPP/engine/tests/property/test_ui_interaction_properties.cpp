/**
 * Property-based tests for the Option-040 Phase 3 button-interaction logic
 * (o-040-03-button-interaction).
 *
 * Implements the four correctness properties from the design "Correctness
 * Properties" section, one property-based TEST_CASE each:
 *   Property 1: point_in_rect is correct with inclusive bounds (pure helper)
 *   Property 2: Hover is a pure, idempotent, camera-independent function of the
 *               current pointer for enabled widgets; out-of-scope and
 *               membership-less widgets are left untouched
 *   Property 3: Confirmed-click fires the callback iff down-inside then
 *               up-inside the same enabled widget; release always clears pressed
 *   Property 4: Disabled widgets are inert for any input sequence
 *
 * Property 1 exercises the SDL-free pure helper point_in_rect directly (no
 * UISystem). Properties 2-4 drive UISystem::update headlessly: a fresh
 * LuaManager/EntityManager/ComponentStorage/Blackboard is constructed per
 * generated case, synthetic mouse state is written to the Blackboard, and the
 * Lua callback effect is observed through a Blackboard key the callback writes
 * via the existing engine.set_blackboard binding (no SDL, no window).
 *
 * Coordinate mapping (bottom-left origin): to place the UI pointer at y = py the
 * test sets mouse.screen_y = window_height - py (and mouse.screen_x = px), so
 * UISystem's to_ui_y(window_height, screen_y) recovers py.
 *
 * Feature: o-040-03-button-interaction
 * Testing Framework: Catch2 v3
 * All property tests bounded per the workspace property-test-bounds policy.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include "engine/ecs/systems/ui_system.hpp"
#include "engine/ecs/systems/ui_render_math.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/lua_manager.hpp"

#include <cmath>
#include <string>

// Configurable test iteration counts (MANDATORY — workspace property-test-bounds policy)
constexpr int NUM_OUTER_TESTS = 10;  // Number of different outer subjects (rects)
constexpr int NUM_INNER_TESTS = 5;   // Number of value variations per subject

// ---------------------------------------------------------------------------
// Helpers shared by the UISystem-driven properties.
// ---------------------------------------------------------------------------

// Create an active screen entity with the given name.
static Entity make_active_screen(ComponentStorage& storage, EntityManager& em,
                                 const std::string& name, bool active) {
    Entity e = em.create_entity();
    UIScreen scr{};
    scr.screen_name = name;
    scr.active = active;
    storage.add_component(e, scr);
    return e;
}

// Create a widget entity (UIElement + UIState + ScreenMembership).
static Entity make_widget(ComponentStorage& storage, EntityManager& em,
                          const UIRect& rect, const std::string& screen_name,
                          const std::string& on_click_fn, const UIState& initial,
                          bool with_membership) {
    Entity e = em.create_entity();
    UIElement el{};
    el.element_type = "button";
    el.rect = rect;
    el.on_click_fn = on_click_fn;
    storage.add_component(e, el);
    storage.add_component(e, initial);
    if (with_membership) {
        ScreenMembership m{};
        m.screen_name = screen_name;
        storage.add_component(e, m);
    }
    return e;
}

// Drive one frame of UISystem given a UI-space pointer (px, py), button edges,
// and a window height. Sets mouse.screen_y = window_height - py so the system's
// Y-inversion recovers py.
static void run_frame(UISystem& ui, ComponentStorage& storage, EntityManager& em,
                      Blackboard& bb, float px, float py, int window_height,
                      bool down, bool up) {
    bb.set<float>("mouse.screen_x", px);
    bb.set<float>("mouse.screen_y", static_cast<float>(window_height) - py);
    bb.set<int>("window_height", window_height);
    // Window-size independence: pin window_width so the canvas transform is the
    // identity at the design height (these properties test the hit-test/click
    // semantics, which are transform-invariant; the transform itself has its own
    // tests in test_ui_canvas_transform*). Callers pass window_height == design.
    bb.set<int>("window_width", static_cast<int>(UI_DESIGN_WIDTH));
    bb.set<bool>("mouse.down", down);
    bb.set<bool>("mouse.up", up);
    ui.update(storage, em, bb);
}

// ---------------------------------------------------------------------------
// Feature: o-040-03-button-interaction, Property 1: point_in_rect is correct
// with inclusive bounds.
//
// For any UIRect (random x, y; positive w, h) and any point classified relative
// to that rect, point_in_rect returns true for every interior point, for every
// point lying exactly on any of the four edges, and for all four corners, and
// returns false for every point strictly outside any edge.
//
// **Validates: Requirements 7.5, 3.1, 3.2**
// ---------------------------------------------------------------------------
TEST_CASE("Property 1: point_in_rect is correct with inclusive bounds",
          "[Engine][ui][property]") {

    SECTION("interior, edges, corners are inside; strictly-outside points are outside") {
        // x, y may be negative; w, h are strictly positive.
        auto pos = GENERATE(take(NUM_OUTER_TESTS, chunk(2, random(-500, 500))));
        auto dim = GENERATE(take(NUM_INNER_TESTS, chunk(2, random(1, 400))));

        UIRect rect{};
        rect.x = static_cast<float>(pos[0]);
        rect.y = static_cast<float>(pos[1]);
        rect.w = static_cast<float>(dim[0]);
        rect.h = static_cast<float>(dim[1]);

        const float left   = rect.x;
        const float right  = rect.x + rect.w;
        const float bottom = rect.y;
        const float top    = rect.y + rect.h;
        const float cx     = rect.x + rect.w / 2.0f;
        const float cy     = rect.y + rect.h / 2.0f;

        // Interior points (w, h >= 1 so the center is strictly interior).
        REQUIRE(point_in_rect(cx, cy, rect));
        REQUIRE(point_in_rect(left + rect.w / 4.0f, bottom + rect.h / 4.0f, rect));
        REQUIRE(point_in_rect(right - rect.w / 4.0f, top - rect.h / 4.0f, rect));

        // Four edge midpoints (inclusive bounds -> inside).
        REQUIRE(point_in_rect(left,  cy,     rect));  // left edge
        REQUIRE(point_in_rect(right, cy,     rect));  // right edge
        REQUIRE(point_in_rect(cx,    bottom, rect));  // bottom edge
        REQUIRE(point_in_rect(cx,    top,    rect));  // top edge

        // Four corners (inclusive -> inside).
        REQUIRE(point_in_rect(left,  bottom, rect));
        REQUIRE(point_in_rect(right, bottom, rect));
        REQUIRE(point_in_rect(left,  top,    rect));
        REQUIRE(point_in_rect(right, top,    rect));

        // Strictly outside each edge by one unit.
        REQUIRE_FALSE(point_in_rect(left - 1.0f,  cy,         rect));  // left of left
        REQUIRE_FALSE(point_in_rect(right + 1.0f, cy,         rect));  // right of right
        REQUIRE_FALSE(point_in_rect(cx,           bottom - 1.0f, rect));  // below bottom
        REQUIRE_FALSE(point_in_rect(cx,           top + 1.0f,    rect));  // above top
        // Far-outside corners.
        REQUIRE_FALSE(point_in_rect(left - 1.0f,  bottom - 1.0f, rect));
        REQUIRE_FALSE(point_in_rect(right + 1.0f, top + 1.0f,    rect));
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-03-button-interaction, Property 2: Hover is a pure, idempotent
// function of the current pointer for enabled widgets.
//
// For any enabled in-scope widget with a random UIRect, any SDL screen-pixel
// pointer, any prior UIState.hovered value, and any camera zoom/lookat
// Blackboard values, running UISystem::update sets UIState.hovered to exactly
// point_in_rect(screen_x, window_height - screen_y, rect) — independent of the
// prior hovered value and of the camera keys — and running update again leaves
// hovered unchanged. Widgets that are out of scope (membership names no active
// screen) or carry no ScreenMembership have their UIState left untouched.
//
// **Validates: Requirements 3.1, 3.2, 3.3, 3.4, 7.1, 7.2, 7.3, 2.2, 2.3, 2.4, 1.6**
// ---------------------------------------------------------------------------
TEST_CASE("Property 2: Hover is a pure, idempotent, camera-independent function of the pointer",
          "[Engine][ui][property]") {

    SECTION("hovered == inclusive hit test, independent of prior hover and camera; out-of-scope untouched") {
        auto rect_in = GENERATE(take(NUM_OUTER_TESTS, chunk(5, random(0, 600))));
        auto ptr_in  = GENERATE(take(NUM_INNER_TESTS, chunk(5, random(-100, 800))));

        UIRect rect{};
        rect.x = static_cast<float>(rect_in[0]);
        rect.y = static_cast<float>(rect_in[1]);
        rect.w = static_cast<float>(rect_in[2] + 1);   // >= 1
        rect.h = static_cast<float>(rect_in[3] + 1);   // >= 1
        const int window_height = static_cast<int>(UI_DESIGN_HEIGHT);  // design height -> identity transform (rect_in[4] now unused)

        const float sx    = static_cast<float>(ptr_in[0]);
        const float sy    = static_cast<float>(ptr_in[1]);
        const bool  prior = (std::abs(ptr_in[2]) % 2) == 1;
        const float cam_zoom = static_cast<float>(ptr_in[3]);
        const float cam_look = static_cast<float>(ptr_in[4]);

        LuaManager lua_manager;
        EntityManager em;
        ComponentStorage storage;
        Blackboard bb;

        // Active screen + an inactive screen.
        make_active_screen(storage, em, "main", true);
        make_active_screen(storage, em, "hidden", false);

        // Enabled in-scope widget with the random prior hover value.
        UIState init_a{};
        init_a.hovered = prior;
        Entity wa = make_widget(storage, em, rect, "main", "", init_a, /*with_membership=*/true);

        // Sentinel widget with NO ScreenMembership -> must be untouched (R2.3).
        UIState sentinel{};
        sentinel.hovered = true;
        sentinel.pressed = true;
        UIRect rect_b = rect;
        Entity wb = make_widget(storage, em, rect_b, "main", "", sentinel, /*with_membership=*/false);

        // Sentinel widget whose membership names only an inactive screen (R2.4).
        Entity wc = make_widget(storage, em, rect_b, "hidden", "", sentinel, /*with_membership=*/true);

        // Camera keys UISystem must ignore (R7.2).
        bb.set<float>("camera.zoom", cam_zoom);
        bb.set<float>("camera.lookat.x", cam_look);
        bb.set<float>("camera.lookat.y", cam_look);

        UISystem ui(lua_manager, window_height);

        // Expected hover is purely the inclusive hit test of the current pointer.
        const float ui_y = to_ui_y(static_cast<float>(window_height), sy);
        const bool expected = point_in_rect(sx, ui_y, rect);

        run_frame(ui, storage, em, bb, sx, ui_y, window_height, /*down=*/false, /*up=*/false);

        REQUIRE(storage.get_component<UIState>(wa).value().get().hovered == expected);

        // Out-of-scope widgets untouched (sentinel preserved).
        REQUIRE(storage.get_component<UIState>(wb).value().get().hovered == true);
        REQUIRE(storage.get_component<UIState>(wb).value().get().pressed == true);
        REQUIRE(storage.get_component<UIState>(wc).value().get().hovered == true);
        REQUIRE(storage.get_component<UIState>(wc).value().get().pressed == true);

        // Idempotent: a second identical update leaves hovered unchanged.
        run_frame(ui, storage, em, bb, sx, ui_y, window_height, /*down=*/false, /*up=*/false);
        REQUIRE(storage.get_component<UIState>(wa).value().get().hovered == expected);
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-03-button-interaction, Property 3: Confirmed-click fires the
// callback iff down-inside then up-inside the same enabled widget, and release
// always clears pressed.
//
// For any enabled in-scope widget with a random UIRect and a non-empty
// on_click_fn bound to a global Lua function that writes a Blackboard key, and
// any down-point and up-point each chosen inside or outside the rect: after a
// frame carrying the down edge then a frame carrying the up edge, the callback
// has fired (key written) iff down-inside AND up-inside; in every case pressed
// is false after the release frame; and when down was not inside, pressed is
// never set. A widget whose on_click_fn is empty never causes a Lua call and is
// cleared identically.
//
// **Validates: Requirements 4.1, 4.2, 4.3, 4.4, 5.1, 5.2, 5.3, 5.4**
// ---------------------------------------------------------------------------
TEST_CASE("Property 3: Confirmed-click fires callback iff down-inside then up-inside the same widget",
          "[Engine][ui][property]") {

    SECTION("callback fires iff down-inside && up-inside; release always clears pressed") {
        auto rvals = GENERATE(take(NUM_OUTER_TESTS, chunk(4, random(50, 400))));
        auto sel   = GENERATE(take(NUM_INNER_TESTS, chunk(2, random(0, 1))));

        // Distinct callback name + key per generated case.
        static int case_counter = 0;
        const int cid = case_counter++;
        const std::string fn_name = "cb_p3_" + std::to_string(cid);
        const std::string key     = "clicked_p3_" + std::to_string(cid);

        UIRect rect{};
        rect.x = static_cast<float>(rvals[0]);
        rect.y = static_cast<float>(rvals[1]);
        rect.w = static_cast<float>(rvals[2]);
        rect.h = static_cast<float>(rvals[3]);

        const bool down_inside = (sel[0] != 0);
        const bool up_inside   = (sel[1] != 0);
        const int window_height = static_cast<int>(UI_DESIGN_HEIGHT);  // design height -> identity transform

        // Interior point (w, h >= 50 -> center is strictly interior); outside
        // point is strictly left/below the rect.
        const float in_x  = rect.x + rect.w / 2.0f;
        const float in_y  = rect.y + rect.h / 2.0f;
        const float out_x = rect.x - 100.0f;
        const float out_y = rect.y - 100.0f;

        const float down_x = down_inside ? in_x : out_x;
        const float down_y = down_inside ? in_y : out_y;
        const float up_x   = up_inside   ? in_x : out_x;
        const float up_y   = up_inside   ? in_y : out_y;

        LuaManager lua_manager;
        EntityManager em;
        ComponentStorage storage;
        Blackboard bb;

        // Define the global callback that writes the proof key.
        auto def = lua_manager.execute_string(
            "function " + fn_name + "() engine.set_blackboard('" + key + "', true) end");
        REQUIRE(def.success);

        make_active_screen(storage, em, "main", true);

        // Widget A: callback bound. Widget B: empty on_click_fn (no-op variant).
        UIState init{};
        Entity wa = make_widget(storage, em, rect, "main", fn_name, init, /*with_membership=*/true);
        Entity wb = make_widget(storage, em, rect, "main", "", init, /*with_membership=*/true);

        UISystem ui(lua_manager, window_height);

        // Frame 1: button-down at the down-point.
        run_frame(ui, storage, em, bb, down_x, down_y, window_height, /*down=*/true, /*up=*/false);

        // pressed is set iff the down was inside (and never when down outside).
        REQUIRE(storage.get_component<UIState>(wa).value().get().pressed == down_inside);
        REQUIRE(storage.get_component<UIState>(wb).value().get().pressed == down_inside);

        // Frame 2: button-up at the up-point.
        run_frame(ui, storage, em, bb, up_x, up_y, window_height, /*down=*/false, /*up=*/true);

        // Callback fired iff down-inside AND up-inside.
        const bool key_written = bb.get_or<bool>(key, false);
        REQUIRE(key_written == (down_inside && up_inside));

        // pressed is always cleared after the release frame.
        REQUIRE(storage.get_component<UIState>(wa).value().get().pressed == false);
        REQUIRE(storage.get_component<UIState>(wb).value().get().pressed == false);
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-03-button-interaction, Property 4: Disabled widgets are inert
// for any input sequence.
//
// For any in-scope widget with UIState.disabled == true, a random UIRect, and
// any sequence of pointer positions and button edges (including down-inside
// followed by up-inside), after running UISystem::update over that sequence the
// widget's UIState.hovered and UIState.pressed remain false, its on_click_fn
// callback is never invoked (no Blackboard key written), and its UIState.value
// equals its initial value.
//
// **Validates: Requirements 6.1, 6.2, 6.3, 6.4**
// ---------------------------------------------------------------------------
TEST_CASE("Property 4: Disabled widgets are inert for any input sequence",
          "[Engine][ui][property]") {

    SECTION("disabled widget: hovered/pressed stay false, callback never fires, value unchanged") {
        auto rvals = GENERATE(take(NUM_OUTER_TESTS, chunk(4, random(50, 400))));
        auto seq   = GENERATE(take(NUM_INNER_TESTS, chunk(9, random(0, 1))));  // 3 random frames x (down, up, inside)

        static int case_counter = 0;
        const int cid = case_counter++;
        const std::string fn_name = "cb_p4_" + std::to_string(cid);
        const std::string key     = "clicked_p4_" + std::to_string(cid);

        UIRect rect{};
        rect.x = static_cast<float>(rvals[0]);
        rect.y = static_cast<float>(rvals[1]);
        rect.w = static_cast<float>(rvals[2]);
        rect.h = static_cast<float>(rvals[3]);

        const int window_height = static_cast<int>(UI_DESIGN_HEIGHT);  // design height -> identity transform
        const float in_x  = rect.x + rect.w / 2.0f;
        const float in_y  = rect.y + rect.h / 2.0f;
        const float out_x = rect.x - 100.0f;
        const float out_y = rect.y - 100.0f;

        // A known, non-default initial value to detect any mutation.
        const float init_value = static_cast<float>(rvals[0]) / 7.0f + 0.5f;

        LuaManager lua_manager;
        EntityManager em;
        ComponentStorage storage;
        Blackboard bb;

        auto def = lua_manager.execute_string(
            "function " + fn_name + "() engine.set_blackboard('" + key + "', true) end");
        REQUIRE(def.success);

        make_active_screen(storage, em, "main", true);

        UIState init{};
        init.disabled = true;
        init.hovered = false;
        init.pressed = false;
        init.value = init_value;
        Entity w = make_widget(storage, em, rect, "main", fn_name, init, /*with_membership=*/true);

        UISystem ui(lua_manager, window_height);

        // Explicit confirmed-click attempt: down inside, then up inside.
        run_frame(ui, storage, em, bb, in_x, in_y, window_height, /*down=*/true,  /*up=*/false);
        run_frame(ui, storage, em, bb, in_x, in_y, window_height, /*down=*/false, /*up=*/true);

        // Three randomized frames.
        for (int f = 0; f < 3; ++f) {
            const bool down   = (seq[f * 3 + 0] != 0);
            const bool up     = (seq[f * 3 + 1] != 0);
            const bool inside = (seq[f * 3 + 2] != 0);
            const float px = inside ? in_x : out_x;
            const float py = inside ? in_y : out_y;
            run_frame(ui, storage, em, bb, px, py, window_height, down, up);
        }

        // Bind the optional first: chaining .value().get() off the returned
        // temporary trips -Wdangling-reference, even though the reference_wrapper
        // points into storage and outlives it.
        auto st_opt = storage.get_component<UIState>(w);
        REQUIRE(st_opt.has_value());
        const UIState& st = st_opt->get();
        REQUIRE(st.hovered == false);
        REQUIRE(st.pressed == false);
        REQUIRE(st.value == init_value);
        REQUIRE(bb.get_or<bool>(key, false) == false);  // callback never invoked
    }
}
