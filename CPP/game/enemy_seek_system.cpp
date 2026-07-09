#include "enemy_seek_system.hpp"
#include "enemy_components.hpp"   // EnemyTag, PathFollower (reused for speed)
#include "player_components.hpp"  // PlayerTag
#include <cmath>

namespace {
// Center of an entity given its bottom-left Position and optional Size.
bool entity_center(ComponentStorage& storage, Entity e, float& cx, float& cy) {
    auto pos = storage.get_component<Position>(e);
    if (!pos.has_value()) return false;
    cx = pos->get().x;
    cy = pos->get().y;
    auto size = storage.get_component<Size>(e);
    if (size.has_value()) {
        cx += size->get().width * 0.5f;
        cy += size->get().height * 0.5f;
    }
    return true;
}
} // namespace

void EnemySeekSystem::update(ComponentStorage& storage) {
    // Locate the player (there is exactly one; bail if none).
    float px = 0.0f, py = 0.0f;
    bool have_player = false;
    for (Entity p : storage.entities_with_component<PlayerTag>()) {
        if (entity_center(storage, p, px, py)) { have_player = true; break; }
    }
    if (!have_player) return;

    for (Entity enemy : storage.entities_with_component<EnemyTag>()) {
        auto vel_opt = storage.get_component<Velocity>(enemy);
        auto pf_opt = storage.get_component<PathFollower>(enemy);
        if (!vel_opt.has_value() || !pf_opt.has_value()) continue;

        float ex = 0.0f, ey = 0.0f;
        if (!entity_center(storage, enemy, ex, ey)) continue;

        float dx = px - ex;
        float dy = py - ey;
        float dist = std::sqrt(dx * dx + dy * dy);
        float speed = pf_opt->get().speed;
        Velocity& vel = vel_opt->get();
        if (dist < 0.0001f) {
            vel.dx = 0.0f;
            vel.dy = 0.0f;
        } else {
            vel.dx = dx / dist * speed;
            vel.dy = dy / dist * speed;
        }
    }
}
