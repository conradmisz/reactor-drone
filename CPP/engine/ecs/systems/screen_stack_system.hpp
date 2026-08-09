#ifndef SCREEN_STACK_SYSTEM_HPP
#define SCREEN_STACK_SYSTEM_HPP

#include <string>
#include <vector>
#include "engine/ecs/component_storage.hpp"

class Blackboard;

/**
 * ScreenStackSystem (Phase 4) — single writer of the Blackboard "screen_stack"
 * ordered list and of every UIScreen.active flag.
 *
 * The front of the list is the base; the back is the Top_Screen. The base entry
 * ("gameplay") represents the non-modal gameplay layer: it has no UIScreen
 * entity and is never popped. Stack depth > 1 means a modal screen is active
 * above the base.
 *
 * The system promotes the Phase 2 stub (which only toggled UIScreen.active for
 * an exact-match name) into a full stack that keeps an ordered list of screen
 * names in the Blackboard and reconciles every UIScreen.active flag to agree
 * with that list after each mutation, eliminating list/flag desync.
 *
 * All operations are deterministic and SDL-free, exercised headlessly by
 * Catch2 unit and property tests.
 */
class ScreenStackSystem {
public:
    // Blackboard keys (the data contract).
    static constexpr const char* STACK_KEY    = "screen_stack";        // vector<string>: ordered stack (front=base, back=top)
    static constexpr const char* BASE_SCREEN  = "gameplay";            // sentinel base; no UIScreen entity
    static constexpr const char* CMD_PUSH     = "ui.cmd.push";         // string: screen name to push
    static constexpr const char* CMD_POP      = "ui.cmd.pop";          // bool true: pop Top_Screen
    static constexpr const char* CMD_CLEAR_TO = "ui.cmd.clear_to";     // string: clear to base + name
    static constexpr const char* CAPTURE_KEY  = "ui_captured_input";   // bool (written by UISystem)

    /**
     * Initialize the stack to [BASE_SCREEN] and reconcile all UIScreen.active.
     *
     * Establishes the live truth at startup: the stack becomes ["gameplay"]
     * (depth 1, non-modal) and every UIScreen is reconciled — any screen whose
     * name is not in the stack (e.g. main_menu, pause) is set inactive. (R1.2)
     *
     * @param blackboard Blackboard holding the screen_stack list
     * @param storage    ComponentStorage holding the UIScreen entities
     */
    void initialize(Blackboard& blackboard, ComponentStorage& storage);

    /**
     * Push a screen name onto the back of the stack, then reconcile.
     *
     * Appends @p name to the back of the stack and reconciles every
     * UIScreen.active flag. An empty name is a no-op (stack and flags unchanged)
     * and does not throw. (R2, R2.5)
     *
     * @param name       Screen name to push (empty -> no-op)
     * @param blackboard Blackboard holding the screen_stack list
     * @param storage    ComponentStorage holding the UIScreen entities
     */
    void push_screen(const std::string& name, Blackboard& blackboard, ComponentStorage& storage);

    /**
     * Pop the Top_Screen off the back of the stack, then reconcile.
     *
     * Removes the back entry only if depth > 1, then reconciles every
     * UIScreen.active flag. At depth 1 the base is retained and the call is a
     * no-op that does not throw. (R3, R3.5)
     *
     * @param blackboard Blackboard holding the screen_stack list
     * @param storage    ComponentStorage holding the UIScreen entities
     */
    void pop_screen(Blackboard& blackboard, ComponentStorage& storage);

    /**
     * Truncate the stack to [BASE_SCREEN], push @p name, then reconcile.
     *
     * Resets the stack to the base plus a single named screen (when @p name is
     * non-empty) and reconciles every UIScreen.active flag. The base entry is
     * always retained at the front. (R11.6)
     *
     * @param name       Screen name to clear to (empty -> stack becomes base only)
     * @param blackboard Blackboard holding the screen_stack list
     * @param storage    ComponentStorage holding the UIScreen entities
     */
    void clear_to(const std::string& name, Blackboard& blackboard, ComponentStorage& storage);

    /**
     * Consume the CMD_* command keys once for this frame.
     *
     * The single consumer of the command-key protocol. Applies, in fixed order,
     * clear_to -> push -> pop, removing each command key as it is consumed. A
     * frame with no command pending makes no change and is idempotent.
     * (R11.4-6, R5)
     *
     * @param blackboard Blackboard holding the command keys and screen_stack
     * @param storage    ComponentStorage holding the UIScreen entities
     */
    void process_commands(Blackboard& blackboard, ComponentStorage& storage);

    /**
     * Read the current stack as an ordered list of screen names.
     *
     * @param blackboard Blackboard holding the screen_stack list
     * @return The stack (front=base, back=top); empty if never initialized
     */
    static std::vector<std::string> get_stack(const Blackboard& blackboard);

    /**
     * Current stack depth.
     *
     * @param blackboard Blackboard holding the screen_stack list
     * @return The number of entries (>= 1 after initialize; 0 if absent)
     */
    static std::size_t depth(const Blackboard& blackboard);

    /**
     * Whether a modal screen is active above the base.
     *
     * @param blackboard Blackboard holding the screen_stack list
     * @return true iff depth > 1
     */
    static bool is_modal(const Blackboard& blackboard);

private:
    /**
     * The ONLY writer of UIScreen.active.
     *
     * Sets active=true for every UIScreen whose screen_name is present in the
     * current stack, and false otherwise. Runs after every initialize /
     * push_screen / pop_screen / clear_to, guaranteeing the set of active screen
     * names always equals the set of stacked names that correspond to a real
     * UIScreen entity (the base "gameplay" matches no entity — harmless).
     * (R1.5, R4.1, R4.2)
     *
     * @param blackboard Blackboard holding the screen_stack list
     * @param storage    ComponentStorage holding the UIScreen entities
     */
    void reconcile_active_flags(Blackboard& blackboard, ComponentStorage& storage);
};

#endif // SCREEN_STACK_SYSTEM_HPP
