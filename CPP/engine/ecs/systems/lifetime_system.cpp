#include "engine/ecs/systems/lifetime_system.hpp"

void LifetimeSystem::update(ComponentStorage& storage, const Blackboard& blackboard) {
    double delta_time = blackboard.get<double>("delta_time");
    float dt_f = static_cast<float>(delta_time);

    auto entities = storage.entities_with_component<Lifetime>();
    for (Entity entity : entities) {
        auto lifetime_opt = storage.get_component<Lifetime>(entity);
        if (!lifetime_opt.has_value()) {
            continue;
        }
        Lifetime& lifetime = lifetime_opt->get();

        lifetime.remaining -= dt_f;

        if (lifetime.remaining <= 0.0f) {
            storage.add_component(entity, DestroyRequest{});
        }
    }
}
