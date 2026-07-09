#include "player_aim_system.hpp"
#include "player_components.hpp"
#include "aim_math.hpp"

void PlayerAimSystem::update(ComponentStorage& storage, const Blackboard& blackboard) {
    // Cursor world position (engine InputSystem stores these as doubles).
    float mx = static_cast<float>(blackboard.get_or<double>("mouse.x", 0.0));
    float my = static_cast<float>(blackboard.get_or<double>("mouse.y", 0.0));

    for (Entity player : storage.entities_with_component<PlayerTag>()) {
        auto pos_opt = storage.get_component<Position>(player);
        auto rot_opt = storage.get_component<Rotation>(player);
        if (!pos_opt.has_value() || !rot_opt.has_value()) continue;

        Position& pos = pos_opt->get();
        // Aim from the player's CENTER (Position is the bottom-left corner).
        float cx = pos.x;
        float cy = pos.y;
        auto size_opt = storage.get_component<Size>(player);
        if (size_opt.has_value()) {
            cx += size_opt->get().width * 0.5f;
            cy += size_opt->get().height * 0.5f;
        }
        rot_opt->get().angle = aim_math::aim_angle(cx, cy, mx, my);
    }
}
