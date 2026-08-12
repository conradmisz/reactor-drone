#include "player_damage_system.hpp"
#include "player_components.hpp"   // PlayerTag, ContactDamage, Flash
#include "enemy_components.hpp"    // EnemyShot, EnemyBehavior, EnemyTag (telemetry classifier)
#include "tower_components.hpp"    // DamageEvent
#include "feedback.hpp"            // add_trauma
#include <algorithm>
#include <cmath>

void PlayerDamageSystem::update(EntityManager& entity_manager,
                                ComponentStorage& storage,
                                Blackboard& blackboard) {
    float dt = blackboard.has("delta_time")
                   ? static_cast<float>(blackboard.get<double>("delta_time"))
                   : 0.0f;

    // Tick down the shared i-frame timer.
    float iframes = blackboard.get_or<float>("player.iframes", 0.0f);
    if (iframes > 0.0f) iframes = std::max(0.0f, iframes - dt);

    for (Entity player : storage.entities_with_component<PlayerTag>()) {
        if (!storage.has_component<Health>(player)) continue;

        auto collided = storage.get_component<CollidedWith>(player);
        if (!collided.has_value()) continue;

        if (iframes > 0.0f) continue;  // invulnerable: ignore contact this frame

        for (Entity other : collided->get().entities) {
            // Anything carrying ContactDamage hurts the drone: enemies (EnemyTag)
            // and v2 Phase-6 static hazard patches alike, through the same event.
            auto cd = storage.get_component<ContactDamage>(other);
            if (!cd.has_value()) continue;
            if (!entity_manager.is_alive(other)) continue;

            float amount = cd->get().amount;

            // Gameplay Phase 3: the Shield Capacitor soaks damage before the hull,
            // and *any* hit restarts the regen delay — including one the shield ate
            // whole, so chip damage can't be free.
            if (auto s = storage.get_component<ShipState>(player); s.has_value()) {
                ShipState& ship = s->get();
                ship.shield_delay = blackboard.get_or<float>("ship.shield_regen_delay", 3.0f);
                if (ship.shield > 0.0f) {
                    float soaked = std::min(ship.shield, amount);
                    ship.shield -= soaked;
                    amount -= soaked;
                }

                // Gameplay Phase 4: Reactive Plating hits the attacker back. It
                // fires on contact, not on hull loss, so it still works behind a
                // full shield — the plating reacts to being *rammed*.
                float reflect = blackboard.get_or<float>("ship.item_amount", 0.0f);
                if (ship.item_id == item_ids::REACTIVE_PLATING && reflect > 0.0f) {
                    Entity back = entity_manager.create_entity();
                    storage.add_component<DamageEvent>(back, DamageEvent{other, reflect});
                }
            }

            if (amount > 0.0f) {
                Entity event_entity = entity_manager.create_entity();
                storage.add_component<DamageEvent>(event_entity, DamageEvent{player, amount});
            }

            // D134: publish the bearing the hit came from, so the shield field's
            // impact bloom plays where the hit landed instead of always at the
            // nose. Cosmetic and write-only — nothing in the sim reads it, so it
            // cannot move the replay canary.
            if (auto ppos = storage.get_component<Position>(player); ppos.has_value()) {
                if (auto opos = storage.get_component<Position>(other); opos.has_value()) {
                    const float dx = opos->get().x - ppos->get().x;
                    const float dy = opos->get().y - ppos->get().y;
                    if (dx != 0.0f || dy != 0.0f) {
                        blackboard.set<float>("player.hit_bearing",
                            std::atan2(dy, dx) * 180.0f / 3.14159265358979323846f);
                    }
                }
            }

            // telemetry: what last hurt the drone, read only at death (write-only, the
            // hit_bearing precedent). "enemy:0" = a default enemy with no EnemyBehavior.
            if (storage.has_component<EnemyShot>(other)) {
                blackboard.set<std::string>("tm.last_hit_by", "shot");
            } else if (auto eb = storage.get_component<EnemyBehavior>(other); eb.has_value()) {
                blackboard.set<std::string>("tm.last_hit_by",
                                            "enemy:" + std::to_string(eb->get().kind));
            } else if (storage.has_component<EnemyTag>(other)) {
                blackboard.set<std::string>("tm.last_hit_by", "enemy:0");
            } else {
                blackboard.set<std::string>("tm.last_hit_by", "hazard");
            }

            // v2 Phase 4: kick the camera and flash the drone red on the hit.
            float trauma = feedback::add_trauma(
                blackboard.get_or<float>("feedback.trauma", 0.0f),
                blackboard.get_or<float>("fb.trauma_player_hit", 0.6f));
            blackboard.set<float>("feedback.trauma", trauma);

            float fdur = blackboard.get_or<float>("fb.flash_duration", 0.12f);
            storage.add_component<Flash>(player, Flash{fdur, fdur,
                static_cast<uint8_t>(blackboard.get_or<int>("fb.player_flash_r", 255)),
                static_cast<uint8_t>(blackboard.get_or<int>("fb.player_flash_g", 70)),
                static_cast<uint8_t>(blackboard.get_or<int>("fb.player_flash_b", 70))});

            iframes = blackboard.get_or<float>("player.invuln_window", 0.8f);
            break;  // one hit per frame
        }
    }

    blackboard.set<float>("player.iframes", iframes);
}
