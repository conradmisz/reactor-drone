/**
 * Property-based tests for Timer::end_frame_no_advance()
 *
 * These tests verify two correctness properties from the design document:
 *   Property 3: end_frame_no_advance preserves frame count
 *   Property 4: end_frame_no_advance updates delta_time
 *
 * Feature: 040-04-debug-pause-step
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/timer.hpp"
#include <thread>
#include <chrono>

// Configurable test iteration counts (MANDATORY — workspace policy)
constexpr int NUM_OUTER_TESTS = 10;  // Number of different test scenarios
constexpr int NUM_INNER_TESTS = 5;   // Number of iterations per scenario

// ---------------------------------------------------------------------------
// Feature: 040-04-debug-pause-step, Property 3: end_frame_no_advance preserves frame count
//
// For any Timer with N prior end_frame() calls, calling start_frame() +
// end_frame_no_advance() M additional times leaves get_frame_count() == N.
//
// **Validates: Requirements 3.5, 5.2**
// ---------------------------------------------------------------------------
TEST_CASE("end_frame_no_advance preserves frame count", "[timer][no_advance][property]") {
    SECTION("Frame count unchanged after M no-advance calls following N normal frames") {
        auto n_normal = GENERATE(take(NUM_OUTER_TESTS, random(0, 10)));
        auto m_no_advance = GENERATE(take(NUM_INNER_TESTS, random(1, 10)));

        Timer timer(60.0);

        // Establish N frames via normal end_frame()
        for (int i = 0; i < n_normal; ++i) {
            timer.start_frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            timer.end_frame();
        }

        uint64_t count_before = timer.get_frame_count();
        REQUIRE(count_before == static_cast<uint64_t>(n_normal));

        // Call start_frame() + end_frame_no_advance() M times
        for (int i = 0; i < m_no_advance; ++i) {
            timer.start_frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            timer.end_frame_no_advance();
        }

        // Frame count must be unchanged
        REQUIRE(timer.get_frame_count() == count_before);
    }
}

// ---------------------------------------------------------------------------
// Feature: 040-04-debug-pause-step, Property 4: end_frame_no_advance updates delta_time
//
// After start_frame(), sleep for a positive duration, then
// end_frame_no_advance(), get_delta_time() >= sleep duration.
//
// **Validates: Requirements 5.5**
// ---------------------------------------------------------------------------
TEST_CASE("end_frame_no_advance updates delta_time", "[timer][no_advance][property]") {
    SECTION("delta_time reflects wall-clock elapsed time after no-advance") {
        auto sleep_ms = GENERATE(take(NUM_OUTER_TESTS, random(2, 30)));

        Timer timer(60.0);

        timer.start_frame();

        // Sleep for a positive duration
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

        timer.end_frame_no_advance();

        double dt = timer.get_delta_time();

        // delta_time must be at least the sleep duration
        REQUIRE(dt >= sleep_ms / 1000.0);

        // delta_time must be positive
        REQUIRE(dt > 0.0);

        // delta_time must be reasonable (not exceeding 1 second)
        REQUIRE(dt <= 1.0);
    }
}
