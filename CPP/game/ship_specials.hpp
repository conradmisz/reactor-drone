#ifndef SHIP_SPECIALS_HPP
#define SHIP_SPECIALS_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/blackboard.hpp"
#include "player_components.hpp"

/**
 * Ship special attributes (gameplay pack v2.3, D221/D223) — the tick_shields
 * idiom: free functions in a header, no new system class.
 *
 * Specials by id (ShipDef::special, published as "ship.special" by start_run):
 *   equip_cd     Falcon — 25% lower boss-item cooldown. No tick: start_run
 *                seeds "ship.active_cd_mult" 0.75 and active_items reads it.
 *   phoenix_veil Owl — dipping below 10% hull grants 4 s of invincibility but
 *                jams the trigger ("ship.no_fire", read by PlayerFireSystem).
 *                Re-arms once hull climbs back to 25%, so hovering at 9% can't
 *                re-trigger it every frame.
 *   ram_dash     Gryphon — a dash recharges shield and shoves contacted
 *                enemies away (handled inside tick_dash, which owns contact).
 */
namespace veil {
constexpr float TRIGGER_FRAC = 0.10f;
constexpr float REARM_FRAC   = 0.25f;
constexpr float DURATION     = 4.0f;

/// Pure trigger/re-arm rules — unit-tested.
inline bool should_fire(float hull_frac, bool armed) {
    return armed && hull_frac < TRIGGER_FRAC;
}
inline bool should_rearm(float hull_frac, bool armed) {
    return !armed && hull_frac >= REARM_FRAC;
}
}  // namespace veil

/// Per-frame veil bookkeeping. Cheap no-op for every ship but the Owl, except
/// the "ship.no_fire" countdown, which any future jam source may reuse.
inline void tick_ship_specials(ComponentStorage& storage, Blackboard& bb, float dt) {
    const float nf = bb.get_or<float>("ship.no_fire", 0.0f);
    if (nf > 0.0f) bb.set<float>("ship.no_fire", nf - dt);

    if (bb.get_or<std::string>("ship.special", std::string()) != "phoenix_veil") return;
    for (Entity player : storage.entities_with_component<PlayerTag>()) {
        auto h = storage.get_component<Health>(player);
        if (!h.has_value() || h->get().max_hp <= 0.0f) return;
        const float frac = h->get().current / h->get().max_hp;
        bool armed = bb.get_or<bool>("veil.armed", true);
        if (veil::should_fire(frac, armed)) {
            bb.set<bool>("veil.armed", false);
            bb.set<float>("ship.no_fire", veil::DURATION);
            bb.set<float>("player.iframes",
                std::max(bb.get_or<float>("player.iframes", 0.0f), veil::DURATION));
        } else if (veil::should_rearm(frac, armed)) {
            bb.set<bool>("veil.armed", true);
        }
        return;  // one player
    }
}

#endif  // SHIP_SPECIALS_HPP
