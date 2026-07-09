#include "experience_system.hpp"
#include "player_components.hpp"  // PlayerTag, Experience

void ExperienceSystem::update(ComponentStorage& storage, Blackboard& blackboard) {
    int pending_xp = blackboard.get_or<int>("pending_xp", 0);

    for (Entity player : storage.entities_with_component<PlayerTag>()) {
        auto exp_opt = storage.get_component<Experience>(player);
        if (!exp_opt.has_value()) continue;
        Experience& exp = exp_opt->get();

        exp.xp += static_cast<float>(pending_xp);

        int levels_gained = 0;
        // threshold guard: a non-positive threshold would loop forever.
        while (exp.threshold > 0.0f && exp.xp >= exp.threshold) {
            exp.xp -= exp.threshold;
            exp.level += 1;
            exp.threshold *= exp.multiplier;
            levels_gained += 1;
        }

        if (levels_gained > 0) {
            int pending = blackboard.get_or<int>("pending_upgrades", 0);
            blackboard.set("pending_upgrades", pending + levels_gained);
        }
        blackboard.set("level", exp.level);
        blackboard.set<float>("xp", exp.xp);
        blackboard.set<float>("xp_threshold", exp.threshold);
    }

    blackboard.set("pending_xp", 0);  // consumed
}
