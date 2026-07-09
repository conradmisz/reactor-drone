#include <catch2/catch_test_macros.hpp>
#include "engine/timer.hpp"
#include "engine/ecs/blackboard.hpp"
#include <thread>
#include <chrono>

// ==========================================================================
// Unit tests for Timer::end_frame_no_advance()
// Validates: Requirements 5.2, 5.3, 5.4, 5.5, 9.1
// ==========================================================================

TEST_CASE("end_frame_no_advance does not increment frame count", "[timer][unit][no_advance]") {
    Timer timer(60.0);

    // Establish a baseline frame count with normal end_frame() calls
    timer.start_frame();
    timer.end_frame();
    timer.start_frame();
    timer.end_frame();
    timer.start_frame();
    timer.end_frame();

    uint64_t baseline = timer.get_frame_count();
    REQUIRE(baseline == 3);

    SECTION("Single no-advance call preserves frame count") {
        timer.start_frame();
        timer.end_frame_no_advance();

        REQUIRE(timer.get_frame_count() == baseline);
    }

    SECTION("Multiple no-advance calls preserve frame count") {
        for (int i = 0; i < 5; i++) {
            timer.start_frame();
            timer.end_frame_no_advance();
        }

        REQUIRE(timer.get_frame_count() == baseline);
    }
}

TEST_CASE("end_frame_no_advance returns positive delta_time", "[timer][unit][no_advance]") {
    Timer timer(60.0);

    timer.start_frame();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    timer.end_frame_no_advance();

    double delta = timer.get_delta_time();
    REQUIRE(delta > 0.0);
    REQUIRE(delta < 0.1);  // Sanity upper bound
}

TEST_CASE("end_frame still increments frame count (regression)", "[timer][unit][no_advance]") {
    Timer timer(60.0);

    // Mix of end_frame and end_frame_no_advance calls
    timer.start_frame();
    timer.end_frame();           // frame_count -> 1
    REQUIRE(timer.get_frame_count() == 1);

    timer.start_frame();
    timer.end_frame_no_advance(); // frame_count stays 1
    REQUIRE(timer.get_frame_count() == 1);

    timer.start_frame();
    timer.end_frame();           // frame_count -> 2
    REQUIRE(timer.get_frame_count() == 2);

    timer.start_frame();
    timer.end_frame_no_advance(); // frame_count stays 2
    REQUIRE(timer.get_frame_count() == 2);

    timer.start_frame();
    timer.end_frame_no_advance(); // frame_count stays 2
    REQUIRE(timer.get_frame_count() == 2);

    timer.start_frame();
    timer.end_frame();           // frame_count -> 3
    REQUIRE(timer.get_frame_count() == 3);
}
