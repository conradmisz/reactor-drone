#include "enemy_death_system.hpp"
#include "enemy_components.hpp"   // EnemyTag, Health
#include "player_components.hpp"  // ContactDamage, Pickup
#include "feedback.hpp"           // add_trauma
#include "engine/project_paths.hpp"
#include <cmath>
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

void EnemyDeathSystem::drop_loot(ComponentStorage& component_storage,
                                 EntityManager& entity_manager,
                                 float cx, float cy, int currency_value) {
    const EconomyConfig& ec = economy_;
    const int lo = std::max(0, ec.min_drops);
    const int hi = std::max(lo, ec.max_drops);

    // R2: every draw below happens on every kill, in the same order, whatever the
    // outcome. `count` decides how many of the pre-rolled scatter offsets are
    // *used*, never how many are drawn.
    std::uniform_int_distribution<int> count_dist(lo, hi);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530717958647692f);

    const int count = count_dist(rng_);
    const float key_roll = unit(rng_);

    const float half = ec.pickup_size * 0.5f;
    auto make_pickup = [&](float x, float y, PickupKind kind, int value,
                           uint8_t r, uint8_t g, uint8_t b) {
        Entity e = entity_manager.create_entity();
        component_storage.add_component<Position>(e, Position{x - half, y - half});
        component_storage.add_component<Size>(e, Size{ec.pickup_size, ec.pickup_size});
        component_storage.add_component<Color>(e, Color{r, g, b, 255});
        component_storage.add_component<Pickup>(e,
            Pickup{static_cast<int>(kind), value, ec.pickup_magnet_speed});
        component_storage.add_component<Lifetime>(e, Lifetime{ec.pickup_lifetime});
        // Above enemies(2) and the player(3) so loot is never hidden under a corpse.
        component_storage.add_component<RenderLayer>(e, RenderLayer{4});
    };

    for (int i = 0; i < hi; ++i) {
        float a = angle_dist(rng_);
        float d = unit(rng_) * ec.pickup_scatter;
        if (i >= count) continue;   // rolled, not used — the draws still happened
        make_pickup(cx + std::cos(a) * d, cy + std::sin(a) * d,
                    PickupKind::Currency, currency_value, 255, 210, 90);
    }

    if (key_roll < ec.key_drop_chance) {
        make_pickup(cx, cy, PickupKind::Key, 1, 255, 130, 245);
    }
}

void EnemyDeathSystem::update(ComponentStorage& component_storage,
                              EntityManager& entity_manager,
                              Blackboard& blackboard) {
    int score = blackboard.get_or<int>("score", 0);

    for (Entity enemy : component_storage.entities_with_component<EnemyTag>()) {
        auto health_opt = component_storage.get_component<Health>(enemy);
        if (!health_opt.has_value()) continue;
        if (health_opt->get().current > 0.0f) continue;
        if (component_storage.has_component<DestroyRequest>(enemy)) continue;  // already dead

        // Reward: score now, currency as physical loot below (D5).
        auto cd = component_storage.get_component<ContactDamage>(enemy);
        int currency_value = 1;
        if (cd.has_value()) {
            score += cd->get().score;
            currency_value = cd->get().currency;
        } else {
            score += 10;
        }

        // Loot drop. Runs before the sprite/particle work so the RNG sequence
        // does not depend on whether the explosion sidecar happened to load (R2).
        {
            float lx = 0.0f, ly = 0.0f;
            if (auto pos = component_storage.get_component<Position>(enemy); pos.has_value()) {
                auto sz = component_storage.get_component<Size>(enemy);
                lx = pos->get().x + (sz.has_value() ? sz->get().width * 0.5f : 0.0f);
                ly = pos->get().y + (sz.has_value() ? sz->get().height * 0.5f : 0.0f);
            }
            drop_loot(component_storage, entity_manager, lx, ly, currency_value);
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

        // v2 Phase 4: every kill adds a little camera trauma.
        blackboard.set<float>("feedback.trauma", feedback::add_trauma(
            blackboard.get_or<float>("feedback.trauma", 0.0f),
            blackboard.get_or<float>("fb.trauma_enemy_death", 0.25f)));

        component_storage.add_component<DestroyRequest>(enemy, DestroyRequest{});
    }

    blackboard.set("score", score);
}
