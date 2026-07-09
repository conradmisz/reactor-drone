#include "movement_system.hpp"
#include <algorithm>
#include <stdexcept>
#include <cmath>

void MovementSystem::update(ComponentStorage& storage, const Blackboard& blackboard) {
    // Read delta_time from blackboard
    double delta_time;
    try {
        delta_time = blackboard.get<double>("delta_time");
    } catch (const std::exception& e) {
        throw std::runtime_error("MovementSystem: delta_time not found in blackboard");
    }
    
    // Clamp delta_time to reasonable range [0.0, 1.0] to handle edge cases
    if (std::isnan(delta_time) || std::isinf(delta_time) || delta_time < 0.0) {
        delta_time = 0.0;
    } else if (delta_time > 1.0) {
        delta_time = 1.0;
    }
    
    // Get all entities with Velocity components
    auto entities = storage.entities_with_component<Velocity>();
    
    // Update position for each entity that has both Position and Velocity
    for (Entity entity : entities) {
        // Check if entity has Position component
        auto pos_opt = storage.get_component<Position>(entity);
        if (!pos_opt.has_value()) {
            continue;  // Skip entities without Position
        }
        
        // Get references to Position and Velocity
        Position& pos = pos_opt->get();
        auto vel_opt = storage.get_component<Velocity>(entity);
        if (!vel_opt.has_value()) {
            continue;  // Should not happen, but be safe
        }
        const Velocity& vel = vel_opt->get();
        
        // Check for NaN or infinity in position or velocity
        if (std::isnan(pos.x) || std::isinf(pos.x)) {
            pos.x = 0.0f;
        }
        if (std::isnan(pos.y) || std::isinf(pos.y)) {
            pos.y = 0.0f;
        }
        if (std::isnan(vel.dx) || std::isinf(vel.dx) || std::isnan(vel.dy) || std::isinf(vel.dy)) {
            continue;  // Skip update if velocity is invalid
        }
        
        // Apply frame-rate independent movement
        // Formula: position += velocity * delta_time
        pos.x += vel.dx * static_cast<float>(delta_time);
        pos.y += vel.dy * static_cast<float>(delta_time);
        
        // Apply boundary checking if entity has Size component and world bounds exist
        auto size_opt = storage.get_component<Size>(entity);
        if (size_opt.has_value()) {
            const Size& size = size_opt->get();
            
            // Skip clamping for WrapAround entities — WrapSystem handles them
            if (storage.has_component<WrapAround>(entity)) {
                continue;
            }
            
            // Read world bounds from blackboard (set by gamedata_loader)
            float world_x = blackboard.get_or<float>("world.x", 0.0f);
            float world_y = blackboard.get_or<float>("world.y", 0.0f);
            float world_w = blackboard.get_or<float>("world.width", 0.0f);
            float world_h = blackboard.get_or<float>("world.height", 0.0f);

            if (world_w > 0.0f && world_h > 0.0f) {
                apply_boundary_checking(pos, size, world_x, world_y, world_w, world_h);
            }
        }
    }
}

void MovementSystem::apply_boundary_checking(Position& pos, const Size& size,
                                            float world_x, float world_y,
                                            float world_w, float world_h) {
    // This function operates in WORLD coordinates (not screen coordinates).
    // The CameraSystem later transforms world → screen; we don't touch that here.
    //
    // Entity position is its bottom-left corner in world space.
    // We clamp so the entire entity stays inside the world rectangle.
    //
    // Example with our 800×600 world centered at origin:
    //   world_x = -400, world_y = -300, world_w = 800, world_h = 600
    //   For a 64×64 player:
    //     min_x = -400          (left edge of world)
    //     max_x = -400+800-64 = 336  (right edge minus player width)
    //     min_y = -300          (bottom edge of world)
    //     max_y = -300+600-64 = 236  (top edge minus player height)
    //   So the player's bottom-left corner ranges from (-400,-300) to (336,236),
    //   which keeps the player's right/top edges at (400,300) — exactly the world boundary.

    // Compute the allowed range for the entity's bottom-left corner
    float min_x = world_x;
    float max_x = world_x + world_w - size.width;
    float min_y = world_y;
    float max_y = world_y + world_h - size.height;

    // Clamp X: don't let the entity go past the left or right world edge
    if (pos.x < min_x) {
        pos.x = min_x;
    } else if (pos.x > max_x) {
        pos.x = max_x;
    }

    // Clamp Y: don't let the entity go past the bottom or top world edge
    if (pos.y < min_y) {
        pos.y = min_y;
    } else if (pos.y > max_y) {
        pos.y = max_y;
    }
}
