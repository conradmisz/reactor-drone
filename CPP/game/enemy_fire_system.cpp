#include "enemy_fire_system.hpp"

#include "collision_layers.hpp"
#include "player_components.hpp"   // PlayerTag, ContactDamage
#include "aim_math.hpp"
#include <algorithm>

namespace enemy_fire {

Entity spawn_shot(ComponentStorage& storage, EntityManager& entity_manager,
                  float cx, float cy, float angle, float speed, float damage,
                  int tier) {
    const ShotSpec spec = shot_spec(tier);
    const float r = spec.radius;

    Entity shot = entity_manager.create_entity();
    storage.add_component<Position>(shot, Position{cx - r, cy - r});
    storage.add_component<Velocity>(shot, aim_math::velocity_from_angle(angle, speed));
    storage.add_component<Size>(shot, Size{r * 2.0f, r * 2.0f});
    // v3 Tier 7: no Color — the ribbon is the shot (colour on EnemyShot).
    storage.add_component<Collider>(shot,
        Collider{r * 2.0f, r * 2.0f, layers::ENEMY_SHOT, layers::ENEMY_SHOT_MASK});
    storage.add_component<CircleCollider>(shot, CircleCollider{r, 0.0f, 0.0f});
    // Score/currency 0: a shot is not a kill, so it must never reward one. This
    // is the whole damage path — PlayerDamageSystem picks it up off CollidedWith.
    storage.add_component<ContactDamage>(shot, ContactDamage{damage, 0, 0, 1.0f});
    storage.add_component<Lifetime>(shot, Lifetime{spec.lifetime});
    storage.add_component<EnemyShot>(shot, EnemyShot{spec.r, spec.g, spec.b});
    // The tier rides on the shot itself so the expire step below knows whether it
    // pierces, without a second component type (the D17/ShipState discipline).
    storage.add_component<EnemyBehavior>(shot,
        EnemyBehavior{behavior_kinds::SHOOTER, tier, 0.0f, 0.0f, angle});
    storage.add_component<RenderLayer>(shot, RenderLayer{5});

    // Trail. Deliberately less than half the player shot's 70/s x 0.35s: enemy
    // shots outnumber player shots badly by wave 30 and the global budget is
    // 2000 (ENGINE.md §5). 40/s x 0.25s is ~10 live particles per shot.
    ParticleEmitter trail;
    trail.shape = EmitterShape::Point;
    trail.additive = true;
    trail.emission_rate = 40.0f;
    trail.particle_lifetime = 0.25f;
    trail.min_speed = 0.0f;
    trail.max_speed = 18.0f;
    trail.cone_half_angle = 180.0f;
    trail.start_r = spec.r; trail.start_g = spec.g; trail.start_b = spec.b; trail.start_a = 210;
    trail.end_r = 60; trail.end_g = 20; trail.end_b = 70; trail.end_a = 0;
    trail.start_size = r; trail.end_size = 0.0f;
    trail.offset_x = r; trail.offset_y = r;
    // v3 Tier 12: not attached — the ribbon is the shot. See player_fire_system.
    (void)trail;

    return shot;
}

const EnemyType* type_for(const GameConfig* cfg, int kind, int tier) {
    if (cfg == nullptr) return nullptr;
    const EnemyType* fallback = nullptr;
    for (const EnemyType& t : cfg->enemy_types) {
        if (behavior_kind_for(t.behavior) != kind) continue;
        if (t.behavior_tier == tier) return &t;
        if (fallback == nullptr) fallback = &t;
    }
    return fallback;
}

bool player_centre(ComponentStorage& storage, float& px, float& py) {
    for (Entity p : storage.entities_with_component<PlayerTag>()) {
        auto pos = storage.get_component<Position>(p);
        auto sz = storage.get_component<Size>(p);
        if (!pos.has_value() || !sz.has_value()) continue;
        px = pos->get().x + sz->get().width * 0.5f;
        py = pos->get().y + sz->get().height * 0.5f;
        return true;
    }
    return false;
}

}  // namespace enemy_fire

