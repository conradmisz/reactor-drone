#include "engine/ecs/systems/wrap_system.hpp"

void WrapSystem::update(ComponentStorage& storage, const Blackboard& blackboard) {
    float world_x = blackboard.get_or<float>("world.x", 0.0f);
    float world_y = blackboard.get_or<float>("world.y", 0.0f);
    float world_w = blackboard.get_or<float>("world.width", 0.0f);
    float world_h = blackboard.get_or<float>("world.height", 0.0f);

    if (world_w <= 0 || world_h <= 0) {
        return;
    }

    auto entities = storage.entities_with_component<WrapAround>();
    for (Entity entity : entities) {
        auto pos_opt = storage.get_component<Position>(entity);
        if (!pos_opt.has_value()) {
            continue;
        }
        Position& pos = pos_opt->get();

        float w = 0.0f;
        float h = 0.0f;
        auto size_opt = storage.get_component<Size>(entity);
        if (size_opt.has_value()) {
            w = size_opt->get().width;
            h = size_opt->get().height;
        }

        // X-axis wrapping (independent of Y)
        if (pos.x >= world_x + world_w) {
            pos.x = world_x - w;
        } else if (pos.x + w <= world_x) {
            pos.x = world_x + world_w;
        }

        // Y-axis wrapping (independent of X)
        if (pos.y >= world_y + world_h) {
            pos.y = world_y - h;
        } else if (pos.y + h <= world_y) {
            pos.y = world_y + world_h;
        }
    }
}
