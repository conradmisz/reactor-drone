#ifndef PLAYER_AIM_SYSTEM_HPP
#define PLAYER_AIM_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * PlayerAimSystem — turns the player drone to face the mouse cursor.
 *
 * Reads the cursor's world position from the Blackboard ("mouse.x"/"mouse.y",
 * populated by the engine InputSystem) and writes the angle from the player's
 * center to the cursor into the player's Rotation.angle. The RenderSystem draws
 * the sprite at that angle, so the drone always points at the cursor.
 */
class PlayerAimSystem {
public:
    void update(ComponentStorage& storage, const Blackboard& blackboard);
};

#endif // PLAYER_AIM_SYSTEM_HPP
