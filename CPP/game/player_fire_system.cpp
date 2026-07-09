#include "player_fire_system.hpp"
#include "player_components.hpp"
#include "tower_components.hpp"   // ProjectileTag, ProjectileData
#include "collision_layers.hpp"
#include "aim_math.hpp"

void PlayerFireSystem::update(ComponentStorage& storage,
                              EntityManager& entity_manager,
                              const Blackboard& blackboard) {
    if (!blackboard.has("delta_time")) return;
    float dt = static_cast<float>(blackboard.get<double>("delta_time"));
    bool mouse_held = blackboard.get_or<bool>("mouse.held", false);

    for (Entity player : storage.entities_with_component<PlayerTag>()) {
        auto wpn_opt = storage.get_component<WeaponStats>(player);
        auto pos_opt = storage.get_component<Position>(player);
        auto rot_opt = storage.get_component<Rotation>(player);
        if (!wpn_opt.has_value() || !pos_opt.has_value() || !rot_opt.has_value()) continue;

        WeaponStats& wpn = wpn_opt->get();
        if (wpn.cooldown_remaining > 0.0f) {
            wpn.cooldown_remaining -= dt;
        }

        bool firing = mouse_held;
        auto input_opt = storage.get_component<Input>(player);
        if (input_opt.has_value() && input_opt->get().fire) firing = true;
        if (!firing || wpn.cooldown_remaining > 0.0f) continue;

        // Ready to fire: reset cooldown from fire rate (guard against 0).
        wpn.cooldown_remaining = wpn.fire_rate > 0.0f ? 1.0f / wpn.fire_rate : 0.25f;

        // Muzzle at the player's center.
        const Position& ppos = pos_opt->get();
        float cx = ppos.x, cy = ppos.y;
        auto psize = storage.get_component<Size>(player);
        if (psize.has_value()) {
            cx += psize->get().width * 0.5f;
            cy += psize->get().height * 0.5f;
        }

        // Aim angle with optional spread jitter.
        float angle = rot_opt->get().angle;
        if (wpn.spread > 0.0f) {
            std::uniform_real_distribution<float> jitter(-wpn.spread * 0.5f, wpn.spread * 0.5f);
            angle += jitter(rng_);
        }
        Velocity vel = aim_math::velocity_from_angle(angle, wpn.projectile_speed);

        constexpr float PR = 6.0f;   // projectile half-size / radius
        Entity shot = entity_manager.create_entity();
        storage.add_component<Position>(shot, Position{cx - PR, cy - PR});
        storage.add_component<Velocity>(shot, vel);
        storage.add_component<Size>(shot, Size{PR * 2.0f, PR * 2.0f});
        storage.add_component<Color>(shot, Color{120, 225, 255, 255});
        storage.add_component<Collider>(shot,
            Collider{PR * 2.0f, PR * 2.0f, layers::PROJECTILE, layers::PROJECTILE_MASK});
        storage.add_component<CircleCollider>(shot, CircleCollider{PR, 0.0f, 0.0f});
        storage.add_component<Lifetime>(shot, Lifetime{wpn.projectile_lifetime});
        storage.add_component<ProjectileTag>(shot, ProjectileTag{});
        storage.add_component<ProjectileData>(shot,
            ProjectileData{NO_TARGET, wpn.projectile_speed, wpn.damage});
        storage.add_component<RenderLayer>(shot, RenderLayer{5});

        // v2: additive glow trail that rides the projectile. The emitter dies with
        // the shot (destroyed on hit / lifetime); its live particles keep fading via
        // their own lifetime. Offset by PR so particles spawn at the shot's centre.
        ParticleEmitter trail;
        trail.shape = EmitterShape::Point;
        trail.additive = true;
        trail.emission_rate = 70.0f;
        trail.particle_lifetime = 0.35f;
        trail.min_speed = 0.0f;
        trail.max_speed = 24.0f;
        trail.cone_half_angle = 180.0f;
        trail.start_r = 120; trail.start_g = 225; trail.start_b = 255; trail.start_a = 220;
        trail.end_r = 40;    trail.end_g = 90;    trail.end_b = 160;   trail.end_a = 0;
        trail.start_size = 7.0f; trail.end_size = 0.0f;
        trail.offset_x = PR;  trail.offset_y = PR;
        storage.add_component<ParticleEmitter>(shot, trail);
    }
}
