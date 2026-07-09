#include "damage_apply_system.hpp"
#include "tower_components.hpp"
#include "enemy_components.hpp"

void DamageApplySystem::update(EntityManager& entity_manager,
                               ComponentStorage& component_storage) {
    auto events = component_storage.entities_with_component<DamageEvent>();

    for (Entity event_entity : events) {
        auto event_opt = component_storage.get_component<DamageEvent>(event_entity);
        if (!event_opt.has_value()) continue;

        const DamageEvent& event = event_opt->get();

        // Mark event entity for destruction regardless of outcome
        component_storage.add_component<DestroyRequest>(event_entity, DestroyRequest{});

        // Verify target exists and has Health
        if (!entity_manager.is_alive(event.target_entity)) continue;
        if (!component_storage.has_component<Health>(event.target_entity)) continue;

        auto health_opt = component_storage.get_component<Health>(event.target_entity);
        if (!health_opt.has_value()) continue;

        Health& health = health_opt->get();

        // Compute effective damage with armor multiplier
        float armor_multiplier = health.armor_multiplier;
        float effective_damage = event.amount * armor_multiplier;

        // Apply damage, floor at zero
        health.current -= effective_damage;
        if (health.current < 0.0f) {
            health.current = 0.0f;
        }
    }
}
