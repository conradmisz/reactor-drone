#include "engine/ecs/systems/rotation_system.hpp"

void RotationSystem::update(ComponentStorage& storage, const Blackboard& blackboard) {
    double delta_time = blackboard.get<double>("delta_time");
    float dt = static_cast<float>(delta_time);

    auto entities = storage.entities_with_component<Rotation>();
    for (Entity entity : entities) {
        auto rotation_opt = storage.get_component<Rotation>(entity);
        if (!rotation_opt.has_value()) {
            continue;
        }
        Rotation& rotation = rotation_opt->get();

        rotation.angle += rotation.angular_velocity * dt;
    }
}