void EnemyFireSystem::update(ComponentStorage& storage, EntityManager& entity_manager,
                             Blackboard& blackboard) {
    if (!blackboard.has("delta_time")) return;
    const float dt = static_cast<float>(blackboard.get<double>("delta_time"));

    float px = 0.0f, py = 0.0f;
    const bool have_player = enemy_fire::player_centre(storage, px, py);

    for (Entity e : storage.entities_with_component<EnemyBehavior>()) {
        // Shots carry an EnemyBehavior too (it is how they remember their tier).
        // They are not shooters.
        if (storage.has_component<EnemyShot>(e)) continue;
        auto beh_opt = storage.get_component<EnemyBehavior>(e);
        if (!beh_opt.has_value()) continue;
        EnemyBehavior& beh = beh_opt->get();
        if (beh.kind != behavior_kinds::SHOOTER) continue;

        auto pos = storage.get_component<Position>(e);
        auto sz = storage.get_component<Size>(e);
        if (!pos.has_value() || !sz.has_value()) continue;
        const float cx = pos->get().x + sz->get().width * 0.5f;
        const float cy = pos->get().y + sz->get().height * 0.5f;

        const enemy_fire::ShotSpec spec = enemy_fire::shot_spec(beh.tier);

        // Tier 2 tracks: the muzzle angle turns toward the drone at a clamped
        // rate, so a moving player can out-turn it. Tiers 1 and 3 snap to the
        // firing solution at the moment they fire.
        if (have_player) {
            const float want = std::atan2(py - cy, px - cx);
            beh.aim = enemy_fire::turn_toward(beh.aim, want, spec.turn_rate * dt);
        }

        // #3 (D109): the crescent has to POINT where it shoots. Enemies carry no
        // Rotation by default, so the moon was drawn mouth-right forever and a
        // shot leaving the mouth was only true when the player stood to its
        // right. Pure rotation (flip_when_left = false) — the art is symmetric
        // about its own axis. Render-only: nothing reads Rotation back.
        if (auto rot = storage.get_component<Rotation>(e); rot.has_value())
            rot->get().angle = beh.aim;
        else
            storage.add_component<Rotation>(e, Rotation{beh.aim, 0.0f, false});

        if (beh.timer > 0.0f) beh.timer -= dt;
        if (beh.timer > 0.0f || !have_player) continue;
        beh.timer = beh.cooldown;

        // Shot speed and damage come from the type row, found by kind+tier rather
        // than stored per entity — one small config scan against one more
        // component field on every enemy in the game.
        float speed = 260.0f, damage = 8.0f;
        if (const EnemyType* t = enemy_fire::type_for(cfg_, behavior_kinds::SHOOTER, beh.tier)) {
            speed = t->shot_speed;
            damage = t->shot_damage;
        }
        const float angle = spec.turn_rate > 0.0f ? beh.aim
                                                  : std::atan2(py - cy, px - cx);
        // Fire from the crescent's mouth, not the centre (#3, D109).
        const float mz = enemy_fire::moon_muzzle_frac(beh.tier) * sz->get().width;
        enemy_fire::spawn_shot(storage, entity_manager,
                               cx + std::cos(angle) * mz, cy + std::sin(angle) * mz,
                               angle, speed * spec.speed_mult, damage, beh.tier);
    }

    // A shot dies on the thing it hit. The laser (tier 3) is the exception: it
    // pierces, which is exactly why it is the tier-3 upgrade.
    for (Entity s : storage.entities_with_component<EnemyShot>()) {
        if (storage.has_component<DestroyRequest>(s)) continue;
        auto col = storage.get_component<CollidedWith>(s);
        if (!col.has_value() || col->get().entities.empty()) continue;
        auto beh = storage.get_component<EnemyBehavior>(s);
        if (beh.has_value() && enemy_fire::shot_spec(beh->get().tier).pierce) continue;
        storage.add_component<DestroyRequest>(s, DestroyRequest{});
    }
}
