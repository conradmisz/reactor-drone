#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <thread>

// Forward declaration
class Blackboard;

/**
 * Timer class for maintaining target frame rate and tracking timing data.
 * 
 * Uses a hybrid sleep + busy-wait approach for precise frame timing:
 * 1. Sleep for (target_frame_time - elapsed - 1ms) to reduce CPU usage
 * 2. Busy-wait for the final 1ms to hit exact frame timing
 * 3. Track frame times in a circular buffer for FPS calculation
 */
class Timer {
public:
    /**
     * Construct a Timer with a target frame rate.
     * @param target_fps Target frames per second (default: 60.0)
     */
    explicit Timer(double target_fps = 60.0);
    
    /**
     * Mark the beginning of a frame.
     * Records the frame start time for timing calculations.
     */
    void start_frame();
    
    /**
     * Mark the end of a frame and maintain target FPS.
     * Performs sleep + busy-wait to maintain target frame rate.
     * Updates frame timing statistics.
     */
    void end_frame();
    
    /**
     * Write timing data to the Blackboard.
     * Writes: "delta_time" (double), "fps" (double), "frame_count" (uint64_t)
     * @param blackboard The Blackboard to write timing data to
     */
    void update_blackboard(Blackboard& blackboard);
    
    /**
     * Get the time elapsed since the previous frame.
     * @return Delta time in seconds
     */
    double get_delta_time() const;
    
    /**
     * Get the average frames per second over the last 60 frames.
     * @return Average FPS
     */
    double get_fps() const;
    
    /**
     * Get the total number of frames executed.
     * @return Frame count
     */
    uint64_t get_frame_count() const;

    /**
     * Get the configured target frame rate.
     * @return Target frames per second passed to the constructor
     */
    double get_target_fps() const;

    /**
     * Mark the end of a frame without advancing the frame counter.
     * Performs sleep + busy-wait to maintain target frame rate and
     * updates frame timing statistics, but does NOT increment frame_count_.
     * Used during debug pause to maintain frame pacing while freezing game time.
     */
    void end_frame_no_advance();

    /**
     * Enable or disable deterministic (fixed-timestep) mode.
     *
     * When enabled, reports constant delta_time equal to target_frame_time_
     * (1.0 / target_fps) instead of measured wall-clock frame time. Sleep and
     * busy-wait pacing are unchanged; only the value handed to the simulation
     * is pinned. Combined with a fixed RNG seed, runs are frame-for-frame
     * reproducible.
     *
     * @param enabled True to report constant delta_time; false for measured.
     */
    void set_deterministic(bool enabled);

    /**
     * @return True if deterministic (fixed-timestep) mode is enabled.
     */
    bool is_deterministic() const;

private:
    /**
     * Internal frame-end logic shared by end_frame() and end_frame_no_advance().
     * Performs sleep/busy-wait for frame pacing, delta_time calculation,
     * sleep error tracking, and FPS circular buffer update.
     */
    void end_frame_internal();
    double target_fps_;
    double target_frame_time_;
    double frame_start_time_;
    double delta_time_;
    double sleep_error_;  // Track sleep inaccuracy for adaptive correction
    bool deterministic_ = false;  // When true, report constant delta_time

    // FPS tracking
    std::array<double, 60> frame_times_;  // Circular buffer for last 60 frames
    size_t frame_index_;
    uint64_t frame_count_;
    
    /**
     * Get the current time in seconds since epoch.
     * @return Current time in seconds
     */
    double get_current_time() const;
};
