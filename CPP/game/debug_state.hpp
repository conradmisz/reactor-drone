/**
 * debug_state.hpp - Pure free functions for the debug state machine
 *
 * Provides edge-detection, pause/step toggle logic, and step-frame
 * delta-time computation used by the main loop's debug controls.
 * All functions are pure (no SDL dependency), making them directly
 * testable via unit and property tests.
 *
 * Requirements: 1.1, 1.2, 1.4, 4.1, 4.3, 4.4, 4.6
 */

#ifndef DEBUG_STATE_HPP
#define DEBUG_STATE_HPP

/**
 * Detect a rising edge (transition from not-pressed to pressed).
 *
 * Returns true only when the key was NOT pressed last frame and IS
 * pressed this frame. Holding a key or releasing it never triggers.
 *
 * @param was_pressed  Key state on the previous frame
 * @param is_pressed   Key state on the current frame
 * @return true on rising edge only
 *
 * Requirements: 1.1, 1.2, 7.1, 7.2
 */
inline bool should_toggle(bool was_pressed, bool is_pressed) {
    return !was_pressed && is_pressed;
}

/**
 * Determine whether a single-step should occur.
 *
 * A step is requested only when the game is paused AND the F2 key
 * has a rising edge. F2 presses while running are ignored.
 *
 * @param debug_paused     Current pause state
 * @param f2_was_pressed   F2 key state on the previous frame
 * @param f2_is_pressed    F2 key state on the current frame
 * @return true if a step should be executed
 *
 * Requirements: 4.1, 4.4
 */
inline bool should_step(bool debug_paused, bool f2_was_pressed, bool f2_is_pressed) {
    return debug_paused && should_toggle(f2_was_pressed, f2_is_pressed);
}

/**
 * Apply a pause toggle triggered by F1.
 *
 * Flips debug_paused. If the game is now paused, the debug HUD is
 * automatically made visible. Unpausing does NOT auto-disable the
 * debug HUD so the developer can keep viewing debug info while running.
 *
 * @param debug_paused      [in/out] Toggled between true and false
 * @param debug_hud_visible [in/out] Set to true when pausing
 *
 * Requirements: 1.1, 1.4
 */
inline void apply_pause_toggle(bool& debug_paused, bool& debug_hud_visible) {
    debug_paused = !debug_paused;
    if (debug_paused) {
        debug_hud_visible = true;
    }
}

/**
 * Finalize a completed step frame.
 *
 * Clears the step_requested flag while keeping the game paused.
 *
 * @param step_requested [in/out] Cleared to false
 * @param debug_paused   [in/out] Remains true (unchanged)
 *
 * Requirements: 4.3
 */
inline void apply_step_complete(bool& step_requested, bool& debug_paused) {
    step_requested = false;
    // debug_paused remains true — the game stays paused after a step
    (void)debug_paused;
}

/**
 * Compute the fixed delta_time used during a step frame.
 *
 * Returns 1.0 / target_fps so that step behavior is deterministic
 * regardless of how long the game was paused.
 *
 * @param target_fps  The game's target frame rate (e.g. 60.0)
 * @return Fixed delta_time in seconds
 *
 * Requirements: 4.6
 */
inline double step_delta_time(double target_fps) {
    return 1.0 / target_fps;
}

#endif // DEBUG_STATE_HPP
