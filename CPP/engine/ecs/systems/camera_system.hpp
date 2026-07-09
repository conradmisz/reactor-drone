/**
 * CameraSystem - System for transforming world-space to screen-space coordinates
 *
 * This system implements the affine transform that converts world-space Position
 * components into screen-space ScreenPosition components. The camera is defined
 * by a look-at point and a zoom factor, both read from the Blackboard.
 *
 * World space uses a centered origin: (0,0) is the center of the world, with
 * positive X rightward and positive Y upward (Cartesian convention).
 * Screen space uses a bottom-left origin: (0,0) is the bottom-left of the window.
 *
 * The transform:
 *   cam_width  = window_width / zoom
 *   cam_height = window_height / zoom
 *   cam_left   = lookat.x - cam_width / 2
 *   cam_bottom = lookat.y - cam_height / 2
 *   screen_x   = (world_x - cam_left) * zoom
 *   screen_y   = (world_y - cam_bottom) * zoom
 *
 * The aspect ratio is enforced by construction — both view dimensions are derived
 * from a single zoom parameter, so the scale factor is identical on both axes.
 *
 * Key ECS Concepts:
 * - Reads Position + Size to determine eligible entities
 * - Writes ScreenPosition as the transform output
 * - Reads all configuration from the Blackboard (no constructor state)
 * - Entities without Position (e.g., HUD elements) are skipped
 */

#ifndef CAMERA_SYSTEM_HPP
#define CAMERA_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * CameraSystem class
 *
 * Responsible for computing the world-to-screen affine transform for all
 * entities that have both Position and Size components. The result is written
 * as a ScreenPosition component on each eligible entity.
 *
 * Camera configuration is read from the Blackboard each frame:
 * - camera.lookat.x (float, default 0.0f)
 * - camera.lookat.y (float, default 0.0f)
 * - camera.zoom     (float, default 1.0f, clamped to min 0.01f)
 * - window_width    (int, required)
 * - window_height   (int, required)
 */
class CameraSystem {
public:
    /**
     * Compute the world-to-screen transform for all eligible entities.
     *
     * Iterates entities with Position, skips those without Size, and writes
     * a ScreenPosition component with the transformed screen coordinates.
     *
     * @param storage  Component storage to query and write components
     * @param blackboard Blackboard containing camera config and window dimensions
     */
    void update(ComponentStorage& storage, const Blackboard& blackboard);
};

#endif // CAMERA_SYSTEM_HPP
