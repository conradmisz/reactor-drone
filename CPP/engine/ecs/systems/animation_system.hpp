#ifndef ANIMATION_SYSTEM_HPP
#define ANIMATION_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * Advances Animation.current_frame based on delta_time and writes the
 * result to SpriteSheet.current_frame for every entity that has both
 * components.
 *
 * Reads: Animation, SpriteSheet, delta_time from Blackboard
 * Writes: Animation (current_frame, elapsed, playing, finished),
 *         SpriteSheet::current_frame
 */
class AnimationSystem {
public:
    void update(ComponentStorage& storage, const Blackboard& blackboard);
};

#endif // ANIMATION_SYSTEM_HPP
