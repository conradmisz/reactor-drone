#ifndef PICKUP_SYSTEM_HPP
#define PICKUP_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "arena_config.hpp"       // EconomyConfig
#include "player_components.hpp"  // item_ids

/**
 * PickupSystem — collects dropped loot (D5) and steers it when the Magnet Core
 * item is equipped.
 *
 * Each frame, for every Pickup entity: if the Magnet Core is equipped it is
 * pulled toward the player at Pickup.magnet_speed once inside
 * economy.pickup_magnet_radius (from wave VACUUM_FIRST_WAVE currency pickups get
 * the same pull inside the much shorter VACUUM_RADIUS, item-free); if its centre
 * is within (player radius + pickup
 * radius) it credits the player's ShipState and gets a DestroyRequest plus a
 * one-shot particle pop.
 *
 * Collection is a plain centre-distance test rather than a collision layer.
 * // ponytail: a distance check is 3 lines; a Collider needs a new layer bit, a
 * // mask edit and a CollidedWith sweep. Magnet steering needs the distance
 * // anyway. Move to the collision layers only if pickup counts get large enough
 * // that the O(players x pickups) scan shows up in a frame-time profile.
 *
 * Uncollected loot expires on its own via the Lifetime component set at the drop
 * site, so this system owns no timers.
 */
class PickupSystem {
public:
    /// Item id of the Magnet Core. Aliases the shared constant so the catalogue
    /// mapping in item_system.hpp is the single source of truth.
    static constexpr int ITEM_MAGNET_CORE = item_ids::MAGNET_CORE;

    /// D193: the passive credit vacuum. From this wave on, currency pickups
    /// inside VACUUM_RADIUS drift toward the drone at their own magnet_speed —
    /// a late-run quality-of-life pull, not the Magnet Core's arena-wide reach.
    /// // ponytail: two constants, not a JSON block — nothing else reads them
    /// // and the Magnet Core already owns the tunable long-range version.
    static constexpr int   VACUUM_FIRST_WAVE = 15;
    static constexpr float VACUUM_RADIUS     = 140.0f;  // ~melee reach

    void set_economy(const EconomyConfig& economy) { economy_ = economy; }

    void update(ComponentStorage& component_storage,
                EntityManager& entity_manager,
                Blackboard& blackboard);

private:
    EconomyConfig economy_;
};

#endif // PICKUP_SYSTEM_HPP
