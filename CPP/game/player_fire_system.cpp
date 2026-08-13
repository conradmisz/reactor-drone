#include "player_fire_system.hpp"
#include "player_components.hpp"
#include "tower_components.hpp"   // ProjectileTag, ProjectileData
#include "collision_layers.hpp"
#include "aim_math.hpp"
#include <algorithm>

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

        // Playtest #8: the trigger is the MOUSE, and only the mouse.
        //
        // The engine's InputSystem writes Input.fire from SDL_SCANCODE_SPACE, and
        // this system used to OR that in — so SPACE fired the weapon on every
        // frame the dash did not consume (no charge, mid-burst, cooldown), which
        // read as "sometimes my dash key shoots instead". Fixed here rather than
        // in InputSystem because that component is engine-general and Lua's
        // engine.get_input still exposes the flag; SPACE-is-dash-only is a
        // Reactor Drone rule, so it belongs in the game's fire system.
        //
        // The headless path is unaffected: a scripted SPACE sets "mouse.held"
        // directly (see main.cpp's scripted_fire), it never went through Input.
        bool firing = mouse_held;

        // #9 (D192): the primary-fire battery. Holding the trigger drains it;
        // anything else charges it at a constant rate. Draining it to EMPTY
        // latches a lockout that only clears at full, so the cost of holding the
        // button down is the whole recharge, not a sliver of it. The two rates
        // arrive on the Blackboard (published once at run start) for the same
        // reason the barrel count does: this system stays config-blind.
        auto ship_opt = storage.get_component<ShipState>(player);
        const float drain = blackboard.get_or<float>("battery.drain_per_s", 0.0f);
        if (ship_opt.has_value() && drain > 0.0f) {
            ShipState& s = ship_opt->get();
            if (firing && !s.battery_locked) {
                s.battery = std::max(0.0f, s.battery - drain * dt);
                if (s.battery <= 0.0f) s.battery_locked = true;
            } else {
                s.battery = std::min(1.0f, s.battery +
                    blackboard.get_or<float>("battery.charge_per_s", 0.0f) * dt);
                if (s.battery >= 1.0f) s.battery_locked = false;
            }
            if (s.battery_locked) firing = false;
        }

        if (!firing || wpn.cooldown_remaining > 0.0f) continue;

        // Ready to fire: reset cooldown from fire rate (guard against 0).
        // Gameplay Phase 4: Overdrive scales the rate while its buff is live.
        // Read here rather than written into WeaponStats on use, so a shop
        // purchase during the buff can never be undone by the expiry (D34).
        float rate = wpn.fire_rate;
        if (ship_opt.has_value() && ship_opt->get().buff_id == consumable_ids::OVERDRIVE)
            rate *= blackboard.get_or<float>("ship.buff_mult", 1.0f);
        wpn.cooldown_remaining = rate > 0.0f ? 1.0f / rate : 0.25f;

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
        constexpr float PR = 6.0f;   // projectile half-size / radius

        // D184 (supersedes D108's hardcoded red): shots are the COMPLEMENT of
        // the ship's hull hue, published by start_run from ShipDef.color. Same
        // catalogue-blind Blackboard pattern as the barrel count. The defaults
        // are the Standard drone's complement — D108's red-orange, so a path
        // that never wrote the keys looks exactly as it did.
        const uint8_t sr = static_cast<uint8_t>(blackboard.get_or<int>("ship.shot_r", 165));
        const uint8_t sg = static_cast<uint8_t>(blackboard.get_or<int>("ship.shot_g", 35));
        const uint8_t sb = static_cast<uint8_t>(blackboard.get_or<int>("ship.shot_b", 0));

        // Gameplay Phase 3 (Twin Barrel): fire `barrels` shots in a fan centred on
        // the aim angle. The count comes from the blackboard, written by the shop,
        // so this system needs no catalogue knowledge. One spread jitter per
        // volley, not per barrel — the fan is meant to be a shape, not a shotgun.
        const int barrels = 1 + std::max(0, blackboard.get_or<int>("ship.extra_shots", 0));
        // Ricochet Coils (D98): same Blackboard-not-catalogue-index pattern as the
        // barrel count. Stamped per shot so a purchase mid-flight never retro-fits
        // bounces onto shots already in the air.
        const int bounces = std::max(0, blackboard.get_or<int>("ship.bounces", 0));
        constexpr float FAN_STEP = 0.09f;   // radians between barrels (~5 degrees)
        for (int b = 0; b < barrels; ++b) {
            float shot_angle = angle + (static_cast<float>(b) - (barrels - 1) * 0.5f) * FAN_STEP;
            Velocity vel = aim_math::velocity_from_angle(shot_angle, wpn.projectile_speed);

            Entity shot = entity_manager.create_entity();
            storage.add_component<Position>(shot, Position{cx - PR, cy - PR});
            storage.add_component<Velocity>(shot, vel);
            storage.add_component<Size>(shot, Size{PR * 2.0f, PR * 2.0f});
            // v3 Tier 7: NO Color component — a shot with one renders as the
            // old solid square underneath its ribbon. The same brightened hue
            // now rides on ProjectileTag and drives the neon ribbon instead.
            storage.add_component<Collider>(shot,
                Collider{PR * 2.0f, PR * 2.0f, layers::PROJECTILE, layers::PROJECTILE_MASK});
            storage.add_component<CircleCollider>(shot, CircleCollider{PR, 0.0f, 0.0f});
            storage.add_component<Lifetime>(shot, Lifetime{wpn.projectile_lifetime});
            storage.add_component<ProjectileTag>(shot, ProjectileTag{
                static_cast<uint8_t>(std::min(255, sr + 90)),
                static_cast<uint8_t>(std::min(255, sg + 35)),
                static_cast<uint8_t>(std::min(255, sb + 60))});
            storage.add_component<ProjectileData>(shot,
                ProjectileData{NO_TARGET, wpn.projectile_speed, wpn.damage, bounces});
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
            // Trail brackets the shot colour: brighter at spawn, dimmed at death.
            trail.start_r = static_cast<uint8_t>(std::min(255, sr + 90));
            trail.start_g = static_cast<uint8_t>(std::min(255, sg + 55));
            trail.start_b = static_cast<uint8_t>(std::min(255, sb + 70));
            trail.start_a = 220;
            trail.end_r = static_cast<uint8_t>(sr / 2);
            trail.end_g = static_cast<uint8_t>(sg / 2);
            trail.end_b = static_cast<uint8_t>(sb / 2);
            trail.end_a = 0;
            trail.start_size = 7.0f; trail.end_size = 0.0f;
            trail.offset_x = PR;  trail.offset_y = PR;
            storage.add_component<ParticleEmitter>(shot, trail);
        }
    }
}
