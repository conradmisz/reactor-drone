#include "player_control_system.hpp"

PlayerControlSystem::PlayerControlSystem(float speed)
    : speed_(speed) {
}

void PlayerControlSystem::update(ComponentStorage& storage) {
    // Get all entities with Input components
    auto entities = storage.entities_with_component<Input>();
    
    // Update velocity for each entity that has both Input and Velocity
    for (Entity entity : entities) {
        // Check if entity has Velocity component
        auto vel_opt = storage.get_component<Velocity>(entity);
        if (!vel_opt.has_value()) {
            continue;  // Skip entities without Velocity
        }
        
        // Get references to Input and Velocity
        auto input_opt = storage.get_component<Input>(entity);
        if (!input_opt.has_value()) {
            continue;  // Should not happen, but be safe
        }
        
        const Input& input = input_opt->get();
        Velocity& vel = vel_opt->get();
        
        // Reset velocity to (0, 0) at start of each update
        // This ensures immediate response to key releases
        vel.dx = 0.0f;
        vel.dy = 0.0f;
        
        // Set velocity based on input flags
        // Multiple keys can be pressed simultaneously for diagonal movement
        
        // Horizontal movement
        if (input.right) {
            vel.dx += speed_;  // Move right (positive X)
        }
        if (input.left) {
            vel.dx -= speed_;  // Move left (negative X)
        }
        
        // Vertical movement (bottom-left origin: positive Y is UP)
        if (input.up) {
            vel.dy += speed_;  // Move up (positive Y)
        }
        if (input.down) {
            vel.dy -= speed_;  // Move down (negative Y)
        }

        // Normalize diagonals: without this, holding two axes gives a velocity of
        // magnitude speed_*sqrt(2), i.e. ~41% faster than moving cardinally.
        if (vel.dx != 0.0f && vel.dy != 0.0f) {
            constexpr float INV_SQRT2 = 0.70710678f;
            vel.dx *= INV_SQRT2;
            vel.dy *= INV_SQRT2;
        }
    }
}
