/**
 * CameraControlSystem implementation
 *
 * Reads keyboard state each frame and updates camera Blackboard values:
 * - camera.zoom:     modified by +/- keys, clamped to [MIN_ZOOM, MAX_ZOOM]
 * - camera.lookat.x: modified by left/right arrow keys
 * - camera.lookat.y: modified by up/down arrow keys
 *
 * Pan speed is inversely proportional to zoom so that visual panning speed
 * feels consistent regardless of zoom level.
 *
 * Requirements: REQ-1, REQ-2, REQ-3, REQ-4, REQ-5, REQ-6, REQ-7
 */

#include "engine/ecs/systems/camera_control_system.hpp"

#include <algorithm>  // std::clamp
#include <SDL3/SDL.h>

// Camera control constants (REQ-7)
constexpr float ZOOM_SPEED = 1.0f;   // zoom units per second
constexpr float PAN_SPEED  = 300.0f; // world units per second (at zoom 1.0)
constexpr float MIN_ZOOM   = 0.25f;  // minimum zoom (4x viewport)
constexpr float MAX_ZOOM   = 4.0f;   // maximum zoom (1/16 viewport)

void apply_camera_controls(Blackboard& blackboard, const CameraInput& input) {
    // Helper: read a float from Blackboard, handling double→float conversion
    // (tests may store values as double; game code stores as float)
    auto read_float = [&blackboard](const std::string& key, float default_val) -> float {
        if (!blackboard.has(key)) return default_val;
        try { return blackboard.get<float>(key); }
        catch (...) {
            try { return static_cast<float>(blackboard.get<double>(key)); }
            catch (...) { return default_val; }
        }
    };

    // Read current state from Blackboard
    float dt       = read_float("delta_time", 0.0f);
    float zoom     = read_float("camera.zoom", 1.0f);
    float lookat_x = read_float("camera.lookat.x", 0.0f);
    float lookat_y = read_float("camera.lookat.y", 0.0f);

    // Zoom: + key zooms in, - key zooms out (REQ-1, REQ-2)
    if (input.zoom_in)  zoom += ZOOM_SPEED * dt;
    if (input.zoom_out) zoom -= ZOOM_SPEED * dt;

    // Clamp zoom to valid range (REQ-3)
    zoom = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);

    // Pan: arrow keys, speed inversely proportional to zoom (REQ-4, REQ-5)
    float effective_pan = PAN_SPEED * dt / zoom;
    if (input.pan_left)  lookat_x -= effective_pan;
    if (input.pan_right) lookat_x += effective_pan;
    if (input.pan_up)    lookat_y += effective_pan;
    if (input.pan_down)  lookat_y -= effective_pan;

    // Camera world bounds clamping (REQ-9)
    // Only activates when world.width exists on Blackboard and is > 0
    if (blackboard.has("world.width")) {
        float world_x = read_float("world.x", 0.0f);
        float world_y = read_float("world.y", 0.0f);
        float world_w = read_float("world.width", 0.0f);
        float world_h = read_float("world.height", 0.0f);

        if (world_w > 0.0f && world_h > 0.0f) {
            // Read window dimensions for viewport size calculation
            int win_w = 800;
            int win_h = 600;
            if (blackboard.has("window_width")) {
                try { win_w = blackboard.get<int>("window_width"); }
                catch (...) { }
            }
            if (blackboard.has("window_height")) {
                try { win_h = blackboard.get<int>("window_height"); }
                catch (...) { }
            }

            float half_view_w = (static_cast<float>(win_w) / zoom) / 2.0f;
            float half_view_h = (static_cast<float>(win_h) / zoom) / 2.0f;

            // X axis clamping
            if (world_w >= half_view_w * 2.0f) {
                // World wider than viewport: clamp edges
                lookat_x = std::clamp(lookat_x,
                    world_x + half_view_w,
                    world_x + world_w - half_view_w);
            } else {
                // World narrower than viewport: center camera on world
                lookat_x = world_x + world_w / 2.0f;
            }

            // Y axis clamping
            if (world_h >= half_view_h * 2.0f) {
                // World taller than viewport: clamp edges
                lookat_y = std::clamp(lookat_y,
                    world_y + half_view_h,
                    world_y + world_h - half_view_h);
            } else {
                // World shorter than viewport: center camera on world
                lookat_y = world_y + world_h / 2.0f;
            }
        }
    }

    // Write updated values back to Blackboard
    blackboard.set("camera.zoom", zoom);
    blackboard.set("camera.lookat.x", lookat_x);
    blackboard.set("camera.lookat.y", lookat_y);
}

void CameraControlSystem::update(Blackboard& blackboard) {
    const bool* keys = SDL_GetKeyboardState(nullptr);

    CameraInput input;
    input.zoom_in   = keys[SDL_SCANCODE_EQUALS];
    input.zoom_out  = keys[SDL_SCANCODE_MINUS];
    input.pan_left  = keys[SDL_SCANCODE_A];
    input.pan_right = keys[SDL_SCANCODE_D];
    input.pan_up    = keys[SDL_SCANCODE_W];
    input.pan_down  = keys[SDL_SCANCODE_S];

    apply_camera_controls(blackboard, input);
}
