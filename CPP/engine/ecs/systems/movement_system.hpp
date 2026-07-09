/**
 * MovementSystem - System for updating entity positions based on velocity
 * 
 * This system demonstrates frame-rate independent movement in game engines.
 * By using delta_time (time elapsed since previous frame), movement speed
 * remains consistent regardless of frame rate.
 * 
 * Key Design Decisions:
 * - Movement is frame-rate independent using the formula: position += velocity * delta_time
 * - Delta_time is read from the Blackboard (global state)
 * - Only entities with both Position and Velocity components are updated
 * - Boundary checking is optional (only applied if entity has Size component)
 * 
 * Requirements: 5.1-5.7, 8.1-8.6
 */

#ifndef MOVEMENT_SYSTEM_HPP
#define MOVEMENT_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * MovementSystem class
 * 
 * Responsible for updating entity positions based on their velocities.
 * This system demonstrates how ECS separates movement logic (system)
 * from position and velocity data (components).
 */
class MovementSystem {
public:
    /**
     * Update all entity positions based on velocity and delta_time
     * 
     * This method:
     * 1. Reads delta_time from the Blackboard
     * 2. Queries for all entities with both Position and Velocity components
     * 3. Updates position using: pos.x += vel.dx * dt, pos.y += vel.dy * dt
     * 4. If entity has Size component, applies boundary checking
     * 
     * @param storage Component storage to query for entities and update Position components
     * @param blackboard Blackboard to read delta_time and window dimensions
     * @throws std::runtime_error if delta_time key is missing from blackboard
     * @throws std::runtime_error if window dimensions are missing when boundary checking is needed
     */
    void update(ComponentStorage& storage, const Blackboard& blackboard);

private:
    /**
     * Apply boundary checking to keep entity within world bounds
     * 
     * Clamps position to ensure the entity stays fully within the world:
     * - position.x is clamped to [world_x, world_x + world_w - size.width]
     * - position.y is clamped to [world_y, world_y + world_h - size.height]
     * 
     * Uses bottom-left origin coordinates where position is the entity's
     * bottom-left corner.
     * 
     * @param pos Position component to clamp
     * @param size Size component for entity dimensions
     * @param world_x World left edge X coordinate
     * @param world_y World bottom edge Y coordinate
     * @param world_w World width
     * @param world_h World height
     */
    void apply_boundary_checking(Position& pos, const Size& size,
                                 float world_x, float world_y,
                                 float world_w, float world_h);
};

#endif // MOVEMENT_SYSTEM_HPP
