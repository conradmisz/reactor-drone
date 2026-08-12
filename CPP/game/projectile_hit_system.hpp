#ifndef PROJECTILE_HIT_SYSTEM_HPP
#define PROJECTILE_HIT_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "arena_config.hpp"   // ArenaConfig (ring centre + radius, for ricochets)

/**
 * ProjectileHitSystem — detects projectile arrival and creates DamageEvent.
 *
 * For each projectile within HIT_THRESHOLD (5.0 px) of target:
 *   - Creates a DamageEvent entity (target, damage)
 *   - Attaches DestroyRequest to projectile
 *   - v2: flashes the struck enemy white (Flash component)
 *
 * If target is dead or lacks Position:
 *   - Attaches DestroyRequest to projectile without creating DamageEvent
 */
class ProjectileHitSystem {
public:
    void update(EntityManager& entity_manager,
                ComponentStorage& component_storage,
                Blackboard& blackboard);

    /// The boundary ring a ricocheting shot reflects off (D98). The ring is a
    /// clamp rather than a collider, so it cannot arrive through CollidedWith.
    /// Never set = no boundary bounce; obstacles still work.
    void set_arena(const ArenaConfig* arena) { arena_ = arena; }

private:
    const ArenaConfig* arena_ = nullptr;
};

#endif // PROJECTILE_HIT_SYSTEM_HPP
