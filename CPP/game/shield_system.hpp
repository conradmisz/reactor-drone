#ifndef SHIELD_SYSTEM_HPP
#define SHIELD_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "player_components.hpp"   // PlayerTag, ShipState
#include <algorithm>

/**
 * tick_shields — recharge the Shield Capacitor (Gameplay Phase 3).
 *
 * ShipState.shield_delay is the *remaining* quiet time: PlayerDamageSystem sets
 * it to blackboard "ship.shield_regen_delay" on every hit, this counts it down,
 * and only once it reaches zero does shield refill at shield_regen per second.
 * Inert until a Shield Capacitor is bought (shield_max stays 0).
 *
 * ponytail: a free function in a header, not a class — it is six lines of state
 * with no configuration and nothing to own. Phase 4's buff timer ticks here too
 * (same shape: one countdown on ShipState); promote it to a real system only if
 * that grows past a handful of lines.
 */
inline void tick_shields(ComponentStorage& storage, float dt) {
    for (Entity p : storage.entities_with_component<PlayerTag>()) {
        auto s = storage.get_component<ShipState>(p);
        if (!s.has_value()) continue;
        ShipState& ship = s->get();
        if (ship.shield_max <= 0.0f) continue;
        if (ship.shield_delay > 0.0f) {
            ship.shield_delay = std::max(0.0f, ship.shield_delay - dt);
            continue;
        }
        ship.shield = std::min(ship.shield_max, ship.shield + ship.shield_regen * dt);
    }
}

#endif // SHIELD_SYSTEM_HPP
