#include "projectile_hit_system.hpp"
#include "tower_components.hpp"   // ProjectileTag, ProjectileData, DamageEvent
#include "enemy_components.hpp"   // EnemyTag

// Arena shooter hit detection: projectiles fly straight (moved by the engine
// MovementSystem) and the engine CollisionSystem reports overlaps via
// CollidedWith. For each projectile that overlapped an enemy this frame, queue a
// DamageEvent on that enemy and destroy the projectile (one enemy per shot).
void ProjectileHitSystem::update(EntityManager& entity_manager,
                                 ComponentStorage& component_storage) {
    auto projectiles = component_storage.entities_with_component<ProjectileTag>();

    for (Entity proj : projectiles) {
        if (component_storage.has_component<DestroyRequest>(proj)) continue;

        auto collided = component_storage.get_component<CollidedWith>(proj);
        auto data_opt = component_storage.get_component<ProjectileData>(proj);
        if (!collided.has_value() || !data_opt.has_value()) continue;

        const ProjectileData& data = data_opt->get();

        for (Entity other : collided->get().entities) {
            // Only enemies take projectile damage.
            if (!component_storage.has_component<EnemyTag>(other)) continue;
            if (!entity_manager.is_alive(other)) continue;

            Entity event_entity = entity_manager.create_entity();
            component_storage.add_component<DamageEvent>(
                event_entity, DamageEvent{other, data.damage});
            component_storage.add_component<DestroyRequest>(proj, DestroyRequest{});
            break;  // one hit per projectile
        }
    }
}
