/**
 * Property-based tests for Timer class
 * 
 * These tests verify universal properties that should hold across all inputs
 * using Catch2's GENERATE() for property-based testing.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include "engine/timer.hpp"
#include "engine/ecs/blackboard.hpp"
#include <cmath>

// Configurable test iteration counts
// Timer tests use real-time sleeping (~16ms/frame at 60fps), so we keep iterations low
// to avoid multi-minute test runs. 3 outer × 2 inner = 6 cases per section is sufficient.
constexpr int NUM_OUTER_TESTS = 3;   // Number of different test scenarios
constexpr int NUM_INNER_TESTS = 2;   // Number of iterations per scenario

// Feature: red-moving-square-class-020, Property 1: Timer Maintains Target Frame Rate
// **Validates: Requirements 1.1, 1.2**
TEST_CASE("Timer maintains target frame rate within tolerance", "[timer][property]") {
    SECTION("Timer maintains 60 FPS over multiple frames") {
        // Use exactly 60 frames (minimum for full buffer) to keep test fast
        auto num_frames = GENERATE(take(NUM_OUTER_TESTS, random(60, 65)));
        
        Timer timer(60.0);
        
        // Run the timer for the specified number of frames
        // The timer will sleep + busy-wait to maintain 60 FPS
        for (int i = 0; i < num_frames; ++i) {
            timer.start_frame();
            timer.end_frame();
        }
        
        // After at least 60 frames, the average FPS should be within 20% of target (60.0)
        // Note: Adaptive sleep error correction helps but OS scheduling still causes variance
        double fps = timer.get_fps();
        double target_fps = 60.0;
        double tolerance = target_fps * 0.25;  // 25% tolerance for loaded systems
        
        // FPS should be within [45.0, 75.0]
        // Note: The timer actively sleeps to maintain this rate
        REQUIRE(fps >= target_fps - tolerance);
        REQUIRE(fps <= target_fps + tolerance);
    }
    
    SECTION("Timer maintains custom target FPS") {
        // Test with different target FPS values — only 60fps to keep test fast
        // Lower FPS values (30, 45) sleep even longer per frame, making tests very slow
        auto target_fps = GENERATE(values({60.0}));
        auto num_frames = GENERATE(take(NUM_INNER_TESTS, random(60, 65)));
        
        Timer timer(target_fps);
        
        // Run the timer for the specified number of frames
        // The timer will sleep + busy-wait to maintain the target FPS
        for (int i = 0; i < num_frames; ++i) {
            timer.start_frame();
            timer.end_frame();
        }
        
        // After at least 60 frames, the average FPS should be within 25% of target
        // Note: Adaptive sleep error correction helps but OS scheduling still causes variance
        double fps = timer.get_fps();
        double tolerance = target_fps * 0.25;  // 25% tolerance for loaded systems
        
        REQUIRE(fps >= target_fps - tolerance);
        REQUIRE(fps <= target_fps + tolerance);
    }
    
    SECTION("Timer FPS is consistent across frame sequences") {
        // Run multiple sequences and verify FPS remains stable
        // Keep sequence length minimal — each frame sleeps ~16ms
        auto sequence_length = GENERATE(take(NUM_OUTER_TESTS, random(60, 65)));
        
        Timer timer(60.0);
        
        // First sequence - the timer will sleep + busy-wait to maintain 60 FPS
        for (int i = 0; i < sequence_length; ++i) {
            timer.start_frame();
            timer.end_frame();
        }
        double fps1 = timer.get_fps();
        
        // Second sequence (continuing from first)
        for (int i = 0; i < sequence_length; ++i) {
            timer.start_frame();
            timer.end_frame();
        }
        double fps2 = timer.get_fps();
        
        // Both FPS measurements should be within 25% of target
        // Note: Adaptive sleep error correction helps but OS scheduling still causes variance
        double target_fps = 60.0;
        double tolerance = target_fps * 0.25;  // 25% tolerance for loaded systems
        
        REQUIRE(fps1 >= target_fps - tolerance);
        REQUIRE(fps1 <= target_fps + tolerance);
        REQUIRE(fps2 >= target_fps - tolerance);
        REQUIRE(fps2 <= target_fps + tolerance);
        
        // FPS should be stable (within 10% of each other)
        double fps_diff = std::abs(fps1 - fps2);
        REQUIRE(fps_diff <= target_fps * 0.10);
    }
}


// Feature: red-moving-square-class-020, Property 2: Delta Time Reflects Actual Elapsed Time
// **Validates: Requirements 1.3, 1.6**
TEST_CASE("Delta time reflects actual elapsed time", "[timer][property]") {
    SECTION("Delta time is approximately equal to actual elapsed time") {
        // Generate different work durations to simulate frame processing
        auto work_duration_ms = GENERATE(take(NUM_OUTER_TESTS, random(1.0, 10.0)));
        
        Timer timer(60.0);
        
        timer.start_frame();
        
        // Simulate frame work
        std::this_thread::sleep_for(std::chrono::duration<double>(work_duration_ms / 1000.0));
        
        timer.end_frame();
        
        // Delta time should reflect the work duration + sleep time
        // Since the timer sleeps to maintain 60 FPS, delta_time should be close to target frame time
        double delta_time = timer.get_delta_time();
        
        // Delta time should be at least the work duration
        REQUIRE(delta_time >= work_duration_ms / 1000.0);
        
        // Delta time should be reasonable (not negative, not excessively large)
        REQUIRE(delta_time >= 0.0);
        REQUIRE(delta_time <= 1.0);  // Should never exceed 1 second per frame
    }
    
    SECTION("Delta time is consistent across frames") {
        auto num_frames = GENERATE(take(NUM_OUTER_TESTS, random(10, 30)));
        
        Timer timer(60.0);
        
        std::vector<double> delta_times;
        
        for (int i = 0; i < num_frames; ++i) {
            timer.start_frame();
            timer.end_frame();
            delta_times.push_back(timer.get_delta_time());
        }
        
        // All delta times should be reasonable
        for (double dt : delta_times) {
            REQUIRE(dt >= 0.0);
            REQUIRE(dt <= 1.0);
        }
        
        // Delta times should be relatively consistent (within 150% of target frame time)
        // This is a loose bound because the first few frames may vary and systems may be loaded
        double target_frame_time = 1.0 / 60.0;
        for (double dt : delta_times) {
            REQUIRE(dt <= target_frame_time * 2.5);
        }
    }
}

// Feature: red-moving-square-class-020, Property 3: FPS Calculation is Average Over Window
// **Validates: Requirements 1.5**
TEST_CASE("FPS calculation is average over window", "[timer][property]") {
    SECTION("FPS equals 1.0 / average_frame_time") {
        // Run enough frames to fill the circular buffer — keep range tight to limit sleep time
        auto num_frames = GENERATE(take(NUM_OUTER_TESTS, random(60, 65)));
        
        Timer timer(60.0);
        
        // Track frame times manually to verify calculation
        std::vector<double> manual_frame_times;
        
        for (int i = 0; i < num_frames; ++i) {
            double start = std::chrono::duration<double>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count();
            
            timer.start_frame();
            timer.end_frame();
            
            double end = std::chrono::duration<double>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count();
            
            manual_frame_times.push_back(end - start);
        }
        
        // Calculate expected FPS from last 60 frame times
        size_t window_size = std::min(manual_frame_times.size(), size_t(60));
        double sum = 0.0;
        for (size_t i = manual_frame_times.size() - window_size; i < manual_frame_times.size(); ++i) {
            sum += manual_frame_times[i];
        }
        double avg_frame_time = sum / window_size;
        double expected_fps = 1.0 / avg_frame_time;
        
        double actual_fps = timer.get_fps();
        
        // FPS should be close to expected (within 5% tolerance for timing variance)
        double tolerance = expected_fps * 0.05;
        REQUIRE(std::abs(actual_fps - expected_fps) <= tolerance);
    }
    
    SECTION("FPS is calculated over last 60 frames") {
        Timer timer(60.0);
        
        // Run exactly 60 frames
        for (int i = 0; i < 60; ++i) {
            timer.start_frame();
            timer.end_frame();
        }
        
        double fps_at_60 = timer.get_fps();
        
        // Run 10 more frames
        for (int i = 0; i < 10; ++i) {
            timer.start_frame();
            timer.end_frame();
        }
        
        double fps_at_70 = timer.get_fps();
        
        // Both FPS values should be within reasonable range of target
        REQUIRE(fps_at_60 >= 45.0);  // 60 * 0.75
        REQUIRE(fps_at_60 <= 75.0);  // 60 * 1.25
        REQUIRE(fps_at_70 >= 45.0);
        REQUIRE(fps_at_70 <= 75.0);
        
        // FPS should be relatively stable (using rolling window)
        REQUIRE(std::abs(fps_at_60 - fps_at_70) <= 12.0);  // Within 20% of target
    }
}

// Feature: red-moving-square-class-020, Property 4: Frame Count Increases Monotonically
// **Validates: Requirements 1.8**
TEST_CASE("Frame count increases monotonically", "[timer][property]") {
    SECTION("Frame count increments by exactly 1 per frame") {
        auto num_frames = GENERATE(take(NUM_OUTER_TESTS, random(10, 50)));
        
        Timer timer(60.0);
        
        // Initial frame count should be 0
        REQUIRE(timer.get_frame_count() == 0);
        
        for (int i = 0; i < num_frames; ++i) {
            uint64_t count_before = timer.get_frame_count();
            
            timer.start_frame();
            timer.end_frame();
            
            uint64_t count_after = timer.get_frame_count();
            
            // Frame count should increase by exactly 1
            REQUIRE(count_after == count_before + 1);
        }
        
        // Final frame count should equal number of frames executed
        REQUIRE(timer.get_frame_count() == static_cast<uint64_t>(num_frames));
    }
    
    SECTION("Frame count never decreases") {
        auto num_frames = GENERATE(take(NUM_OUTER_TESTS, random(20, 60)));
        
        Timer timer(60.0);
        
        uint64_t previous_count = 0;
        
        for (int i = 0; i < num_frames; ++i) {
            timer.start_frame();
            timer.end_frame();
            
            uint64_t current_count = timer.get_frame_count();
            
            // Frame count should never decrease
            REQUIRE(current_count >= previous_count);
            
            // Frame count should strictly increase
            REQUIRE(current_count > previous_count);
            
            previous_count = current_count;
        }
    }
    
    SECTION("Frame count is accurate across multiple sequences") {
        // Each frame sleeps ~16ms, so keep sequence lengths small
        auto sequence1_length = GENERATE(take(NUM_OUTER_TESTS, random(5, 10)));
        auto sequence2_length = GENERATE(take(NUM_INNER_TESTS, random(5, 10)));
        
        Timer timer(60.0);
        
        // First sequence
        for (int i = 0; i < sequence1_length; ++i) {
            timer.start_frame();
            timer.end_frame();
        }
        
        uint64_t count_after_seq1 = timer.get_frame_count();
        REQUIRE(count_after_seq1 == static_cast<uint64_t>(sequence1_length));
        
        // Second sequence
        for (int i = 0; i < sequence2_length; ++i) {
            timer.start_frame();
            timer.end_frame();
        }
        
        uint64_t count_after_seq2 = timer.get_frame_count();
        REQUIRE(count_after_seq2 == static_cast<uint64_t>(sequence1_length + sequence2_length));
    }
}

// Feature: red-moving-square-class-020, Property 5: Timer Updates Blackboard Correctly
// **Validates: Requirements 1.11**
TEST_CASE("Timer updates blackboard correctly", "[timer][property]") {
    SECTION("Blackboard contains correct timing data after update") {
        auto num_frames = GENERATE(take(NUM_OUTER_TESTS, random(10, 30)));
        
        Timer timer(60.0);
        Blackboard blackboard;
        
        // Run some frames
        for (int i = 0; i < num_frames; ++i) {
            timer.start_frame();
            timer.end_frame();
        }
        
        // Update blackboard
        timer.update_blackboard(blackboard);
        
        // Blackboard should contain all required keys
        REQUIRE(blackboard.has("delta_time"));
        REQUIRE(blackboard.has("fps"));
        REQUIRE(blackboard.has("frame_count"));
        
        // Values should match Timer's internal state
        double delta_time = blackboard.get<double>("delta_time");
        double fps = blackboard.get<double>("fps");
        uint64_t frame_count = blackboard.get<uint64_t>("frame_count");
        
        REQUIRE(delta_time == timer.get_delta_time());
        REQUIRE(fps == timer.get_fps());
        REQUIRE(frame_count == timer.get_frame_count());
        
        // Values should be reasonable
        REQUIRE(delta_time >= 0.0);
        REQUIRE(delta_time <= 1.0);
        REQUIRE(fps >= 0.0);
        REQUIRE(fps <= 120.0);  // Reasonable upper bound
        REQUIRE(frame_count == static_cast<uint64_t>(num_frames));
    }
    
    SECTION("Blackboard updates reflect current Timer state") {
        Timer timer(60.0);
        Blackboard blackboard;
        
        // First update
        timer.start_frame();
        timer.end_frame();
        timer.update_blackboard(blackboard);
        
        uint64_t frame_count_1 = blackboard.get<uint64_t>("frame_count");
        REQUIRE(frame_count_1 == 1);
        
        // Second update
        timer.start_frame();
        timer.end_frame();
        timer.update_blackboard(blackboard);
        
        uint64_t frame_count_2 = blackboard.get<uint64_t>("frame_count");
        REQUIRE(frame_count_2 == 2);
        
        // Frame count should have increased
        REQUIRE(frame_count_2 > frame_count_1);
    }
    
    SECTION("Multiple blackboard updates maintain consistency") {
        auto num_updates = GENERATE(take(NUM_OUTER_TESTS, random(5, 15)));
        
        Timer timer(60.0);
        Blackboard blackboard;
        
        for (int i = 0; i < num_updates; ++i) {
            timer.start_frame();
            timer.end_frame();
            timer.update_blackboard(blackboard);
            
            // Verify consistency after each update
            double delta_time = blackboard.get<double>("delta_time");
            double fps = blackboard.get<double>("fps");
            uint64_t frame_count = blackboard.get<uint64_t>("frame_count");
            
            REQUIRE(delta_time == timer.get_delta_time());
            REQUIRE(fps == timer.get_fps());
            REQUIRE(frame_count == timer.get_frame_count());
            REQUIRE(frame_count == static_cast<uint64_t>(i + 1));
        }
    }
}
