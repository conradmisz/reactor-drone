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

/**
 * tick_buff — expire the active timed consumable buff (Gameplay Phase 4, D29).
 *
 * The buff is *read* where it matters (PlayerFireSystem scales fire_rate while
 * buff_id == consumable_ids::OVERDRIVE), so expiry is only "clear the id" — no
 * save/restore step to get wrong when the shop edits the same stat mid-buff.
 * Re-using a consumable resets the timer rather than stacking, which falls out
 * of there being exactly one buff slot.
 */
inline void tick_buff(ComponentStorage& storage, float dt) {
    for (Entity p : storage.entities_with_component<PlayerTag>()) {
        auto s = storage.get_component<ShipState>(p);
        if (!s.has_value()) continue;
        ShipState& ship = s->get();
        if (ship.buff_id < 0) continue;
        ship.buff_timer -= dt;
        if (ship.buff_timer <= 0.0f) { ship.buff_id = -1; ship.buff_timer = 0.0f; }
    }
}

#endif // SHIELD_SYSTEM_HPP
