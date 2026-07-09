#include "enemy_death_system.hpp"
#include "enemy_components.hpp"   // EnemyTag, Health
#include "player_components.hpp"  // ContactDamage
#include "engine/project_paths.hpp"
#include <string>

const sidecar_loader::LoadedSprite* EnemyDeathSystem::effect_sprite() {
    if (effect_.has_value()) return &effect_.value();
    if (effect_failed_) return nullptr;
    try {
        std::string path = project_paths::assets_dir() + "/images/effect_explosion.json";
        effect_ = sidecar_loader::load(path, "expand");
        return &effect_.value();
    } catch (...) {
        effect_failed_ = true;  // fall back to no death sprite; game keeps running
        return nullptr;
    }
}

void EnemyDeathSystem::update(ComponentStorage& component_storage,
                              EntityManager& entity_manager,
                              Blackboard& blackboard) {
    int score = blackboard.get_or<int>("score", 0);
    int pending_xp = blackboard.get_or<int>("pending_xp", 0);

    for (Entity enemy : component_storage.entities_with_component<EnemyTag>()) {
        auto health_opt = component_storage.get_component<Health>(enemy);
        if (!health_opt.has_value()) continue;
        if (health_opt->get().current > 0.0f) continue;
        if (component_storage.has_component<DestroyRequest>(enemy)) continue;  // already dead

        // Reward: score + XP from the enemy's combat payload.
        auto cd = component_storage.get_component<ContactDamage>(enemy);
        if (cd.has_value()) {
            score += cd->get().score;
            pending_xp += cd->get().xp;
        } else {
            score += 10;
            pending_xp += 1;
        }

        // Spawn the one-shot explosion at the enemy's position.
        const sidecar_loader::LoadedSprite* fx = effect_sprite();
        if (fx != nullptr) {
            auto pos = component_storage.get_component<Position>(enemy);
            auto size = component_storage.get_component<Size>(enemy);
            if (pos.has_value()) {
                float w = size.has_value() ? size->get().width : 40.0f;
                float h = size.has_value() ? size->get().height : 40.0f;
                Entity effect = entity_manager.create_entity();
                component_storage.add_component<Position>(effect,
                    Position{pos->get().x, pos->get().y});
                component_storage.add_component<Size>(effect, Size{w, h});
                component_storage.add_component<SpriteSheet>(effect, fx->sprite_sheet);
                component_storage.add_component<Animation>(effect, fx->animation);
                // Total clip time = frame_count * frame_duration.
                float life = static_cast<float>(fx->animation.frame_count) *
                             fx->animation.frame_duration;
                component_storage.add_component<Lifetime>(effect, Lifetime{life});
                component_storage.add_component<RenderLayer>(effect, RenderLayer{4});
            }
        }

        // v2: additive particle burst alongside the explosion clip. A short-lived
        // host with a high-rate radial emitter sprays for a moment, then is
        // destroyed (emitter dies with host; particles fade via their lifetime).
        if (auto pos = component_storage.get_component<Position>(enemy); pos.has_value()) {
            auto sz = component_storage.get_component<Size>(enemy);
            float w = sz.has_value() ? sz->get().width : 40.0f;
            float h = sz.has_value() ? sz->get().height : 40.0f;
            Entity burst = entity_manager.create_entity();
            component_storage.add_component<Position>(burst,
                Position{pos->get().x + w * 0.5f, pos->get().y + h * 0.5f});
            ParticleEmitter e;
            e.shape = EmitterShape::Point;
            e.additive = true;
            e.emission_rate = 500.0f;
            e.particle_lifetime = 0.45f;
            e.min_speed = 60.0f; e.max_speed = 240.0f;
            e.cone_half_angle = 180.0f;
            e.start_r = 255; e.start_g = 200; e.start_b = 120; e.start_a = 255;
            e.end_r = 255;   e.end_g = 60;    e.end_b = 30;    e.end_a = 0;
            e.start_size = 7.0f; e.end_size = 0.0f;
            component_storage.add_component<ParticleEmitter>(burst, e);
            component_storage.add_component<Lifetime>(burst, Lifetime{0.10f});
        }

        component_storage.add_component<DestroyRequest>(enemy, DestroyRequest{});
    }

    blackboard.set("score", score);
    blackboard.set("pending_xp", pending_xp);
}
