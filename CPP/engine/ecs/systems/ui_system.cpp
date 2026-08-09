/**
 * UISystem implementation
 *
 * Per-frame mouse interaction for active-screen widgets. Runs once per frame
 * after InputSystem (so the current frame's pointer and button edges are on the
 * Blackboard) and before any game system that consumes the mouse, and before
 * the UIRenderSystem reads UIState.
 *
 * This is a thin orchestration shell: every input-varying geometry decision
 * lives in the pure helpers in ui_render_math.hpp (to_ui_y, point_in_rect),
 * which the unit and property tests exercise directly without a window. update()
 * reads the Blackboard, calls those helpers, mutates UIState, and invokes the
 * widget's named global Lua callback under lua_pcall with error containment.
 *
 * Bottom-left origin is preserved: the SDL top-left pointer is Y-inverted via
 * to_ui_y(window_height, screen_y) with NO camera/zoom/lookat transform, so the
 * pointer and the widget rect are compared in one consistent bottom-left space.
 *
 * The engine.* bindings are registered once on the Lua state and the engine
 * pointers are stored every frame (mirroring ScriptSystem::update) so that a
 * callback such as on_play_click can call engine.set_blackboard against the live
 * Blackboard.
 *
 * Added in Phase 3 (o-040-03-button-interaction).
 */

#include "engine/ecs/systems/ui_system.hpp"
#include "engine/ecs/systems/ui_render_math.hpp"
#include "engine/ecs/systems/screen_stack_system.hpp"
#include "engine/ui_focus_math.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/components.hpp"
#include "engine/lua_manager.hpp"
#include "engine/lua_bindings.hpp"

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

// Phase 6 (o-040-06-lua-screens): slider knob width, shared with the
// UIRenderSystem render constant so the dragged value tracks the rendered knob.
namespace {
constexpr float UI_SLIDER_KNOB_W = 16.0f;

// Phase 7: only interactive widget kinds receive keyboard focus; labels and
// panels are inert decoration and are skipped by Tab navigation.
bool is_focusable_type(const std::string& t) {
    return t == "button" || t == "slider" || t == "checkbox";
}
}

UISystem::UISystem(LuaManager& lua_manager, int window_height)
    : lua_manager_(lua_manager), window_height_(window_height) {
}

