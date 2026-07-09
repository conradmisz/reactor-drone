#ifndef DAMAGE_APPLY_SYSTEM_HPP
#define DAMAGE_APPLY_SYSTEM_HPP

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"

/**
 * DamageApplySystem — processes all DamageEvent components and applies damage.
 *
 * Iterates entities with DamageEvent. For each event:
 * - Reads the target's Health component
 * - Computes effective damage = amount × armor_multiplier (default 1.0)
 * - Subtracts effective damage from Health.current, flooring at 0.0f
 * - Attaches DestroyRequest to the event entity (cleanup)
 *
 * If the target is dead or lacks Health, attaches DestroyRequest to the
 * event entity without applying damage.
 *
 * Single responsibility: centralized damage math and event cleanup.
 */
class DamageApplySystem {
public:
    void update(EntityManager& entity_manager,
                ComponentStorage& component_storage);
};

#endif // DAMAGE_APPLY_SYSTEM_HPP
