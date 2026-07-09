#ifndef UPGRADE_SYSTEM_HPP
#define UPGRADE_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "arena_config.hpp"
#include <vector>
#include <random>

/**
 * UpgradeSystem — on each queued level-up, picks a weighted-random upgrade from
 * the pool and applies it to the player, flashing a HUD message.
 *
 * Consumes Blackboard "pending_upgrades" (set by ExperienceSystem). For each
 * pending level-up it weighted-random-picks an Upgrade and mutates the player's
 * WeaponStats / Health, then writes "upgrade_message" + "upgrade_message_timer"
 * to the Blackboard for GameHUDSystem to display (the design's no-real-UI
 * mitigation: auto-apply + flash text).
 */
class UpgradeSystem {
public:
    UpgradeSystem() : rng_(1234u) {}

    void set_pool(const std::vector<Upgrade>& pool, unsigned int seed) {
        pool_ = pool;
        rng_.seed(seed);
    }

    void update(ComponentStorage& storage, Blackboard& blackboard);

    /// Pure helper: index of the upgrade selected by a roll in [0,1) over the
    /// cumulative weight distribution. Weights are treated as max(0, weight).
    /// Returns 0 for an empty/zero-weight pool.
    static size_t pick_index(const std::vector<Upgrade>& pool, float roll01);

private:
    std::vector<Upgrade> pool_;
    std::mt19937 rng_;
};

#endif // UPGRADE_SYSTEM_HPP
