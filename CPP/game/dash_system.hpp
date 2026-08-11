#ifndef DASH_SYSTEM_HPP
#define DASH_SYSTEM_HPP

#include <algorithm>
#include <cmath>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "arena_config.hpp"        // DashConfig
#include "enemy_components.hpp"    // EnemyTag
#include "player_components.hpp"   // PlayerTag, ShipState
#include "tower_components.hpp"    // DamageEvent

/**
 * tick_dash — the thruster dash (#5, D57).
 *
 * LSHIFT fires a short burst of velocity along the drone's current heading (its
 * aim, if it is standing still) that both closes distance and hurts whatever it
 * passes through. Free functions in a header, the `tick_shields` idiom: the
 * durable state — the cooldown and the remaining burst — already lives on
 * ShipState (dash_cd / dash_timer), so there is no system to own.
 *
 * DashState is the little that does NOT belong on a component: which enemies this
 * particular dash has already hit, and the thruster emission rate to restore when
 * it ends. One dash's worth of scratch, cleared at every trigger.
 *
 * The two damage rules come straight from the playtest note:
 *   - each enemy the dash overlaps takes damage EXACTLY ONCE, however many frames
 *     the burst spends inside it (0.15s is ~9 frames at 60Hz — without the
 *     already-hit list a dash would delete the swarm);
 *   - the player can be hurt by AT MOST ONE enemy per dash. The first contact is
 *     let through to PlayerDamageSystem untouched; from then on the i-frame timer
 *     is held above the remaining burst, so ploughing through a crowd costs one
 *     hit, not one per body.
 *
 * Particle cost: the existing thruster emitter is temporarily driven harder for
 * the burst — no new emitter, no new host entity. See DASH_EMISSION_RATE.
 */

/// Emission rate the player's thruster runs at during a burst. 0.15s at 240/s is
/// ~36 extra live particles at a 0.4s lifetime — under 2% of DEFAULT_MAX_PARTICLES,
/// and it decays before the cooldown is up, so dashes cannot stack their cost.
constexpr float DASH_EMISSION_RATE = 240.0f;

/// Frames in the dash button's cooldown dial (#11). Must equal SWEEP_N in
/// make_sprites.py — the atlas is authored at even progress steps and this is
/// the only thing that says how many of them there are.
constexpr int DASH_SWEEP_FRAMES = 16;

/**
 * The dial frame for a recharge fraction (0 = just dashed, 1 = ready).
 *
 * Total and clamped: any input returns a valid frame. 1.0 lands on the last
 * frame rather than one past the end — the caller parks the sprite at that
 * point, so the near-empty frame is never actually drawn.
 */
inline int dash_sweep_frame(float frac) {
    const int i = static_cast<int>(std::clamp(frac, 0.0f, 1.0f) * DASH_SWEEP_FRAMES);
    return std::min(i, DASH_SWEEP_FRAMES - 1);
}

struct DashState {
    float dir_x = 1.0f, dir_y = 0.0f;   // heading captured when the burst began
    std::vector<Entity> hit;            // enemies already damaged by THIS dash
    bool player_contact = false;        // the one hit this dash is allowed to cost
    float base_emission = 0.0f;         // thruster rate to restore when it ends
    bool boosted = false;
};

/**
 * @param key_down  LSHIFT (or its scripted alias) held this frame.
 * @param dt        frame delta in seconds.
 *
 * Must run after PlayerControlSystem has written the frame's Velocity — the burst
 * overrides ordinary movement — and before collision, so the i-frame hold lands
 * before PlayerDamageSystem reads it.
 */