void UISystem::update(ComponentStorage& storage,
                      EntityManager& entity_manager,
                      Blackboard& blackboard) {
    // Phase 4 (R6): publish the modal-capture flag exactly once per frame from
    // the current stack depth (depth>1 means a modal screen is active above the
    // base). Set unconditionally at the very top of update — BEFORE any early
    // return — so the flag is always refreshed each frame (R6.3, R15.6).
    blackboard.set<bool>("ui_captured_input", ScreenStackSystem::is_modal(blackboard));

    lua_State* L = lua_manager_.state();

    // Register engine.* bindings once on this state; store the live engine
    // pointers every frame so engine.set_blackboard sees this Blackboard. This
    // mirrors ScriptSystem::update exactly. (R5.2, R9.4)
    if (!bindings_registered_) {
        register_bindings(L);
        bindings_registered_ = true;
    }
    store_engine_pointers(L, &storage, &entity_manager, &blackboard);

    // 1. Pointer in bottom-left UI space (R7). Missing keys default to far
    //    off-screen / no edge, so a frame with no mouse data is a safe no-op.
    float sx = blackboard.get_or<float>("mouse.screen_x", -1e9f);
    float sy = blackboard.get_or<float>("mouse.screen_y", -1e9f);
    int   ww = blackboard.get_or<int>("window_width", static_cast<int>(UI_DESIGN_WIDTH));
    int   wh = blackboard.get_or<int>("window_height", window_height_);
    float ui_x = sx;                                            // R7.1 (X identity)
    float ui_y = to_ui_y(static_cast<float>(wh), sy);           // R7.1 (Y invert), R7.2 (no camera)

    // Window-size independence: the same design->window canvas transform the
    // renderer uses. Widget rects are mapped to window space before hit-testing,
    // so the clickable area always matches what is drawn. (matches UIRenderSystem)
    const UICanvasTransform xform =
        ui_canvas_transform(static_cast<float>(ww), static_cast<float>(wh));

    bool mouse_down = blackboard.get_or<bool>("mouse.down", false);
    bool mouse_up   = blackboard.get_or<bool>("mouse.up", false);

    // 2. Active-screen set — same rule as Phase 2 UIRenderSystem. (R2.1)
    std::unordered_set<std::string> active_screens;
    for (Entity screen_entity : storage.entities_with_component<UIScreen>()) {
        auto screen_opt = storage.get_component<UIScreen>(screen_entity);
        if (!screen_opt.has_value()) continue;
        const UIScreen& screen = screen_opt->get();
        if (screen.active) {
            active_screens.insert(screen.screen_name);
        }
    }

    // No active screen -> no UIState change, no error. (R1.6)
    if (active_screens.empty()) {
        // Phase 7: with nothing on screen, drop keyboard focus so a stale id
        // never drives Enter or the focus border on the next active frame.
        has_focus_ = false;
        return;
    }

    // Scope interaction to the TOP of the screen stack: a pushed (modal) screen
    // takes all hover/press/click/focus; lower screens stay active (still drawn)
    // but are frozen. Fallback: when there is no screen_stack on the Blackboard
    // (UISystem unit tests drive UIScreen.active directly), use the full active
    // set so single-screen tests are unaffected.
    std::unordered_set<std::string> interactive;
    {
        std::vector<std::string> stack = ScreenStackSystem::get_stack(blackboard);
        if (!stack.empty()) {
            interactive.insert(stack.back());
        } else {
            interactive = active_screens;
        }
    }

    // Phase 7: keyboard focus. Build the ordered focusable list for the active
    // screen(s): in-scope, enabled, interactive widgets, sorted by entity id
    // (== JSON layout order). Tab advances focus through this list; Enter (below)
    // activates the focused widget.
    std::vector<Entity> focusable;
    for (Entity entity : storage.entities_with_component<UIElement>()) {
        if (!storage.has_component<ScreenMembership>(entity)) continue;
        auto m = storage.get_component<ScreenMembership>(entity);
        if (!m.has_value()) continue;
        if (interactive.find(m->get().screen_name) == interactive.end()) continue;
        if (!storage.has_component<UIState>(entity)) continue;
        auto s = storage.get_component<UIState>(entity);
        if (!s.has_value() || s->get().disabled) continue;
        auto e = storage.get_component<UIElement>(entity);
        if (!e.has_value() || !is_focusable_type(e->get().element_type)) continue;
        focusable.push_back(entity);
    }
    std::sort(focusable.begin(), focusable.end());

    // Current focus index within the list (or -1 when nothing is focused or the
    // previously-focused widget left the list — screen change, disable, removal).
    int current_index = -1;
    if (has_focus_) {
        for (size_t i = 0; i < focusable.size(); ++i) {
            if (focusable[i] == focused_entity_) { current_index = static_cast<int>(i); break; }
        }
        if (current_index < 0) has_focus_ = false;  // stale focus -> drop it
    }

    bool tab_pressed   = blackboard.get_or<bool>("ui.tab_pressed", false);
    bool enter_pressed = blackboard.get_or<bool>("ui.enter_pressed", false);

    if (tab_pressed) {
        int next = next_focus_index(static_cast<int>(focusable.size()),
                                    current_index, /*forward=*/true);
        if (next >= 0) {
            has_focus_ = true;
            focused_entity_ = focusable[static_cast<size_t>(next)];
        } else {
            has_focus_ = false;  // nothing focusable
        }
    }

    // 3. Process exactly the in-scope widgets. (R2)
    for (Entity entity : storage.entities_with_component<UIElement>()) {
        // No membership -> not hit-tested, UIState untouched. (R2.3)
        if (!storage.has_component<ScreenMembership>(entity)) continue;
        auto membership_opt = storage.get_component<ScreenMembership>(entity);
        if (!membership_opt.has_value()) continue;
        const ScreenMembership& membership = membership_opt->get();

        // Only the top-of-stack (interactive) screen's widgets react; lower
        // screens stay drawn but frozen. (R2.4)
        if (interactive.find(membership.screen_name) == interactive.end()) {
            continue;
        }

        // Nothing to mutate without a UIState.
        if (!storage.has_component<UIState>(entity)) continue;
        auto state_opt = storage.get_component<UIState>(entity);
        if (!state_opt.has_value()) continue;
        UIState& st = state_opt->get();

        auto element_opt = storage.get_component<UIElement>(entity);
        if (!element_opt.has_value()) continue;
        const UIElement& el = element_opt->get();

        // Phase 7: reconcile keyboard focus for EVERY in-scope widget (before the
        // disabled early-out, so a widget that became disabled while focused has
        // its focus flag cleared too). UISystem is the single writer of focused.
        st.focused = (has_focus_ && entity == focused_entity_);

        // Disabled widgets are inert: no hover, no press, no callback, and
        // value left untouched. (R6.1, R6.2, R6.3, R6.4)
        if (st.disabled) continue;

        // Map the widget rect from the design canvas to window space so the
        // clickable area matches what UIRenderSystem draws at this window size.
        const UIRect wrect = ui_apply_transform(xform, el.rect);

        bool inside = point_in_rect(ui_x, ui_y, wrect);   // R7.3, R7.5

        // Hover is a PURE function of the current pointer. (R3.1, R3.2, R3.4)
        st.hovered = inside;

        // Press on down-inside (R4.1); a down NOT inside does not set it (R4.2).
        if (mouse_down && inside) {
            st.pressed = true;
        }

        // Phase 6: while a slider is held, its value tracks the pointer x (drag),
        // using the same knob width the renderer uses so the knob follows the
        // cursor. Pure inverse of slider_knob_center_x; clamped to [0,1].
        if (el.element_type == "slider" && st.pressed) {
            st.value = slider_value_from_pointer(wrect, ui_x, UI_SLIDER_KNOB_W * xform.scale);
        }

        // Release handling. (R4.3, R5)
        if (mouse_up) {
            // Confirmed click: press originated here and release lands inside.
            // (R5.1, R5.2)
            if (st.pressed && inside) {
                // Phase 6: a checkbox toggles its value on a confirmed click,
                // before any callback fires.
                if (el.element_type == "checkbox") {
                    st.value = (st.value != 0.0f) ? 0.0f : 1.0f;
                }
                // Existing dispatch — fires for any widget with a non-empty
                // callback (sliders and checkboxes included), after the final
                // drag/toggle value is set. (R5.2)
                if (!el.on_click_fn.empty()) {
                    publish_click(blackboard, el.on_click_fn);
                    invoke_callback(el.on_click_fn);
                }
            }
            // Empty on_click_fn -> no Lua call, treated as a no-op. (R5.4)
            // Always clear pressed on release. (R4.3, R5.3)
            st.pressed = false;
        }

        // Phase 7: Enter activates the focused widget — the keyboard equivalent
        // of a confirmed click. Same action as the mouse path: a checkbox toggles
        // its value, then a non-empty on_click_fn fires.
        if (enter_pressed && st.focused) {
            if (el.element_type == "checkbox") {
                st.value = (st.value != 0.0f) ? 0.0f : 1.0f;
            }
            if (!el.on_click_fn.empty()) {
                publish_click(blackboard, el.on_click_fn);
                invoke_callback(el.on_click_fn);
            }
        }
    }
}

