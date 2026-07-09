#ifndef WRAP_SYSTEM_HPP
#define WRAP_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * Teleports entities with WrapAround to the opposite world edge
 * when they move fully beyond a boundary (toroidal wrapping).
 *
 * Reads: Position, Size (optional), WrapAround
 * Reads from Blackboard: world.x, world.y, world.width, world.height
 * Writes: Position
 */
class WrapSystem {
public:
    void update(ComponentStorage& storage, const Blackboard& blackboard);
};

#endif // WRAP_SYSTEM_HPP
