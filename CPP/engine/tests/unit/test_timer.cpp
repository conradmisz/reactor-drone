#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/timer.hpp"
#include "engine/ecs/blackboard.hpp"
#include <thread>
#include <chrono>

TEST_CASE("Timer initializes with correct target FPS", "[timer][unit]") {
    Timer timer(60.0);
    
    REQUIRE(timer.get_frame_count() == 0);
    REQUIRE(timer.get_delta_time() == 0.0);
}

TEST_CASE("Timer frame count increments", "[timer][unit]") {
    Timer timer(60.0);
    
    timer.start_frame();
    timer.end_frame();
    
    REQUIRE(timer.get_frame_count() == 1);
    
    timer.start_frame();
    timer.end_frame();
    
    REQUIRE(timer.get_frame_count() == 2);
}

TEST_CASE("Timer tracks delta time", "[timer][unit]") {
    Timer timer(60.0);
    
    timer.start_frame();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    timer.end_frame();
    
    double delta = timer.get_delta_time();
    
    // Delta time should be at least 10ms (0.01s) but less than 100ms (0.1s)
    REQUIRE(delta >= 0.01);
    REQUIRE(delta < 0.1);
}

TEST_CASE("Timer updates blackboard correctly", "[timer][unit]") {
    Timer timer(60.0);
    Blackboard blackboard;
    
    timer.start_frame();
    timer.end_frame();
    timer.update_blackboard(blackboard);
    
    REQUIRE(blackboard.has("delta_time"));
    REQUIRE(blackboard.has("fps"));
    REQUIRE(blackboard.has("frame_count"));
    
    double delta_time = blackboard.get<double>("delta_time");
    double fps = blackboard.get<double>("fps");
    uint64_t frame_count = blackboard.get<uint64_t>("frame_count");
    
    REQUIRE(delta_time >= 0.0);
    REQUIRE(fps > 0.0);
    REQUIRE(frame_count == 1);
}

TEST_CASE("Timer calculates FPS over multiple frames", "[timer][unit]") {
    Timer timer(60.0);
    
    // Run several frames
    for (int i = 0; i < 10; i++) {
        timer.start_frame();
        timer.end_frame();
    }
    
    double fps = timer.get_fps();
    
    // FPS should be reasonable (between 30 and 120)
    REQUIRE(fps > 30.0);
    REQUIRE(fps < 120.0);
}