void UISystem::publish_click(Blackboard& blackboard, const std::string& fn_name) {
    // v2 addition. A confirmed click is published on the Blackboard as well as
    // dispatched to Lua, so a game that carries no Lua layer at all can still
    // consume its own menus by reading UI_CLICK_KEY. Games that DO script their
    // menus are unaffected — the Lua call still happens, right after this.
    //
    // Single-slot on purpose: two widgets cannot confirm a click in one frame
    // (a press and its release both have to land inside the same rect), and the
    // consumer clears the key. Last writer would win if that ever changed.
    blackboard.set<std::string>(UI_CLICK_KEY, fn_name);
}

void UISystem::invoke_callback(const std::string& fn_name) {
    lua_State* L = lua_manager_.state();
    lua_getglobal(L, fn_name.c_str());          // push the global (R5.2 resolve)
    if (!lua_isfunction(L, -1)) {               // unresolved / not callable (R5.5)
        lua_pop(L, 1);                          // pop the non-function value
        return;                                 // no call, no error, continue
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {      // call: 0 args, 0 results (R5.2)
        const char* err = lua_tostring(L, -1);  // contain runtime error (R5.6)
        std::cerr << "[UISystem] callback '" << fn_name << "' error: "
                  << (err ? err : "unknown") << std::endl;
        lua_pop(L, 1);                          // pop the error message
    }
    // frame continues regardless (R5.5, R5.6)
}
