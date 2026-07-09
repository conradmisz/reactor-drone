#include "upgrade_system.hpp"
#include "player_components.hpp"  // PlayerTag, WeaponStats
#include "enemy_components.hpp"   // Health
#include <algorithm>

size_t UpgradeSystem::pick_index(const std::vector<Upgrade>& pool, float roll01) {
    if (pool.empty()) return 0;
    float total = 0.0f;
    for (const auto& u : pool) total += std::max(0.0f, u.weight);
    if (total <= 0.0f) return 0;

    float target = std::clamp(roll01, 0.0f, 0.999999f) * total;
    float acc = 0.0f;
    for (size_t i = 0; i < pool.size(); ++i) {
        acc += std::max(0.0f, pool[i].weight);
        if (target < acc) return i;
    }
    return pool.size() - 1;
}

void UpgradeSystem::update(ComponentStorage& storage, Blackboard& blackboard) {
    int pending = blackboard.get_or<int>("pending_upgrades", 0);
    if (pending <= 0 || pool_.empty()) return;

    std::uniform_real_distribution<float> roll(0.0f, 1.0f);

    for (Entity player : storage.entities_with_component<PlayerTag>()) {
        for (int i = 0; i < pending; ++i) {
            const Upgrade& up = pool_[pick_index(pool_, roll(rng_))];

            auto wpn = storage.get_component<WeaponStats>(player);
            auto hp  = storage.get_component<Health>(player);

            if (up.stat == "fire_rate" && wpn.has_value()) {
                wpn->get().fire_rate += up.amount;
            } else if (up.stat == "damage" && wpn.has_value()) {
                wpn->get().damage += up.amount;
            } else if (up.stat == "projectile_speed" && wpn.has_value()) {
                wpn->get().projectile_speed += up.amount;
            } else if (up.stat == "spread" && wpn.has_value()) {
                wpn->get().spread = std::max(0.0f, wpn->get().spread - up.amount);
            } else if (up.stat == "max_health" && hp.has_value()) {
                hp->get().max_hp += up.amount;
                hp->get().current = std::min(hp->get().max_hp, hp->get().current + up.amount);
            }

            blackboard.set<std::string>("upgrade_message", "LEVEL UP! " + up.label);
            blackboard.set<float>("upgrade_message_timer", 2.5f);
        }
        break;  // single player
    }

    blackboard.set("pending_upgrades", 0);  // consumed
}
