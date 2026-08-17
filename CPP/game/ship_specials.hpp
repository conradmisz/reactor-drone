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
 *   dash_charge  Owl — a second dash charge from wave 1 (playtest #3 item 4,
 *                D229 — replaces the phoenix veil, owner's call). No tick:
 *                start_run bumps ShipState::dash_max by one.
 *   ram_dash     Gryphon — a dash recharges shield and shoves contacted
 *                enemies away (handled inside tick_dash, which owns contact).
 */

/// Per-frame special bookkeeping. Today that is only the "ship.no_fire" jam
/// countdown (read by PlayerFireSystem and the secondaries) — nothing sets it
/// since the veil retired (D229), but any future jam source re-uses it.
inline void tick_ship_specials(ComponentStorage& storage, Blackboard& bb, float dt) {
    (void)storage;
    const float nf = bb.get_or<float>("ship.no_fire", 0.0f);
    if (nf > 0.0f) bb.set<float>("ship.no_fire", nf - dt);
}

#endif  // SHIP_SPECIALS_HPP
