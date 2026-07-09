#ifndef EXPERIENCE_SYSTEM_HPP
#define EXPERIENCE_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * ExperienceSystem — grants queued kill XP to the player and levels up.
 *
 * Consumes Blackboard "pending_xp" (accumulated by EnemyDeathSystem) into the
 * player's Experience.xp. While xp >= threshold it spends the threshold, raises
 * the level, and multiplies the threshold by Experience.multiplier — so several
 * levels can be gained from one big XP grant. Each level gained increments
 * Blackboard "pending_upgrades" (consumed by UpgradeSystem) and the current level
 * is published to Blackboard "level" for the HUD.
 */
class ExperienceSystem {
public:
    void update(ComponentStorage& storage, Blackboard& blackboard);
};

#endif // EXPERIENCE_SYSTEM_HPP
