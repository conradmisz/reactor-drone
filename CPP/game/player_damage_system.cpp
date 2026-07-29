#include "player_damage_system.hpp"
#include "player_components.hpp"   // PlayerTag, ContactDamage, Flash
#include "tower_components.hpp"    // DamageEvent
#include "feedback.hpp"            // add_trauma
#include <algorithm>

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
            }

            if (amount > 0.0f) {
                Entity event_entity = entity_manager.create_entity();
                storage.add_component<DamageEvent>(event_entity, DamageEvent{player, amount});
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
