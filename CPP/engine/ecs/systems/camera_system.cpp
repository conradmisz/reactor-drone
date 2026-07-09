/**
 * CameraSystem implementation
 *
 * Transforms world-space Position to screen-space ScreenPosition using an
 * affine transform defined by a look-at point and zoom factor read from the
 * Blackboard.
 *
 * The transform:
 *   cam_width  = window_width / zoom
 *   cam_height = window_height / zoom
 *   cam_left   = lookat.x - cam_width / 2
 *   cam_bottom = lookat.y - cam_height / 2
 *   screen_x   = (world_x - cam_left) * zoom
 *   screen_y   = (world_y - cam_bottom) * zoom
 *
 * The aspect ratio is enforced by construction — both view dimensions are
 * derived from a single zoom parameter, so the scale factor is identical on
 * both axes.
 */

#include "engine/ecs/systems/camera_system.hpp"

#include <algorithm>

void CameraSystem::update(ComponentStorage& storage, const Blackboard& blackboard) {
    // Read camera configuration from Blackboard with defaults
    // Note: Values may be stored as float (from game code) or double (from tests),
    // so we try float first, then fall back to double with a cast.
    auto read_float = [&blackboard](const std::string& key, float default_val) -> float {
        if (!blackboard.has(key)) return default_val;
        try { return blackboard.get<float>(key); }
        catch (...) {
            try { return static_cast<float>(blackboard.get<double>(key)); }
            catch (...) { return default_val; }
        }
    };

    float lookat_x = read_float("camera.lookat.x", 0.0f);
    float lookat_y = read_float("camera.lookat.y", 0.0f);
    float zoom     = read_float("camera.zoom", 1.0f);

    // Clamp zoom to minimum 0.01f to prevent division by zero
    zoom = std::max(zoom, 0.01f);

    // Window dimensions (always set by main.cpp)
    int win_w = blackboard.get<int>("window_width");
    int win_h = blackboard.get<int>("window_height");

    // Derive view rectangle dimensions
    float cam_width  = static_cast<float>(win_w) / zoom;
    float cam_height = static_cast<float>(win_h) / zoom;

    // Compute view rectangle origin (bottom-left corner in world space)
    float cam_left   = lookat_x - cam_width  / 2.0f;
    float cam_bottom = lookat_y - cam_height / 2.0f;

    // Transform each entity that has both Position and Size
    auto entities = storage.entities_with_component<Position>();
    for (Entity entity : entities) {
        if (!storage.has_component<Size>(entity)) {
            continue;  // Skip entities without Size
        }

        auto pos_opt = storage.get_component<Position>(entity);
        if (!pos_opt.has_value()) {
            continue;
        }

        const auto& pos = pos_opt->get();

        // Affine transform: world → screen
        float screen_x = (pos.x - cam_left)   * zoom;
        float screen_y = (pos.y - cam_bottom)  * zoom;

        storage.add_component<ScreenPosition>(entity, ScreenPosition{screen_x, screen_y});
    }
}
