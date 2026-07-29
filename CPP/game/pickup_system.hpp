#ifndef PICKUP_SYSTEM_HPP
#define PICKUP_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "arena_config.hpp"       // EconomyConfig

/**
 * PickupSystem — collects dropped loot (D5) and steers it when the Magnet Core
 * item is equipped.
 *
 * Each frame, for every Pickup entity: if the Magnet Core is equipped it is
 * pulled toward the player at Pickup.magnet_speed once inside
 * economy.pickup_magnet_radius; if its centre is within (player radius + pickup
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
    /// Item id of the Magnet Core in the Phase 4 item catalogue.
    static constexpr int ITEM_MAGNET_CORE = 0;

    void set_economy(const EconomyConfig& economy) { economy_ = economy; }

    void update(ComponentStorage& component_storage,
                EntityManager& entity_manager,
                Blackboard& blackboard);

private:
    EconomyConfig economy_;
};

#endif // PICKUP_SYSTEM_HPP
