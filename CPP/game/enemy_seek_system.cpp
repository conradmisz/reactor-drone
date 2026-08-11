#include "enemy_seek_system.hpp"
#include "enemy_components.hpp"   // EnemyTag, PathFollower (seek speed + repath state)
#include "player_components.hpp"  // PlayerTag
#include "enemy_path.hpp"
#include "engine/pathfinding.hpp"
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

// Point the velocity at (tx,ty) at `speed`; zero it if already on the target.
void steer_toward(Velocity& vel, float ex, float ey, float tx, float ty, float speed) {
    float dx = tx - ex, dy = ty - ey;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 0.0001f) {
        vel.dx = 0.0f;
        vel.dy = 0.0f;
    } else {
        vel.dx = dx / dist * speed;
        vel.dy = dy / dist * speed;
    }
}

// D186: every enemy faces its travel direction, the way the moon faces its aim
// (D109). All v2 enemy art carries a +X front (dart nose, clamp jaws, barrel);
// without a Rotation it rendered pointing right forever. Pure rotation
// (flip_when_left = false), render-only, written from the velocity just
// steered — a stationary enemy keeps its last heading rather than snapping to 0.
// Shooters overwrite this later in the frame with their aim (enemy_fire,
// specialty), which is the better face for a thing that is firing at you.
void face_velocity(ComponentStorage& storage, Entity e, const Velocity& vel) {
    if (vel.dx == 0.0f && vel.dy == 0.0f) return;
    const float ang = std::atan2(vel.dy, vel.dx);
    if (auto rot = storage.get_component<Rotation>(e); rot.has_value())
        rot->get().angle = ang;
    else
        storage.add_component<Rotation>(e, Rotation{ang, 0.0f, false});
}
} // namespace

void EnemySeekSystem::update(ComponentStorage& storage, Blackboard& blackboard) {
    // Locate the player (there is exactly one; bail if none).
    float px = 0.0f, py = 0.0f;
    bool have_player = false;
    for (Entity p : storage.entities_with_component<PlayerTag>()) {
        if (entity_center(storage, p, px, py)) { have_player = true; break; }
    }
    if (!have_player) return;

    const float dt = static_cast<float>(blackboard.get_or<double>("delta_time", 0.0));
    const bool have_grid = grid_ && obstacles_ && !obstacles_->empty();

    for (Entity enemy : storage.entities_with_component<EnemyTag>()) {
        auto vel_opt = storage.get_component<Velocity>(enemy);
        auto pf_opt = storage.get_component<PathFollower>(enemy);
        if (!vel_opt.has_value() || !pf_opt.has_value()) continue;

        float ex = 0.0f, ey = 0.0f;
        if (!entity_center(storage, enemy, ex, ey)) continue;

        Velocity& vel = vel_opt->get();
        PathFollower& pf = pf_opt->get();
        const float speed = pf.speed;

        // Enemy radius inflates the LOS test so it starts pathing just before a
        // corner clip (0 if the enemy has no Size).
        float radius = 0.0f;
        if (auto sz = storage.get_component<Size>(enemy); sz.has_value()) {
            radius = sz->get().width * 0.5f;
        }

        // Fast path: no grid, or a clear straight shot -> steer straight at the
        // player. Arm the repath timer so the next blocked frame recomputes at once.
        if (!have_grid ||
            enemy_path::line_of_sight_clear(ex, ey, px, py, *obstacles_, radius)) {
            steer_toward(vel, ex, ey, px, py, speed);
            face_velocity(storage, enemy, vel);
            pf.repath_timer = 0.0f;
            continue;
        }

        // Blocked: repath at most every repath_interval_ seconds, otherwise reuse
        // the cached target cell centre.
        pf.repath_timer -= dt;
        if (pf.repath_timer <= 0.0f) {
            PathGrid pg(grid_.get());
            GridCoord start = grid_->world_to_tile(ex, ey);
            GridCoord goal = grid_->world_to_tile(px, py);
            std::vector<GridCoord> path = find_path(pg, start, goal);
            if (path.size() >= 2) {
                enemy_path::cell_center(path[1].col, path[1].row, grid_->tile_size,
                                        pf.target_x, pf.target_y);
            } else {
                // No route (or already in the player's cell): head straight at them.
                pf.target_x = px;
                pf.target_y = py;
            }
            pf.repath_timer = repath_interval_;
        }

        steer_toward(vel, ex, ey, pf.target_x, pf.target_y, speed);
        face_velocity(storage, enemy, vel);
    }
}