inline void tick_dash(ComponentStorage& storage,
                      EntityManager& entity_manager,
                      Blackboard& blackboard,
                      const DashConfig& cfg,
                      DashState& state,
                      bool key_down,
                      float dt) {
    if (!(cfg.duration > 0.0f)) return;   // feature off

    for (Entity player : storage.entities_with_component<PlayerTag>()) {
        auto ship_opt = storage.get_component<ShipState>(player);
        auto vel_opt = storage.get_component<Velocity>(player);
        auto pos_opt = storage.get_component<Position>(player);
        if (!ship_opt.has_value() || !vel_opt.has_value() || !pos_opt.has_value()) return;
        ShipState& ship = ship_opt->get();
        Velocity& vel = vel_opt->get();

        auto emitter = storage.get_component<ParticleEmitter>(player);

        // --- Charges (#10, D192) ---
        // One cooldown clock refilling a stack of charges, rather than N clocks:
        // the timer only runs while the stack is short, and each expiry hands
        // back exactly one burst. dash_max grows by one per boss killed.
        if (ship.dash_max < 1) { ship.dash_max = 1; ship.dash_charges = 1; }
        ship.dash_charges = std::min(ship.dash_charges, ship.dash_max);
        if (ship.dash_charges < ship.dash_max) {
            ship.dash_cd = std::max(0.0f, ship.dash_cd - dt);
            if (ship.dash_cd <= 0.0f) {
                ++ship.dash_charges;
                if (ship.dash_charges < ship.dash_max) ship.dash_cd = cfg.cooldown;
            }
        } else {
            ship.dash_cd = 0.0f;
        }

        // --- Trigger ---
        const bool stack_was_full = ship.dash_charges >= ship.dash_max;
        if (ship.dash_timer <= 0.0f && key_down && ship.dash_charges > 0) {
            float dx = vel.dx, dy = vel.dy;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.0001f) {
                dx /= len; dy /= len;
            } else if (auto rot = storage.get_component<Rotation>(player); rot.has_value()) {
                // Standing still: dash where the drone is AIMING. Falling back to
                // the stale previous heading would send it somewhere the player
                // is no longer looking, which reads as a broken control.
                dx = std::cos(rot->get().angle);
                dy = std::sin(rot->get().angle);
            } else {
                dx = 1.0f; dy = 0.0f;
            }
            state.dir_x = dx;
            state.dir_y = dy;
            state.hit.clear();
            state.player_contact = false;
            ship.dash_timer = cfg.duration;
            --ship.dash_charges;
            // Only start the refill clock if the stack was full; spending a
            // second charge must not rewind the one already regenerating.
            if (stack_was_full) ship.dash_cd = cfg.cooldown;
            if (emitter.has_value() && !state.boosted) {
                state.base_emission = emitter->get().emission_rate;
                emitter->get().emission_rate = DASH_EMISSION_RATE;
                state.boosted = true;
            }
        }

        if (ship.dash_timer <= 0.0f) return;

        // --- Burst ---
        vel.dx = state.dir_x * cfg.speed;
        vel.dy = state.dir_y * cfg.speed;

        auto psz = storage.get_component<Size>(player);
        const float pr = psz.has_value() ? psz->get().width * 0.5f : 20.0f;
        const float pcx = pos_opt->get().x + pr;
        const float pcy = pos_opt->get().y + (psz.has_value() ? psz->get().height * 0.5f : pr);

        bool touching = false;
        for (Entity e : storage.entities_with_component<EnemyTag>()) {
            if (storage.has_component<DestroyRequest>(e)) continue;
            auto epos = storage.get_component<Position>(e);
            auto esz = storage.get_component<Size>(e);
            if (!epos.has_value() || !esz.has_value()) continue;
            const float er = esz->get().width * 0.5f;
            const float ex = epos->get().x + er;
            const float ey = epos->get().y + esz->get().height * 0.5f;
            const float dx = ex - pcx, dy = ey - pcy;
            if (std::sqrt(dx * dx + dy * dy) > pr + er) continue;

            touching = true;
            if (std::find(state.hit.begin(), state.hit.end(), e) != state.hit.end()) continue;
            state.hit.push_back(e);
            if (cfg.damage > 0.0f) {
                Entity ev = entity_manager.create_entity();
                storage.add_component<DamageEvent>(ev, DamageEvent{e, cfg.damage});
            }
        }

        if (touching) {
            if (state.player_contact) {
                // Already paid for this dash: hold i-frames past the end of the
                // burst so no second body can land a hit before it is over.
                const float hold = ship.dash_timer + dt;
                blackboard.set<float>("player.iframes",
                    std::max(blackboard.get_or<float>("player.iframes", 0.0f), hold));
            } else {
                // First contact of the dash — let PlayerDamageSystem resolve it
                // normally this frame (it sets its own invuln window afterwards).
                state.player_contact = true;
            }
        }

        ship.dash_timer -= dt;
        if (ship.dash_timer <= 0.0f) {
            ship.dash_timer = 0.0f;
            if (emitter.has_value() && state.boosted) {
                emitter->get().emission_rate = state.base_emission;
            }
            state.boosted = false;
            state.hit.clear();
        }
        return;   // one player
    }
}

#endif  // DASH_SYSTEM_HPP
