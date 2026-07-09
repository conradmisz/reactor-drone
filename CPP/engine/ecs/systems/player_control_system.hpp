/**
 * PlayerControlSystem - System for converting Input component state to Velocity
 * 
 * This system demonstrates how player input controls entity movement in a game engine.
 * It reads Input component flags (arrow keys) and sets Velocity component values,
 * which the MovementSystem then uses to update positions.
 * 
 * Key Design Decisions:
 * - Velocity is reset to (0, 0) each frame, then set based on current input
 * - This ensures immediate response to key releases (no momentum)
 * - Multiple keys can be pressed simultaneously for diagonal movement
 * - Speed is configurable (default 200.0 pixels/second)
 * - Positive dy moves UP (bottom-left origin coordinate system)
 * 
 * Requirements: 6.1-6.8
 */

#ifndef PLAYER_CONTROL_SYSTEM_HPP
#define PLAYER_CONTROL_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"

/**
 * PlayerControlSystem class
 * 
 * Responsible for converting player input (Input component) into movement (Velocity component).
 * This system demonstrates the separation of concerns in ECS:
 * - InputSystem handles raw keyboard events → Input component
 * - PlayerControlSystem handles Input component → Velocity component
 * - MovementSystem handles Velocity component → Position component
 */
class PlayerControlSystem {
public:
    /**
     * Constructor
     * 
     * @param speed Movement speed in pixels per second (default 200.0)
     */
    explicit PlayerControlSystem(float speed = 200.0f);
    
    /**
     * Update all entity velocities based on input state
     * 
     * This method:
     * 1. Queries for all entities with both Input and Velocity components
     * 2. Resets velocity to (0, 0)
     * 3. Sets velocity based on input flags:
     *    - input.right: adds +speed to velocity.dx
     *    - input.left: adds -speed to velocity.dx
     *    - input.up: adds +speed to velocity.dy (moves UP in bottom-left origin)
     *    - input.down: adds -speed to velocity.dy (moves DOWN)
     * 4. Supports diagonal movement when multiple keys are pressed
     * 
     * @param storage Component storage to query for entities and update Velocity components
     */
    void update(ComponentStorage& storage);

private:
    float speed_;  // Movement speed in pixels per second
};

#endif // PLAYER_CONTROL_SYSTEM_HPP
