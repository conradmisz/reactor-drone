#include "pickup_system.hpp"
#include "player_components.hpp"   // PlayerTag, ShipState, Pickup
#include "enemy_components.hpp"    // Health (the drone carries one too)
#include <algorithm>
#include <cmath>

void PickupSystem::update(ComponentStorage& component_storage,
                          EntityManager& entity_manager,
                          Blackboard& blackboard) {
    if (!blackboard.has("delta_time")) return;
    const float dt = static_cast<float>(blackboard.get<double>("delta_time"));

    // The single player: centre, radius and the ShipState the loot credits.
    Entity player = 0;
    bool have_player = false;
    float px = 0.0f, py = 0.0f, pr = 20.0f;
    for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
        auto pos = component_storage.get_component<Position>(p);
        auto sz = component_storage.get_component<Size>(p);
        if (!pos.has_value() || !sz.has_value()) continue;
        px = pos->get().x + sz->get().width * 0.5f;
        py = pos->get().y + sz->get().height * 0.5f;
        pr = sz->get().width * 0.5f;
        player = p;
        have_player = true;
        break;
    }
    if (!have_player) return;

    auto ship_opt = component_storage.get_component<ShipState>(player);
    if (!ship_opt.has_value()) return;
    ShipState& ship = ship_opt->get();

    const bool magnet = (ship.item_id == ITEM_MAGNET_CORE);
    // D193: from wave 15 the drone has a passive credit vacuum — units inside
    // roughly melee reach drift in on their own. Same steering as the Magnet
    // Core, a much shorter leash, and currency only: hull/shield/key pickups
    // still have to be flown over.
    const bool vacuum = blackboard.get_or<int>("wave", 0) >= VACUUM_FIRST_WAVE;
    // Salvager (Phase 4): every credit pickup is worth more. Values are small
    // ints (1/2/4), so a bare multiply would round 1 x 1.25 straight back to 1 —
    // the floor of +1 guarantees the item is never a no-op on the common drop.
    const float salvage = (ship.item_id == item_ids::SALVAGER)
                              ? blackboard.get_or<float>("ship.item_amount", 1.0f)
                              : 1.0f;

    for (Entity e : component_storage.entities_with_component<Pickup>()) {
        if (component_storage.has_component<DestroyRequest>(e)) continue;
        auto pos = component_storage.get_component<Position>(e);
        auto sz = component_storage.get_component<Size>(e);
        if (!pos.has_value() || !sz.has_value()) continue;

        // D183: a pickup on a clock warns before it vanishes — the last 3 s it
        // blinks, faster as the timer runs out (~0.6 s period down to ~0.12 s).
        // Driven purely by the remaining lifetime and written as a hard on/off
        // Tint alpha: render-only, no RNG, and the collection maths below never
        // notices, so the canary cannot move. (A gradual alpha fade was already
        // tried and rejected — see feedback.hpp.)
        if (auto lt = component_storage.get_component<Lifetime>(e); lt.has_value()) {
            if (auto tint = component_storage.get_component<Tint>(e); tint.has_value()) {
                const float t = lt->get().remaining;
                constexpr float BLINK_WINDOW = 3.0f;
                if (t < BLINK_WINDOW) {
                    const float period = 0.12f + t * 0.16f;
                    const bool visible = std::fmod(t, period) > period * 0.4f;
                    tint->get().a = visible ? 255 : 0;
                }
            }
        }

        const float half = sz->get().width * 0.5f;
        float cx = pos->get().x + half;
        float cy = pos->get().y + half;
        float dx = px - cx, dy = py - cy;
        float dist = std::sqrt(dx * dx + dy * dy);

        const Pickup& pk = component_storage.get_component<Pickup>(e)->get();

        float pull_radius = magnet ? economy_.pickup_magnet_radius : 0.0f;
        if (vacuum && pk.kind == static_cast<int>(PickupKind::Currency))
            pull_radius = std::max(pull_radius, VACUUM_RADIUS);

        if (pull_radius > 0.0f && dist > 0.001f && dist <= pull_radius) {
            float step = pk.magnet_speed * dt;
            if (step > dist) step = dist;         // never overshoot past the ship
            pos->get().x += (dx / dist) * step;
            pos->get().y += (dy / dist) * step;
            cx = pos->get().x + half;
            cy = pos->get().y + half;
            dist -= step;
        }

        if (dist > pr + half) continue;

        if (pk.kind == static_cast<int>(PickupKind::Key)) {
            ship.keys += pk.value;
        } else if (pk.kind == static_cast<int>(PickupKind::Health)) {
            // #10 (D56): hull repair, never overheal. Clamped here rather than at
            // the placement site so the rule holds for any future producer.
            if (auto h = component_storage.get_component<Health>(player); h.has_value()) {
                h->get().current = std::min(h->get().max_hp,
                                            h->get().current + static_cast<float>(pk.value));
            }
        } else if (pk.kind == static_cast<int>(PickupKind::Shield)) {
            // Same rule against shield_max, which is 0 until a Shield Capacitor is
            // bought — an unbanked cell is worth nothing rather than a free bank.
            ship.shield = std::min(ship.shield_max,
                                   ship.shield + static_cast<float>(pk.value));
        } else {
            ship.currency += salvage > 1.0f
                ? std::max(pk.value + 1, static_cast<int>(std::lround(pk.value * salvage)))
                : pk.value;
        }

        // One-shot pop: a short-lived host carrying a burst emitter, the same
        // pattern EnemyDeathSystem uses for the death burst.
        Entity pop = entity_manager.create_entity();
        component_storage.add_component<Position>(pop, Position{cx, cy});
        ParticleEmitter fx;
        fx.shape = EmitterShape::Point;
        fx.additive = true;
        fx.emission_rate = 260.0f;
        fx.particle_lifetime = 0.28f;
        fx.min_speed = 40.0f; fx.max_speed = 130.0f;
        fx.cone_half_angle = 180.0f;
        fx.start_r = 255; fx.start_g = 230; fx.start_b = 140; fx.start_a = 255;
        fx.end_r = 255;   fx.end_g = 150;   fx.end_b = 40;    fx.end_a = 0;
        fx.start_size = 5.0f; fx.end_size = 0.0f;
        component_storage.add_component<ParticleEmitter>(pop, fx);
        component_storage.add_component<Lifetime>(pop, Lifetime{0.06f});

        component_storage.add_component<DestroyRequest>(e, DestroyRequest{});
    }
}
