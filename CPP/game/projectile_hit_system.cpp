#include "projectile_hit_system.hpp"
#include "tower_components.hpp"   // ProjectileTag, ProjectileData, DamageEvent
#include "enemy_components.hpp"   // EnemyTag
#include "player_components.hpp"  // Flash
#include "collision_layers.hpp"   // OBSTACLE
#include "bullet_bounce.hpp"      // bounce::off_aabb / inside_circle

namespace {

/// Centre + radius of an entity drawn from a bottom-left Position and a Size.
/// Returns false if either is missing.
bool circle_of(ComponentStorage& s, Entity e, float& cx, float& cy, float& r) {
    auto p = s.get_component<Position>(e);
    auto z = s.get_component<Size>(e);
    if (!p.has_value() || !z.has_value()) return false;
    r = z->get().width * 0.5f;
    cx = p->get().x + r;
    cy = p->get().y + z->get().height * 0.5f;
    return true;
}

/// Write a reflected centre + velocity back onto the projectile.
void apply_bounce(ComponentStorage& s, Entity proj, const bounce::Result& b, float r) {
    if (auto p = s.get_component<Position>(proj); p.has_value()) {
        p->get().x = b.cx - r;
        p->get().y = b.cy - r;
    }
    if (auto v = s.get_component<Velocity>(proj); v.has_value()) {
        v->get().dx = b.vx;
        v->get().dy = b.vy;
    }
}

}  // namespace

