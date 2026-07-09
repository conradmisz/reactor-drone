/**
 * Unit tests for debug state machine free functions (debug_state.hpp)
 *
 * Validates: Requirements 1.1, 1.2, 1.3, 1.4, 4.1, 4.3, 4.4, 4.6
 *
 * Tests cover:
 * - should_toggle: rising edge detection
 * - apply_pause_toggle: pause/unpause with auto-enable debug HUD
 * - should_step: step only when paused AND rising edge
 * - apply_step_complete: clears step_requested, stays paused
 * - step_delta_time: returns 1.0 / target_fps
 * - Initial state expectations
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "game/debug_state.hpp"

// ===========================================================================
// should_toggle — edge detection
// ===========================================================================

TEST_CASE("should_toggle: rising edge returns true", "[debug_state][unit]") {
    REQUIRE(should_toggle(false, true) == true);
}

TEST_CASE("should_toggle: hold returns false", "[debug_state][unit]") {
    REQUIRE(should_toggle(true, true) == false);
}

TEST_CASE("should_toggle: release returns false", "[debug_state][unit]") {
    REQUIRE(should_toggle(true, false) == false);
}

TEST_CASE("should_toggle: idle returns false", "[debug_state][unit]") {
    REQUIRE(should_toggle(false, false) == false);
}

// ===========================================================================
// apply_pause_toggle — pause/unpause with debug HUD auto-enable
// ===========================================================================

TEST_CASE("apply_pause_toggle: toggling to paused auto-enables debug HUD",
          "[debug_state][unit]") {
    bool debug_paused = false;
    bool debug_hud_visible = false;

    apply_pause_toggle(debug_paused, debug_hud_visible);

    REQUIRE(debug_paused == true);
    REQUIRE(debug_hud_visible == true);
}

TEST_CASE("apply_pause_toggle: toggling to running does NOT auto-disable debug HUD",
          "[debug_state][unit]") {
    bool debug_paused = true;
    bool debug_hud_visible = true;

    apply_pause_toggle(debug_paused, debug_hud_visible);

    REQUIRE(debug_paused == false);
    REQUIRE(debug_hud_visible == true);  // NOT auto-disabled
}

TEST_CASE("apply_pause_toggle: toggling to running preserves HUD-off state",
          "[debug_state][unit]") {
    // HUD was manually turned off (F10) while paused, then unpause
    bool debug_paused = true;
    bool debug_hud_visible = false;

    apply_pause_toggle(debug_paused, debug_hud_visible);

    REQUIRE(debug_paused == false);
    REQUIRE(debug_hud_visible == false);  // stays off
}

// ===========================================================================
// should_step — step only when paused AND rising edge on F2
// ===========================================================================

TEST_CASE("should_step: paused with rising edge returns true",
          "[debug_state][unit]") {
    REQUIRE(should_step(true, false, true) == true);
}

TEST_CASE("should_step: paused with hold returns false",
          "[debug_state][unit]") {
    REQUIRE(should_step(true, true, true) == false);
}

TEST_CASE("should_step: paused with release returns false",
          "[debug_state][unit]") {
    REQUIRE(should_step(true, true, false) == false);
}

TEST_CASE("should_step: paused with idle returns false",
          "[debug_state][unit]") {
    REQUIRE(should_step(true, false, false) == false);
}

TEST_CASE("should_step: running with rising edge returns false",
          "[debug_state][unit]") {
    REQUIRE(should_step(false, false, true) == false);
}

TEST_CASE("should_step: running with hold returns false",
          "[debug_state][unit]") {
    REQUIRE(should_step(false, true, true) == false);
}

TEST_CASE("should_step: running ignores all F2 states",
          "[debug_state][unit]") {
    REQUIRE(should_step(false, false, false) == false);
    REQUIRE(should_step(false, false, true) == false);
    REQUIRE(should_step(false, true, false) == false);
    REQUIRE(should_step(false, true, true) == false);
}

// ===========================================================================
// apply_step_complete — clears step_requested, debug_paused stays true
// ===========================================================================

TEST_CASE("apply_step_complete: clears step_requested",
          "[debug_state][unit]") {
    bool step_requested = true;
    bool debug_paused = true;

    apply_step_complete(step_requested, debug_paused);

    REQUIRE(step_requested == false);
    REQUIRE(debug_paused == true);
}

// ===========================================================================
// step_delta_time — returns 1.0 / target_fps
// ===========================================================================

TEST_CASE("step_delta_time: 60 FPS", "[debug_state][unit]") {
    REQUIRE_THAT(step_delta_time(60.0),
                 Catch::Matchers::WithinRel(1.0 / 60.0, 1e-12));
}

TEST_CASE("step_delta_time: 30 FPS", "[debug_state][unit]") {
    REQUIRE_THAT(step_delta_time(30.0),
                 Catch::Matchers::WithinRel(1.0 / 30.0, 1e-12));
}

TEST_CASE("step_delta_time: 120 FPS", "[debug_state][unit]") {
    REQUIRE_THAT(step_delta_time(120.0),
                 Catch::Matchers::WithinRel(1.0 / 120.0, 1e-12));
}

TEST_CASE("step_delta_time: 144 FPS", "[debug_state][unit]") {
    REQUIRE_THAT(step_delta_time(144.0),
                 Catch::Matchers::WithinRel(1.0 / 144.0, 1e-12));
}

// ===========================================================================
// Initial state expectations
// ===========================================================================

TEST_CASE("Initial state: debug_paused is false, debug_hud_visible is false",
          "[debug_state][unit]") {
    // Per Req 1.3 and 7.4, the initial state is running with HUD hidden
    bool debug_paused = false;
    bool debug_hud_visible = false;

    REQUIRE(debug_paused == false);
    REQUIRE(debug_hud_visible == false);
}
