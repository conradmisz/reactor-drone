#ifndef PROJECTILE_HIT_SYSTEM_HPP
#define PROJECTILE_HIT_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"

/**
 * ProjectileHitSystem — detects projectile arrival and creates DamageEvent.
 *
 * For each projectile within HIT_THRESHOLD (5.0 px) of target:
 *   - Creates a DamageEvent entity (target, damage)
 *   - Attaches DestroyRequest to projectile
 *
 * If target is dead or lacks Position:
 *   - Attaches DestroyRequest to projectile without creating DamageEvent
 */
class ProjectileHitSystem {
public:
    void update(EntityManager& entity_manager,
                ComponentStorage& component_storage);
};

#endif // PROJECTILE_HIT_SYSTEM_HPP