// Arena shooter hit detection: projectiles fly straight (moved by the engine
// MovementSystem) and the engine CollisionSystem reports overlaps via
// CollidedWith. For each projectile that overlapped an enemy this frame, queue a
// DamageEvent on that enemy and destroy the projectile (one enemy per shot).
//
// D98: a shot with ProjectileData.bounces > 0 reflects off obstacles and the
// arena ring instead of dying on them, spending one bounce per surface. Enemies
// always win over a surface in the same frame — a ricochet must never eat a hit.
void ProjectileHitSystem::update(EntityManager& entity_manager,
                                 ComponentStorage& component_storage,
                                 Blackboard& blackboard) {
    const float fdur = blackboard.get_or<float>("fb.flash_duration", 0.12f);
    const uint8_t fr = static_cast<uint8_t>(blackboard.get_or<int>("fb.enemy_flash_r", 255));
    const uint8_t fg = static_cast<uint8_t>(blackboard.get_or<int>("fb.enemy_flash_g", 255));
    const uint8_t fb = static_cast<uint8_t>(blackboard.get_or<int>("fb.enemy_flash_b", 255));

    auto projectiles = component_storage.entities_with_component<ProjectileTag>();

    for (Entity proj : projectiles) {
        if (component_storage.has_component<DestroyRequest>(proj)) continue;

        auto data_opt = component_storage.get_component<ProjectileData>(proj);
        if (!data_opt.has_value()) continue;
        ProjectileData& data = data_opt->get();

        auto collided = component_storage.get_component<CollidedWith>(proj);
        Entity hit_enemy = NO_TARGET;
        Entity hit_solid = NO_TARGET;
        if (collided.has_value()) {
            for (Entity other : collided->get().entities) {
                if (component_storage.has_component<EnemyTag>(other) &&
                    entity_manager.is_alive(other)) {
                    hit_enemy = other;
                    // telemetry: write-only observation, nothing in the sim reads tm.*
                    // (the player.hit_bearing precedent) — cannot move the replay canary.
                    blackboard.set<double>("tm.hits", blackboard.get_or<double>("tm.hits", 0.0) + 1.0);
                    break;   // one hit per projectile, and an enemy outranks a wall
                }
                if (auto col = component_storage.get_component<Collider>(other);
                    hit_solid == NO_TARGET && col.has_value() &&
                    (col->get().layer & layers::OBSTACLE)) {
                    hit_solid = other;
                }
            }
        }

        if (hit_enemy != NO_TARGET) {
            Entity event_entity = entity_manager.create_entity();
            component_storage.add_component<DamageEvent>(
                event_entity, DamageEvent{hit_enemy, data.damage});

            // v2 Phase 4: flash the struck enemy (overwrites any in-flight flash,
            // resetting its clock — repeated hits keep it lit).
            component_storage.add_component<Flash>(hit_enemy, Flash{fdur, fdur, fr, fg, fb});

            // v2: additive impact spark burst at the projectile's position.
            if (auto ppos = component_storage.get_component<Position>(proj); ppos.has_value()) {
                Entity burst = entity_manager.create_entity();
                component_storage.add_component<Position>(burst,
                    Position{ppos->get().x, ppos->get().y});
                ParticleEmitter e;
                e.shape = EmitterShape::Point;
                e.additive = true;
                e.emission_rate = 400.0f;
                e.particle_lifetime = 0.25f;
                e.min_speed = 40.0f; e.max_speed = 160.0f;
                e.cone_half_angle = 180.0f;
                e.start_r = 180; e.start_g = 245; e.start_b = 255; e.start_a = 255;
                e.end_r = 40;    e.end_g = 90;    e.end_b = 160;   e.end_a = 0;
                e.start_size = 5.0f; e.end_size = 0.0f;
                component_storage.add_component<ParticleEmitter>(burst, e);
                component_storage.add_component<Lifetime>(burst, Lifetime{0.06f});
            }

            component_storage.add_component<DestroyRequest>(proj, DestroyRequest{});
            continue;
        }

        float cx = 0.0f, cy = 0.0f, r = 0.0f;
        const bool have_circle = circle_of(component_storage, proj, cx, cy, r);
        auto vel = component_storage.get_component<Velocity>(proj);

        // v2 Phase 6: a shot that meets a solid obstacle stops dead — unless it
        // has ricochets left (D98), in which case it reflects and spends one.
        if (hit_solid != NO_TARGET) {
            // Engine suite (D146): a solid carrying Health is DESTRUCTIBLE, so the
            // shot's damage is routed into it through the same DamageEvent ->
            // DamageApplySystem path enemies use, rather than through a second
            // damage path CrumbleSystem would own. An obstacle with no Health (the
            // shipped default, `hp: 0`) is untouched and this is one failed lookup.
            if (component_storage.has_component<Health>(hit_solid)) {
                Entity ev = entity_manager.create_entity();
                component_storage.add_component<DamageEvent>(ev,
                    DamageEvent{hit_solid, data.damage});
            }
            bounce::Result b;
            auto opos = component_storage.get_component<Position>(hit_solid);
            auto osz = component_storage.get_component<Size>(hit_solid);
            if (data.bounces <= 0) {
                component_storage.add_component<DestroyRequest>(proj, DestroyRequest{});
            } else if (have_circle && vel.has_value() && opos.has_value() && osz.has_value() &&
                       bounce::off_aabb(cx, cy, r, vel->get().dx, vel->get().dy,
                                        opos->get().x, opos->get().y,
                                        osz->get().width, osz->get().height, b)) {
                --data.bounces;
                apply_bounce(component_storage, proj, b, r);
            }
            // A shot with bounces left that the helper refused (it is clear of the
            // box, or already leaving it) is left alone rather than destroyed —
            // that is what stops a ricochet burning its whole budget, or dying, on
            // the surface it has just pushed off.
            continue;
        }

        // The arena ring is a clamp, not a collider (main.cpp), so there is no
        // CollidedWith to read — a ricocheting shot tests the boundary itself.
        // Shots without bounces keep flying out and expiring on Lifetime, exactly
        // as before.
        if (data.bounces > 0 && arena_ != nullptr && have_circle && vel.has_value()) {
            bounce::Result b;
            if (bounce::inside_circle(cx, cy, r, vel->get().dx, vel->get().dy,
                                      arena_->center_x, arena_->center_y,
                                      arena_->radius, b)) {
                --data.bounces;
                apply_bounce(component_storage, proj, b, r);
            }
        }
    }
}
