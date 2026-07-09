#include "player_damage_system.hpp"
#include "player_components.hpp"   // PlayerTag, ContactDamage
#include "enemy_components.hpp"    // EnemyTag
#include "tower_components.hpp"    // DamageEvent
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
            if (!storage.has_component<EnemyTag>(other)) continue;
            if (!entity_manager.is_alive(other)) continue;

            float amount = 10.0f;
            auto cd = storage.get_component<ContactDamage>(other);
            if (cd.has_value()) amount = cd->get().amount;

            Entity event_entity = entity_manager.create_entity();
            storage.add_component<DamageEvent>(event_entity, DamageEvent{player, amount});

            iframes = blackboard.get_or<float>("player.invuln_window", 0.8f);
            break;  // one hit per frame
        }
    }

    blackboard.set<float>("player.iframes", iframes);
}
