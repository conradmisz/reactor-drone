/**
 * Property-based tests for debug state machine free functions (debug_state.hpp)
 *
 * These tests verify five correctness properties from the design document:
 *   Property 1: Edge-detection toggle only fires on rising edge
 *   Property 2: Pausing auto-enables debug HUD
 *   Property 5: Step request conditioned on pause state
 *   Property 6: Step completes and remains paused
 *   Property 7: Fixed delta_time for step frames
 *
 * Feature: 040-04-debug-pause-step
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "game/debug_state.hpp"

// Configurable test iteration counts (MANDATORY — workspace policy)
constexpr int NUM_OUTER_TESTS = 10;  // Number of different test scenarios
constexpr int NUM_INNER_TESTS = 5;   // Number of iterations per scenario

// ---------------------------------------------------------------------------
// Feature: 040-04-debug-pause-step, Property 1: Edge-detection toggle only fires on rising edge
//
// For any (was_pressed, is_pressed) pair, should_toggle returns true
// iff !was_pressed && is_pressed.
//
// **Validates: Requirements 1.1, 1.2, 7.1, 7.2, 7.3**
// ---------------------------------------------------------------------------
TEST_CASE("Edge-detection toggle only fires on rising edge",
          "[debug_state][property]") {
    SECTION("should_toggle matches !was_pressed && is_pressed for all bool pairs") {
        auto was_val = GENERATE(take(NUM_OUTER_TESTS, random(0, 1)));
        auto is_val  = GENERATE(take(NUM_INNER_TESTS, random(0, 1)));

        bool was_pressed = static_cast<bool>(was_val);
        bool is_pressed  = static_cast<bool>(is_val);

        bool expected = !was_pressed && is_pressed;
        REQUIRE(should_toggle(was_pressed, is_pressed) == expected);
    }
}

// ---------------------------------------------------------------------------
// Feature: 040-04-debug-pause-step, Property 2: Pausing auto-enables debug HUD
//
// For any sequence of F1 toggles starting from debug_paused=false,
// debug_hud_visible=false, whenever state transitions to paused,
// debug_hud_visible must be true.
//
// **Validates: Requirements 1.4**
// ---------------------------------------------------------------------------
TEST_CASE("Pausing auto-enables debug HUD",
          "[debug_state][property]") {
    SECTION("After a random sequence of toggles, paused implies HUD visible") {
        // Generate a random number of F1 toggle presses (1..20)
        auto num_toggles = GENERATE(take(NUM_OUTER_TESTS, random(1, 20)));

        bool debug_paused     = false;
        bool debug_hud_visible = false;

        for (int i = 0; i < num_toggles; ++i) {
            apply_pause_toggle(debug_paused, debug_hud_visible);

            // Whenever we transition to paused, HUD must be visible
            if (debug_paused) {
                REQUIRE(debug_hud_visible == true);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Feature: 040-04-debug-pause-step, Property 5: Step request conditioned on pause state
//
// For any (debug_paused, f2_was, f2_is), should_step is true
// iff debug_paused && !f2_was && f2_is.
//
// **Validates: Requirements 4.1, 4.4**
// ---------------------------------------------------------------------------
TEST_CASE("Step request conditioned on pause state",
          "[debug_state][property]") {
    SECTION("should_step matches debug_paused && !f2_was && f2_is") {
        auto paused_val = GENERATE(take(NUM_OUTER_TESTS, random(0, 1)));
        auto f2_was_val = GENERATE(take(NUM_INNER_TESTS, random(0, 1)));
        auto f2_is_val  = GENERATE(take(NUM_INNER_TESTS, random(0, 1)));

        bool debug_paused  = static_cast<bool>(paused_val);
        bool f2_was        = static_cast<bool>(f2_was_val);
        bool f2_is         = static_cast<bool>(f2_is_val);

        bool expected = debug_paused && !f2_was && f2_is;
        REQUIRE(should_step(debug_paused, f2_was, f2_is) == expected);
    }
}

// ---------------------------------------------------------------------------
// Feature: 040-04-debug-pause-step, Property 6: Step completes and remains paused
//
// For any state where debug_paused=true, step_requested=true,
// after apply_step_complete, step_requested=false and debug_paused=true.
//
// **Validates: Requirements 4.3**
// ---------------------------------------------------------------------------
TEST_CASE("Step completes and remains paused",
          "[debug_state][property]") {
    SECTION("apply_step_complete clears step and keeps paused across iterations") {
        // Run the same invariant check multiple times to satisfy property form
        auto iteration = GENERATE(take(NUM_OUTER_TESTS, random(1, 100)));
        (void)iteration;  // value unused — each iteration re-checks the invariant

        bool step_requested = true;
        bool debug_paused   = true;

        apply_step_complete(step_requested, debug_paused);

        REQUIRE(step_requested == false);
        REQUIRE(debug_paused == true);
    }
}

// ---------------------------------------------------------------------------
// Feature: 040-04-debug-pause-step, Property 7: Fixed delta_time for step frames
//
// For any positive target_fps, step_delta_time(target_fps) == 1.0 / target_fps.
//
// **Validates: Requirements 4.6**
// ---------------------------------------------------------------------------
TEST_CASE("Fixed delta_time for step frames",
          "[debug_state][property]") {
    SECTION("step_delta_time equals 1.0 / target_fps for random positive FPS") {
        auto fps = GENERATE(take(NUM_OUTER_TESTS, random(1.0, 300.0)));

        double result   = step_delta_time(fps);
        double expected = 1.0 / fps;

        REQUIRE_THAT(result, Catch::Matchers::WithinRel(expected, 1e-12));
    }
}
