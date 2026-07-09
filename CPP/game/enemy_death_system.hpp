#ifndef ENEMY_DEATH_SYSTEM_HPP
#define ENEMY_DEATH_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/sidecar_loader.hpp"
#include <optional>

/**
 * EnemyDeathSystem — turns dead enemies (Health <= 0) into score, XP, and a
 * short death animation, then marks them for destruction.
 *
 * Per kill: adds the enemy's ContactDamage.score to Blackboard "score", adds its
 * ContactDamage.xp to Blackboard "pending_xp" (consumed by ExperienceSystem),
 * spawns a one-shot explosion sprite with a matching Lifetime at the enemy's
 * position, and attaches a DestroyRequest. Double-reward is prevented by
 * skipping enemies already marked for destruction.
 */
class EnemyDeathSystem {
public:
    void update(ComponentStorage& component_storage,
                EntityManager& entity_manager,
                Blackboard& blackboard);

private:
    // Explosion sprite, loaded once on first death.
    std::optional<sidecar_loader::LoadedSprite> effect_;
    bool effect_failed_ = false;
    const sidecar_loader::LoadedSprite* effect_sprite();
};

#endif // ENEMY_DEATH_SYSTEM_HPP
