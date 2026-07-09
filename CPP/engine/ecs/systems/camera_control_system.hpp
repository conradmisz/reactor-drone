/**
 * CameraControlSystem - System for keyboard-driven camera zoom and pan
 *
 * This system reads keyboard input and updates camera Blackboard values
 * (camera.zoom, camera.lookat.x, camera.lookat.y) each frame. It runs
 * before CameraSystem so the updated values are used for world-to-screen
 * transforms in the same frame.
 *
 * Design:
 * - CameraInput struct decouples input source from logic (testable without SDL)
 * - apply_camera_controls() free function implements all math (unit-testable)
 * - CameraControlSystem::update() maps SDL keyboard state → CameraInput
 *
 * Key mappings:
 *   +/=  → zoom in      -  → zoom out
 *   A    → pan left      D  → pan right
 *   W    → pan up        S  → pan down
 *
 * Requirements: REQ-1 through REQ-7
 */

#ifndef CAMERA_CONTROL_SYSTEM_HPP
#define CAMERA_CONTROL_SYSTEM_HPP

#include "engine/ecs/blackboard.hpp"

/**
 * CameraInput struct
 *
 * Pure-data representation of camera control intent for a single frame.
 * Decoupled from SDL so that apply_camera_controls() can be tested
 * without any windowing system.
 */
struct CameraInput {
    bool zoom_in   = false;
    bool zoom_out  = false;
    bool pan_left  = false;
    bool pan_right = false;
    bool pan_up    = false;
    bool pan_down  = false;
};

/**
 * Apply camera control input to the Blackboard.
 *
 * Reads delta_time, camera.zoom, camera.lookat.x, camera.lookat.y from
 * the Blackboard, applies zoom/pan deltas based on the CameraInput flags,
 * clamps zoom to [MIN_ZOOM, MAX_ZOOM], and writes the results back.
 *
 * @param blackboard  Game blackboard (read/write)
 * @param input       Camera input flags for this frame
 */
void apply_camera_controls(Blackboard& blackboard, const CameraInput& input);

/**
 * CameraControlSystem class
 *
 * Polls SDL keyboard state each frame, builds a CameraInput, and delegates
 * to apply_camera_controls(). Does NOT need ComponentStorage — it only
 * touches the Blackboard.
 */
class CameraControlSystem {
public:
    /**
     * Read SDL keyboard state and update camera Blackboard values.
     *
     * @param blackboard  Game blackboard (read/write)
     */
    void update(Blackboard& blackboard);
};

#endif // CAMERA_CONTROL_SYSTEM_HPP
