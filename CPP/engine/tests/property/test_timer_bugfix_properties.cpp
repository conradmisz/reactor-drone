/**
 * Property-based tests for Timer bugfix verification
 *
 * These tests target two specific bugs in the Timer class:
 *   Bug 1 (Init): delta_time_ is initialized to 1.0/target_fps instead of 0.0
 *   Bug 2 (Clamp): delta_time_ can exceed 1.0s because the clamp is applied
 *          to frame_elapsed but not to total_frame_time
 *
 * EXPLORATION TEST — expected to FAIL on unfixed code to confirm bugs exist.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/timer.hpp"
#include <thread>
#include <chrono>

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;  // Number of different FPS / scenario values
[[maybe_unused]] constexpr int NUM_INNER_TESTS = 5;   // Reserved for multi-iteration scenarios

// ---------------------------------------------------------------------------
// Property 1a: Init bug — delta_time should be 0.0 before any frame
// **Validates: Requirements 1.1, 2.1**
// ---------------------------------------------------------------------------
TEST_CASE("Bug condition: delta_time is zero before any frame", "[timer][bugfix][property]") {
    SECTION("For any positive target_fps, get_delta_time() == 0.0 before any frame") {
        auto target_fps = GENERATE(take(NUM_OUTER_TESTS, random(1.0, 240.0)));

        Timer timer(target_fps);

        // Before any start_frame()/end_frame() cycle, delta_time must be 0.0
        REQUIRE(timer.get_delta_time() == 0.0);
    }
}

// ---------------------------------------------------------------------------
// Property 1b: Clamp bug — delta_time must be <= 1.0 after a long frame
// **Validates: Requirements 1.2, 2.2**
// ---------------------------------------------------------------------------
TEST_CASE("Bug condition: delta_time is clamped to 1.0 after long frame", "[timer][bugfix][property]") {
    SECTION("After a 1.5-second frame, get_delta_time() <= 1.0") {
        Timer timer(60.0);

        timer.start_frame();

        // Sleep for 1.5 seconds to simulate a very long frame
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

        timer.end_frame();

        // delta_time must be clamped to at most 1.0 second
        REQUIRE(timer.get_delta_time() <= 1.0);
    }
}

// ===========================================================================
// PRESERVATION PROPERTY TESTS
// These tests PASS on unfixed code and verify behavior that must be preserved
// after the bugfix is applied.
// ===========================================================================

#include "engine/ecs/blackboard.hpp"

// ---------------------------------------------------------------------------
// Property 2a: Normal frame delta_time reflects actual total frame time
// For frames with work time < 1.0s, delta_time_ reflects actual total frame
// time (including sleep to hit target FPS) and is ≤ 1.0.
// **Validates: Requirements 3.1, 3.2**
// ---------------------------------------------------------------------------
TEST_CASE("Preservation: normal frame delta_time reflects actual total frame time",
          "[timer][bugfix][preservation][property]") {
    SECTION("For short work durations, delta_time is positive and <= 1.0") {
        auto sleep_ms = GENERATE(take(NUM_OUTER_TESTS, random(1, 50)));

        Timer timer(60.0);

        timer.start_frame();

        // Simulate work that is well under 1.0 second
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

        timer.end_frame();

        double dt = timer.get_delta_time();

        // delta_time must be at least as long as the work we did
        REQUIRE(dt >= sleep_ms / 1000.0);

        // delta_time must not exceed 1.0 second (non-bug-condition frames)
        REQUIRE(dt <= 1.0);

        // delta_time must be positive
        REQUIRE(dt > 0.0);
    }
}

// ---------------------------------------------------------------------------
// Property 2b: frame_count_ equals N after N end_frame() calls
// **Validates: Requirements 3.3, 3.4**
// ---------------------------------------------------------------------------
TEST_CASE("Preservation: frame_count equals number of frames executed",
          "[timer][bugfix][preservation][property]") {
    SECTION("After N frames, frame_count == N") {
        auto num_frames = GENERATE(take(NUM_OUTER_TESTS, random(1, 10)));

        Timer timer(60.0);

        REQUIRE(timer.get_frame_count() == 0);

        for (int i = 0; i < num_frames; ++i) {
            timer.start_frame();
            // Minimal work — just enough to advance the clock
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            timer.end_frame();
        }

        REQUIRE(timer.get_frame_count() == static_cast<uint64_t>(num_frames));
    }
}

// ---------------------------------------------------------------------------
// Property 2c: update_blackboard() writes values matching Timer getters
// **Validates: Requirements 3.3, 3.4**
// ---------------------------------------------------------------------------
TEST_CASE("Preservation: update_blackboard writes values matching Timer getters",
          "[timer][bugfix][preservation][property]") {
    SECTION("Blackboard values match getters after frames") {
        auto num_frames = GENERATE(take(NUM_OUTER_TESTS, random(1, 10)));

        Timer timer(60.0);
        Blackboard blackboard;

        for (int i = 0; i < num_frames; ++i) {
            timer.start_frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            timer.end_frame();
        }

        timer.update_blackboard(blackboard);

        // Blackboard must contain all three keys
        REQUIRE(blackboard.has("delta_time"));
        REQUIRE(blackboard.has("fps"));
        REQUIRE(blackboard.has("frame_count"));

        // Values must match the Timer's own getters exactly
        REQUIRE(blackboard.get<double>("delta_time") == timer.get_delta_time());
        REQUIRE(blackboard.get<double>("fps") == timer.get_fps());
        REQUIRE(blackboard.get<uint64_t>("frame_count") == timer.get_frame_count());

        // Sanity: frame_count matches what we ran
        REQUIRE(blackboard.get<uint64_t>("frame_count") == static_cast<uint64_t>(num_frames));
    }
}
