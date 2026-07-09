#ifndef ROTATION_SYSTEM_HPP
#define ROTATION_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * Updates the angle of every entity with a Rotation component each frame
 * by applying: angle += angular_velocity * delta_time.
 *
 * Reads: Rotation, delta_time from Blackboard
 * Writes: Rotation::angle
 */
class RotationSystem {
public:
    void update(ComponentStorage& storage, const Blackboard& blackboard);
};

#endif // ROTATION_SYSTEM_HPP
