#include "projectile_hit_system.hpp"
#include "tower_components.hpp"   // ProjectileTag, ProjectileData, DamageEvent
#include "enemy_components.hpp"   // EnemyTag
#include "player_components.hpp"  // Flash

// Arena shooter hit detection: projectiles fly straight (moved by the engine
// MovementSystem) and the engine CollisionSystem reports overlaps via
// CollidedWith. For each projectile that overlapped an enemy this frame, queue a
// DamageEvent on that enemy and destroy the projectile (one enemy per shot).
void ProjectileHitSystem::update(EntityManager& entity_manager,
                                 ComponentStorage& component_storage,
                                 const Blackboard& blackboard) {
    const float fdur = blackboard.get_or<float>("fb.flash_duration", 0.12f);
    const uint8_t fr = static_cast<uint8_t>(blackboard.get_or<int>("fb.enemy_flash_r", 255));
    const uint8_t fg = static_cast<uint8_t>(blackboard.get_or<int>("fb.enemy_flash_g", 255));
    const uint8_t fb = static_cast<uint8_t>(blackboard.get_or<int>("fb.enemy_flash_b", 255));

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

            // v2 Phase 4: flash the struck enemy (overwrites any in-flight flash,
            // resetting its clock — repeated hits keep it lit).
            component_storage.add_component<Flash>(other, Flash{fdur, fdur, fr, fg, fb});

            // v2: additive impact spark burst at the projectile's position.
            if (auto ppos = component_storage.get_component<Position>(proj); ppos.has_value()) {
                Entity burst = entity_manager.create_entity();
                component_storage.add_component<Position>(burst,
                    Position{ppos->get().x, ppos->get().y});
                ParticleEmitter e;
                e.shape = EmitterShape::Point;
                e.additive = true;
                e.emission_rate = 400.0f;
                e.particle_lifetime = 0.25f;
                e.min_speed = 40.0f; e.max_speed = 160.0f;
                e.cone_half_angle = 180.0f;
                e.start_r = 180; e.start_g = 245; e.start_b = 255; e.start_a = 255;
                e.end_r = 40;    e.end_g = 90;    e.end_b = 160;   e.end_a = 0;
                e.start_size = 5.0f; e.end_size = 0.0f;
                component_storage.add_component<ParticleEmitter>(burst, e);
                component_storage.add_component<Lifetime>(burst, Lifetime{0.06f});
            }

            component_storage.add_component<DestroyRequest>(proj, DestroyRequest{});
            break;  // one hit per projectile
        }
    }
}
