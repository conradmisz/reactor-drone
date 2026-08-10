#ifndef ITEM_SYSTEM_HPP
#define ITEM_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "player_components.hpp"   // PlayerTag, ShipState, item_ids, consumable_ids
#include "enemy_components.hpp"    // EnemyTag, Health
#include "tower_components.hpp"    // DamageEvent
#include "arena_config.hpp"        // ShopConfig, ShopUpgradeDef
#include <algorithm>
#include <cmath>
#include <string>

/**
 * items — the Phase 4 equipment slots (D6/D7): one passive item, one held
 * consumable, both on ShipState.
 *
 * Three of the four items are one-liners bolted onto the system that already
 * owns their moment — Magnet Core and Salvager in PickupSystem, Reactive Plating
 * in PlayerDamageSystem. Only the Repulsor Field has no such home, so its push
 * lives here; and consumables need a single "spend it" entry point, which is
 * use_consumable().
 *
 * ponytail: free functions in a header, not classes — same call as tick_shields
 * (D29). There is no per-frame state to own and nothing to configure beyond the
 * catalogue, which the caller already holds. Promote to a real system if items
 * ever need their own timers.
 *
 * Effect strings, never row indices (D26): the catalogue maps
 * effect -> item_ids/consumable_ids here, and every consumer switches on the
 * string or on a constant, so re-ordering GameData.json can never fire the
 * wrong effect.
 */
namespace items {

/// Catalogue `effect` -> the id stored in ShipState.item_id. -1 = not an item.
inline int item_id_for(const std::string& effect) {
    if (effect == "magnet")   return item_ids::MAGNET_CORE;
    if (effect == "repulsor") return item_ids::REPULSOR_FIELD;
    if (effect == "reactive") return item_ids::REACTIVE_PLATING;
    if (effect == "salvage")  return item_ids::SALVAGER;
    return -1;
}

/// Catalogue `effect` -> ShipState.consumable_id. -1 = not a consumable.
inline int consumable_id_for(const std::string& effect) {
    if (effect == "repair")      return consumable_ids::REPAIR_KIT;
    if (effect == "overdrive")   return consumable_ids::OVERDRIVE;
    if (effect == "emp")         return consumable_ids::EMP_BURST;
    if (effect == "phase_shift") return consumable_ids::PHASE_SHIFT;
    return -1;
}

/// The catalogue row a held consumable id came from, or nullptr.
inline const ShopUpgradeDef* consumable_def(const ShopConfig& shop, int id) {
    for (const auto& d : shop.consumables)
        if (consumable_id_for(d.effect) == id) return &d;
    return nullptr;
}

/// The single player entity plus its ShipState, or nullptr if there is none.
inline ShipState* ship_of(ComponentStorage& storage, Entity& out_player) {
    for (Entity p : storage.entities_with_component<PlayerTag>()) {
        auto s = storage.get_component<ShipState>(p);
        if (!s.has_value()) return nullptr;
        out_player = p;
        return &s->get();
    }
    return nullptr;
}

/**
 * Repulsor Field: shove every enemy inside `radius` outward at the item's
 * `amount` px/s (published as "ship.item_amount" when it was equipped).
 *
 * A *soft* push, deliberately: a hard "eject to the rim" like
 * push_circle_out_of_aabb would make contact damage impossible and the drone
 * invulnerable. Each step is clamped so an enemy settles at the rim instead of
 * being flung past it, and the push is tuned below the slowest enemy's speed so
 * nothing can ever be held forever (which would stall D4's arena-clear gate).
 */
/**
 * The push itself, without the "is the Repulsor Field equipped" question.
 *
 * Extracted (Iteration 3, D74) so the boss-reward repulsion device can reuse the
 * exact same shove instead of copying it — it is the same effect from a different
 * trigger, and two copies of a clamped push would drift apart the first time
 * either is retuned.
 */
inline void push_enemies_out(ComponentStorage& storage, float px, float py,
                             float radius, float push, float dt) {
    if (push <= 0.0f || radius <= 0.0f || dt <= 0.0f) return;
    for (Entity e : storage.entities_with_component<EnemyTag>()) {
        auto epos = storage.get_component<Position>(e);
        auto esz = storage.get_component<Size>(e);
        if (!epos.has_value() || !esz.has_value()) continue;
        const float half = esz->get().width * 0.5f;
        const float dx = (epos->get().x + half) - px;
        const float dy = (epos->get().y + half) - py;
        const float d = std::sqrt(dx * dx + dy * dy);
        if (d >= radius || d < 0.001f) continue;
        const float step = std::min(push * dt, radius - d);
        epos->get().x += dx / d * step;
        epos->get().y += dy / d * step;
    }
}

inline void repulse_enemies(ComponentStorage& storage, const Blackboard& blackboard,
                            float radius, float dt) {
    Entity player = 0;
    ShipState* ship = ship_of(storage, player);
    if (ship == nullptr || ship->item_id != item_ids::REPULSOR_FIELD) return;
    auto pos = storage.get_component<Position>(player);
    auto sz = storage.get_component<Size>(player);
    if (!pos.has_value() || !sz.has_value()) return;
    push_enemies_out(storage,
                     pos->get().x + sz->get().width * 0.5f,
                     pos->get().y + sz->get().height * 0.5f,
                     radius, blackboard.get_or<float>("ship.item_amount", 0.0f), dt);
}

/**
 * Spend the held consumable (Q). Returns false when the slot is empty or the
 * catalogue no longer contains the held id.
 *
 * Phase Shift needs no buff machinery at all — PlayerDamageSystem already ticks
 * "player.iframes" down every frame, so a 3 s i-frame window *is* the effect.
 * Overdrive is the only thing that arms ShipState.buff_id/buff_timer.
 */
inline bool use_consumable(ComponentStorage& storage, EntityManager& entity_manager,
                           Blackboard& blackboard, const ShopConfig& shop) {
    Entity player = 0;
    ShipState* ship = ship_of(storage, player);
    if (ship == nullptr || ship->consumable_id < 0) return false;
    const ShopUpgradeDef* d = consumable_def(shop, ship->consumable_id);
    if (d == nullptr) return false;

    if (d->effect == "repair") {
        if (auto h = storage.get_component<Health>(player); h.has_value())
            h->get().current = std::min(h->get().max_hp, h->get().current + d->amount);
    } else if (d->effect == "overdrive") {
        // No save/restore of fire_rate: PlayerFireSystem multiplies while the
        // buff is live, so a shop purchase during the buff can't desync it.
        ship->buff_id = consumable_ids::OVERDRIVE;
        ship->buff_timer = d->duration;
        blackboard.set<float>("ship.buff_mult", d->amount);
    } else if (d->effect == "emp") {
        for (Entity e : storage.entities_with_component<EnemyTag>()) {
            auto h = storage.get_component<Health>(e);
            if (!h.has_value() || h->get().current <= 0.0f) continue;
            Entity ev = entity_manager.create_entity();
            storage.add_component<DamageEvent>(ev, DamageEvent{e, d->amount});
        }
    } else if (d->effect == "phase_shift") {
        blackboard.set<float>("player.iframes", d->duration);
    } else {
        return false;   // unknown effect: keep the consumable rather than eat it
    }

    ship->consumable_id = -1;
    blackboard.set<std::string>("ship.consumable_name", std::string());
    blackboard.set<std::string>("hud_message", d->name + " used");
    blackboard.set<float>("hud_message_timer", 2.0f);
    return true;
}

}  // namespace items

#endif  // ITEM_SYSTEM_HPP