TEST_CASE("Timer maintains target frame rate", "[timer][unit]") {
    Timer timer(60.0);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Run 10 frames
    for (int i = 0; i < 10; i++) {
        timer.start_frame();
        timer.end_frame();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double>(end - start).count();
    
    // 10 frames at 60 FPS should take approximately 10/60 = 0.167 seconds
    // Allow generous tolerance for loaded systems (0.14 to 0.30 seconds)
    REQUIRE(duration >= 0.14);
    REQUIRE(duration <= 0.30);
}

// Edge case tests

TEST_CASE("Timer initialization with different target FPS values", "[timer][unit][edge]") {
    SECTION("Standard 60 FPS") {
        Timer timer(60.0);
        REQUIRE(timer.get_frame_count() == 0);
        REQUIRE(timer.get_delta_time() == 0.0);
    }
    
    SECTION("30 FPS") {
        Timer timer(30.0);
        REQUIRE(timer.get_frame_count() == 0);
        REQUIRE(timer.get_delta_time() == 0.0);
    }
    
    SECTION("120 FPS") {
        Timer timer(120.0);
        REQUIRE(timer.get_frame_count() == 0);
        REQUIRE(timer.get_delta_time() == 0.0);
    }
}

TEST_CASE("Timer frame_count starts at 0 and increments correctly", "[timer][unit][edge]") {
    Timer timer(60.0);
    
    SECTION("Initial frame count is 0") {
        REQUIRE(timer.get_frame_count() == 0);
    }
    
    SECTION("Frame count increments after each frame") {
        for (uint64_t i = 1; i <= 100; i++) {
            timer.start_frame();
            timer.end_frame();
            REQUIRE(timer.get_frame_count() == i);
        }
    }
    
    SECTION("Frame count persists across blackboard updates") {
        Blackboard blackboard;
        
        timer.start_frame();
        timer.end_frame();
        timer.update_blackboard(blackboard);
        REQUIRE(blackboard.get<uint64_t>("frame_count") == 1);
        
        timer.start_frame();
        timer.end_frame();
        timer.update_blackboard(blackboard);
        REQUIRE(blackboard.get<uint64_t>("frame_count") == 2);
    }
}

TEST_CASE("Timer handles frame overrun gracefully", "[timer][unit][edge]") {
    Timer timer(60.0);  // Target: 16.67ms per frame
    
    SECTION("Frame taking longer than target still completes") {
        timer.start_frame();
        // Simulate work that takes longer than target frame time
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // 50ms > 16.67ms
        timer.end_frame();
        
        // Frame should complete without hanging
        REQUIRE(timer.get_frame_count() == 1);
        
        // Delta time should reflect the actual work time
        double delta = timer.get_delta_time();
        REQUIRE(delta >= 0.05);  // At least 50ms
        REQUIRE(delta < 0.1);    // But not excessively large
    }
    
    SECTION("Multiple overrun frames don't accumulate errors") {
        for (int i = 0; i < 5; i++) {
            timer.start_frame();
            std::this_thread::sleep_for(std::chrono::milliseconds(30));  // 30ms > 16.67ms
            timer.end_frame();
        }
        
        REQUIRE(timer.get_frame_count() == 5);
        
        // FPS should reflect the slower frame rate
        double fps = timer.get_fps();
        REQUIRE(fps < 60.0);  // Running slower than target
        REQUIRE(fps > 20.0);  // But still reasonable
    }
    
    SECTION("Timer recovers after overrun frames") {
        // First, cause an overrun
        timer.start_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        timer.end_frame();
        
        // Then run normal frames
        for (int i = 0; i < 10; i++) {
            timer.start_frame();
            timer.end_frame();
        }
        
        // Timer should recover and maintain target FPS
        double fps = timer.get_fps();
        REQUIRE(fps > 30.0);
        REQUIRE(fps < 120.0);
    }
}

TEST_CASE("Timer handles extremely long frames", "[timer][unit][edge]") {
    Timer timer(60.0);
    
    SECTION("Frame longer than 1 second is clamped") {
        timer.start_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));  // 1.5 seconds
        timer.end_frame();
        
        // Delta time should be clamped to prevent physics explosions
        double delta = timer.get_delta_time();
        REQUIRE(delta <= 1.0);  // Clamped to max 1 second
        REQUIRE(delta >= 1.0);  // Should be at the clamp value
    }
}

TEST_CASE("Timer delta_time accuracy", "[timer][unit][edge]") {
    Timer timer(60.0);
    
    SECTION("Zero-work frame has minimal delta time") {
        timer.start_frame();
        timer.end_frame();
        
        double delta = timer.get_delta_time();
        REQUIRE(delta >= 0.0);
        REQUIRE(delta < 0.1);  // Should be very small
    }
    
    SECTION("Delta time reflects actual work time") {
        timer.start_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        timer.end_frame();
        
        double delta = timer.get_delta_time();
        REQUIRE(delta >= 0.01);  // At least 10ms
        REQUIRE(delta < 0.05);   // But not much more
    }
}

TEST_CASE("Timer deterministic mode reports fixed delta_time", "[timer][unit]") {
    Timer timer(60.0);
    const double expected = 1.0 / 60.0;

    timer.set_deterministic(true);
    REQUIRE(timer.get_delta_time() == Catch::Approx(expected));

    timer.start_frame();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    timer.end_frame();
    REQUIRE(timer.get_delta_time() == Catch::Approx(expected));
}

TEST_CASE("Timer FPS calculation stability", "[timer][unit][edge]") {
    Timer timer(60.0);
    
    SECTION("FPS is reasonable after first frame") {
        timer.start_frame();
        timer.end_frame();
        
        double fps = timer.get_fps();
        REQUIRE(fps > 0.0);
        REQUIRE(fps < 1000.0);  // Sanity check
    }
    
    SECTION("FPS stabilizes over multiple frames") {
        // Run enough frames to fill the circular buffer
        for (int i = 0; i < 60; i++) {
            timer.start_frame();
            timer.end_frame();
        }
        
        double fps = timer.get_fps();
        REQUIRE(fps > 30.0);
        REQUIRE(fps < 120.0);
    }
}
