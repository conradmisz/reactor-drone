#ifndef UI_SYSTEM_HPP
#define UI_SYSTEM_HPP

#include <cstdint>
#include <string>

// Forward declarations — no #include needed in the header
class LuaManager;
class ComponentStorage;
class EntityManager;
class Blackboard;

/**
 * UISystem — per-frame mouse interaction for active-screen widgets.
 *
 * Runs once per frame after InputSystem (so the current frame's pointer and
 * mouse-button edges are available on the Blackboard) and before any game
 * system that consumes the mouse, and before the UIRenderSystem reads UIState.
 *
 * For each enabled, in-scope widget (a widget whose ScreenMembership names an
 * active UIScreen) it:
 * - sets UIState.hovered from an inclusive-bounds hit test (a pointer exactly on
 *   any edge is inside);
 * - sets UIState.pressed on a mouse-button-down inside the widget;
 * - on a mouse-button-up inside the SAME widget, fires the widget's on_click_fn
 *   global Lua callback (a confirmed click), and clears pressed on every
 *   release.
 *
 * Disabled widgets are skipped entirely: no hover, no press, no callback, and
 * UIState.value is left untouched. Widgets with no ScreenMembership, or whose
 * membership names only inactive screens, are left untouched.
 *
 * Pointer hit-testing happens in bottom-left-origin UI coordinates: the SDL
 * top-left screen-pixel pointer is Y-inverted (ui_y = window_height - screen_y)
 * with NO camera/zoom/lookat transform, so the pointer and the widget rect are
 * compared in one consistent space.
 */
class UISystem {
public:
    /**
     * Blackboard key carrying the on_click_fn name of the widget whose click was
     * confirmed this frame (v2 addition). Written alongside — not instead of —
     * the Lua dispatch, so a game with no Lua menu layer can still react to its
     * own buttons. The consumer is responsible for removing the key once handled;
     * UISystem never clears it, so an unread click is not silently dropped.
     */
    static constexpr const char* UI_CLICK_KEY = "ui.clicked_fn";

    /**
     * Construct the UISystem.
     *
     * @param lua_manager   Engine Lua state owner used to resolve and call the
     *                      named global on_click_fn callbacks.
     * @param window_height Fallback window height in pixels for the pointer
     *                      Y-inversion; update() prefers the live "window_height"
     *                      Blackboard key when present (the window is resizable).
     */
    UISystem(LuaManager& lua_manager, int window_height);

    /**
     * Process one frame of widget interaction.
     *
     * Reads the per-frame mouse state from the Blackboard, hit-tests the enabled
     * in-scope widgets in bottom-left-origin space, mutates their UIState
     * (hovered/pressed), and invokes confirmed-click Lua callbacks. If no widget
     * is in scope, the update completes without changing any UIState.
     *
     * @param storage        ComponentStorage to query for widget/screen entities
     * @param entity_manager EntityManager (passed through for callback wiring)
     * @param blackboard     Blackboard holding the per-frame mouse state and the
     *                       window height
     */
    void update(ComponentStorage& storage,
                EntityManager& entity_manager,
                Blackboard& blackboard);

private:
    /**
     * Resolve a named GLOBAL Lua function and call it with zero arguments under
     * lua_pcall. A name that does not resolve to a callable global results in no
     * call and no error; a runtime error raised by the callback is logged and
     * contained so the frame continues.
     *
     * @param fn_name Name of the global Lua function to invoke
     */
    void invoke_callback(const std::string& fn_name);

    /**
     * Publish a confirmed click's callback name to the Blackboard under
     * UI_CLICK_KEY. See the key's documentation for why this exists.
     */
    static void publish_click(Blackboard& blackboard, const std::string& fn_name);

    LuaManager& lua_manager_;
    int  window_height_;
    bool bindings_registered_ = false;  // engine.* registered once on this state

    // Phase 7 keyboard focus. UISystem is the single writer of UIState.focused.
    // focused_entity_ is an Entity id (uint32_t alias); has_focus_ guards it
    // (id 0 is a valid entity, so a separate validity flag is required).
    bool          has_focus_ = false;
    std::uint32_t focused_entity_ = 0;
};

#endif // UI_SYSTEM_HPP
