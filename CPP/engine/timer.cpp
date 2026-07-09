#include "timer.hpp"
#include "ecs/blackboard.hpp"
#include <stdexcept>
#include <algorithm>
#include <cmath>

Timer::Timer(double target_fps)
    : target_fps_(target_fps)
    , target_frame_time_(1.0 / target_fps)
    , frame_start_time_(0.0)
    , delta_time_(0.0)  // Zero before any frame is processed
    , sleep_error_(0.0)
    , frame_index_(0)
    , frame_count_(0)
{
    // Initialize circular buffer with target frame time
    frame_times_.fill(target_frame_time_);
}

void Timer::start_frame() {
    frame_start_time_ = get_current_time();
}

void Timer::end_frame_internal() {
    double frame_end = get_current_time();
    double frame_elapsed = frame_end - frame_start_time_;
    
    // Handle negative delta time (clock went backward)
    if (frame_elapsed < 0.0) {
        frame_elapsed = 0.0;
        // In a production system, we would log a warning here
    }
    
    // Handle excessively large delta time (> 1 second suggests clock issue or debugger pause)
    if (frame_elapsed > 1.0) {
        frame_elapsed = std::min(frame_elapsed, 1.0);
        // In a production system, we would log a warning here
    }
    
    // Sleep + busy-wait to maintain target FPS with adaptive error correction
    double time_left = target_frame_time_ - frame_elapsed - sleep_error_;

    if (time_left > 0.0) {
        // Sleep burns most of the idle time cheaply. On Windows the default
        // timer resolution is ~15.6ms, so sleep_for can overshoot by that
        // much. Only sleep when there is enough headroom that an overshoot
        // won't blow past the frame budget.
        constexpr double SLEEP_THRESHOLD = 0.002;  // 2ms — safe on all platforms
        double sleep_requested = time_left * 0.5;  // sleep half, busy-wait the rest

        if (sleep_requested > SLEEP_THRESHOLD) {
            double before_sleep = get_current_time();
            std::this_thread::sleep_for(
                std::chrono::duration<double>(sleep_requested)
            );
            double actual_sleep = get_current_time() - before_sleep;

            // sleep_for() nearly always overshoots by some amount due to OS
            // scheduling. Record that overshoot so the NEXT frame asks for
            // less sleep up front. Damp by 50% to avoid oscillation.
            double overshoot = actual_sleep - sleep_requested;
            sleep_error_ = std::max(0.0, sleep_error_ + overshoot * 0.5);
        }

        // Busy-wait for precise timing — this never overshoots the target.
        while (get_current_time() - frame_start_time_ < target_frame_time_) {
            std::this_thread::yield();
        }
    } else {
        // Running behind: forget learned overshoot, it's not helping.
        sleep_error_ = 0.0;
    }
    
    // Calculate TOTAL frame time (including sleep) for delta_time
    // This is what gets used for frame-rate independent movement
    double total_frame_time = get_current_time() - frame_start_time_;
    delta_time_ = std::min(total_frame_time, 1.0);

    if (deterministic_) {
        delta_time_ = target_frame_time_;
    }

    // Store measured frame time for FPS (not the synthetic delta_time_).
    frame_times_[frame_index_] = total_frame_time;
    frame_index_ = (frame_index_ + 1) % frame_times_.size();
}

void Timer::end_frame() {
    end_frame_internal();
    frame_count_++;
}

void Timer::end_frame_no_advance() {
    end_frame_internal();
}

void Timer::set_deterministic(bool enabled) {
    deterministic_ = enabled;
    if (enabled) {
        delta_time_ = target_frame_time_;
    }
}

bool Timer::is_deterministic() const {
    return deterministic_;
}

void Timer::update_blackboard(Blackboard& blackboard) {
    blackboard.set("delta_time", delta_time_);
    blackboard.set("fps", get_fps());
    blackboard.set("frame_count", frame_count_);
}

double Timer::get_delta_time() const {
    return delta_time_;
}

double Timer::get_fps() const {
    // Calculate average frame time over last 60 frames
    double sum = 0.0;
    for (double ft : frame_times_) {
        sum += ft;
    }
    double avg_frame_time = sum / frame_times_.size();
    
    // Avoid division by zero
    if (avg_frame_time <= 0.0) {
        return 0.0;
    }
    
    return 1.0 / avg_frame_time;
}

uint64_t Timer::get_frame_count() const {
    return frame_count_;
}

double Timer::get_target_fps() const {
    return target_fps_;
}

double Timer::get_current_time() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration<double>(duration).count();
}
