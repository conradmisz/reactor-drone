#include "secondary_fire.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "player_components.hpp"   // PlayerTag, ShipState, WeaponStats
#include "enemy_components.hpp"    // EnemyTag, PathFollower
#include "tower_components.hpp"    // ProjectileTag, ProjectileData, DamageEvent
#include "collision_layers.hpp"
#include "aim_math.hpp"

namespace {

constexpr float TWO_PI = 6.2831853f;

struct Muzzle { float x, y, angle; };

bool player_muzzle(ComponentStorage& s, Entity player, Muzzle& m) {
    auto pos = s.get_component<Position>(player);
    auto rot = s.get_component<Rotation>(player);
    if (!pos.has_value() || !rot.has_value()) return false;
    m.x = pos->get().x; m.y = pos->get().y;
    if (auto z = s.get_component<Size>(player); z.has_value()) {
        m.x += z->get().width * 0.5f;
        m.y += z->get().height * 0.5f;
    }
    m.angle = rot->get().angle;
    return true;
}

/// One secondary projectile — the PlayerFireSystem spawn shape (ribbon visual
/// via ProjectileTag, no Color component — D207/D201 rules hold here too).
Entity spawn_shot(ComponentStorage& s, EntityManager& em, Blackboard& bb,
                  float cx, float cy, float angle, float speed, float damage,
                  float half_size, float lifetime, bool pierce, bool incendiary) {
    Velocity vel = aim_math::velocity_from_angle(angle, speed);
    Entity shot = em.create_entity();
    bb.set<double>("tm.shots", bb.get_or<double>("tm.shots", 0.0) + 1.0);
    const uint8_t sr = static_cast<uint8_t>(bb.get_or<int>("ship.shot_r", 165));
    const uint8_t sg = static_cast<uint8_t>(bb.get_or<int>("ship.shot_g", 35));
    const uint8_t sb = static_cast<uint8_t>(bb.get_or<int>("ship.shot_b", 0));
    s.add_component<Position>(shot, Position{cx - half_size, cy - half_size});
    s.add_component<Velocity>(shot, vel);
    s.add_component<Size>(shot, Size{half_size * 2.0f, half_size * 2.0f});
    s.add_component<Collider>(shot,
        Collider{half_size * 2.0f, half_size * 2.0f, layers::PROJECTILE, layers::PROJECTILE_MASK});
    s.add_component<CircleCollider>(shot, CircleCollider{half_size, 0.0f, 0.0f});
    s.add_component<Lifetime>(shot, Lifetime{lifetime});
    s.add_component<ProjectileTag>(shot, ProjectileTag{
        static_cast<uint8_t>(std::min(255, sr + 90)),
        static_cast<uint8_t>(std::min(255, sg + 35)),
        static_cast<uint8_t>(std::min(255, sb + 60))});
    ProjectileData pd{NO_TARGET, speed, damage, 0, pierce};
    pd.incendiary = incendiary;
    s.add_component<ProjectileData>(shot, pd);
    s.add_component<RenderLayer>(shot, RenderLayer{5});
    return shot;
}

}  // namespace

void tick_secondary_fire(ComponentStorage& storage, EntityManager& em,
                         Blackboard& bb, float dt) {
    const std::string id = bb.get_or<std::string>("weapon.secondary", std::string());
    const float cd_max = bb.get_or<float>("weapon.secondary_cd_max", 10.0f);

    for (Entity player : storage.entities_with_component<PlayerTag>()) {
        auto ship_opt = storage.get_component<ShipState>(player);
        auto wpn_opt  = storage.get_component<WeaponStats>(player);
        if (!ship_opt.has_value() || !wpn_opt.has_value()) return;
        ShipState& ship = ship_opt->get();
        WeaponStats& wpn = wpn_opt->get();

        if (ship.secondary_cd > 0.0f) ship.secondary_cd = std::max(0.0f, ship.secondary_cd - dt);

        // HUD feedback: 1.0 = ready.
        bb.set<float>("ship.secondary_frac",
                      cd_max > 0.0f ? 1.0f - ship.secondary_cd / cd_max : 1.0f);

        // The lava stream keeps emitting after the trigger (it is a burst, not a
        // held beam): while the timer runs, drip slag droplets forward.
        if (ship.stream_timer > 0.0f) {
            ship.stream_timer -= dt;
            ship.stream_acc += dt;
            Muzzle m;
            while (ship.stream_acc >= secondary::STREAM_STEP_S) {
                ship.stream_acc -= secondary::STREAM_STEP_S;
                if (player_muzzle(storage, player, m)) {
                    // ponytail: droplet spread/lifetime are feel numbers, tune in playtest
                    const float jitter = ((ship.stream_seed = ship.stream_seed * 1103515245u + 12345u)
                                          >> 16 & 0x7FFF) / 32768.0f - 0.5f;
                    spawn_shot(storage, em, bb, m.x, m.y, m.angle + jitter * 0.35f,
                               420.0f, 6.0f, 7.0f, 0.5f, false, true);
                }
            }
        }

        const bool held = bb.get_or<bool>("mouse2.held", false);
        const bool jammed = bb.get_or<float>("ship.no_fire", 0.0f) > 0.0f;

        if (id == "charge_shot") {
            if (held && !jammed && ship.secondary_cd <= 0.0f) {
                if (ship.secondary_charge < 0.0f) ship.secondary_charge = 0.0f;
                ship.secondary_charge += dt;
                // Playtest #1 item 7: the charge must be VISIBLE. It drives the
                // same HUD gauge the cooldown uses, so holding right-mouse fills
                // the bar and releasing empties it into the shot.
                bb.set<float>("ship.secondary_frac",
                              secondary::charge_frac(ship.secondary_charge));
                bb.set<bool>("ship.secondary_charging", true);
            } else if (ship.secondary_charge >= 0.0f) {
                // Released (or jammed mid-hold): loose the shot.
                const float frac = secondary::charge_frac(ship.secondary_charge);
                ship.secondary_charge = -1.0f;
                bb.set<bool>("ship.secondary_charging", false);
                Muzzle m;
                if (frac > 0.0f && player_muzzle(storage, player, m)) {
                    // Playtest #1 item 7: a full charge must read as PHYSICALLY
                    // bigger, not a slightly fatter dot. Half-size 10 -> 38 px
                    // (a 76px slug at full hold) and it pierces when charged
                    // past half, so the payoff is visible as well as numeric.
                    spawn_shot(storage, em, bb, m.x, m.y, m.angle,
                               480.0f, wpn.damage * secondary::charge_damage_mult(frac),
                               10.0f + 28.0f * frac, 1.4f, frac >= 0.5f, false);
                    ship.secondary_cd = secondary::charge_cooldown(frac, cd_max);
                }
            }
            return;   // one player
        }

        // Edge-triggered behaviors share one shape: ready + pressed -> fire.
        const bool pressed = held && !ship.secondary_prev_held;
        ship.secondary_prev_held = held;
        if (!pressed || jammed || ship.secondary_cd > 0.0f || id.empty()) return;

        Muzzle m;
        if (!player_muzzle(storage, player, m)) return;

        if (id == "crescent_burst") {
            constexpr int N = 8;
            for (int i = 0; i < N; ++i)
                spawn_shot(storage, em, bb, m.x, m.y, m.angle + TWO_PI * i / N,
                           wpn.projectile_speed, wpn.damage,
                           wpn.projectile_size, 0.6f, true, false);
            ship.secondary_cd = cd_max;
        } else if (id == "lava_stream") {
            ship.stream_timer = secondary::STREAM_S;
            ship.stream_acc = 0.0f;
            ship.secondary_cd = cd_max;
        } else if (id == "blizzard") {
            // One traveling snow ring: big, slow, medium range. No damage —
            // the slow IS the payload.
            Velocity vel = aim_math::velocity_from_angle(m.angle, 180.0f);
            Entity ring = em.create_entity();
            const float R = 70.0f;
            storage.add_component<Position>(ring, Position{m.x - R, m.y - R});
            storage.add_component<Velocity>(ring, vel);
            storage.add_component<Size>(ring, Size{R * 2.0f, R * 2.0f});
            storage.add_component<Lifetime>(ring, Lifetime{2.2f});   // ~400 px of travel
            storage.add_component<BlizzardTag>(ring, BlizzardTag{});
            // Frosty puff so the ring reads on screen without a sprite.
            ParticleEmitter snow;
            snow.shape = EmitterShape::Circle;
            snow.additive = true;
            snow.emission_rate = 160.0f;
            snow.particle_lifetime = 0.5f;
            snow.min_speed = 10.0f; snow.max_speed = 50.0f;
            snow.cone_half_angle = 180.0f;
            snow.radius = R;
            snow.start_r = 190; snow.start_g = 230; snow.start_b = 255; snow.start_a = 200;
            snow.end_r = 90;    snow.end_g = 140;   snow.end_b = 220;   snow.end_a = 0;
            snow.start_size = 4.0f; snow.end_size = 0.0f;
            snow.offset_x = R; snow.offset_y = R;
            storage.add_component<ParticleEmitter>(ring, snow);
            ship.secondary_cd = cd_max;
        }
        return;   // one player
    }
}

void tick_burns_and_chills(ComponentStorage& storage, EntityManager& em, float dt) {
    // Blizzard rings chill everything they overlap (refreshing the timer).
    for (Entity ring : storage.entities_with_component<BlizzardTag>()) {
        auto rp = storage.get_component<Position>(ring);
        auto rz = storage.get_component<Size>(ring);
        if (!rp.has_value() || !rz.has_value()) continue;
        const float rr = rz->get().width * 0.5f;
        const float rx = rp->get().x + rr, ry = rp->get().y + rr;
        const float slow = storage.get_component<BlizzardTag>(ring)->get().slow_mult;
        for (Entity e : storage.entities_with_component<EnemyTag>()) {
            auto ep = storage.get_component<Position>(e);
            auto ez = storage.get_component<Size>(e);
            auto pf = storage.get_component<PathFollower>(e);
            if (!ep.has_value() || !ez.has_value() || !pf.has_value()) continue;
            const float er = ez->get().width * 0.5f;
            const float dx = ep->get().x + er - rx, dy = ep->get().y + ez->get().height * 0.5f - ry;
            if (dx * dx + dy * dy > (rr + er) * (rr + er)) continue;
            if (auto ch = storage.get_component<Chill>(e); ch.has_value()) {
                ch->get().time_left = 1.5f;   // refresh; speed already scaled
            } else {
                Chill c{1.5f, pf->get().speed};
                pf->get().speed *= slow;
                storage.add_component<Chill>(e, c);
            }
        }
    }

    // Chill expiry restores the exact original speed.
    for (Entity e : storage.entities_with_component<Chill>()) {
        auto ch = storage.get_component<Chill>(e);
        ch->get().time_left -= dt;
        if (ch->get().time_left > 0.0f) continue;
        if (auto pf = storage.get_component<PathFollower>(e); pf.has_value())
            pf->get().speed = ch->get().orig_speed;
        storage.remove_component<Chill>(e);
    }

    // Burns: DamageEvents in DPS_TICK_S bites, expiring LINGER seconds after
    // the last exposure (projectile_hit_system refreshes time_left).
    for (Entity e : storage.entities_with_component<Burn>()) {
        auto b = storage.get_component<Burn>(e);
        b->get().time_left -= dt;
        b->get().acc += dt;
        if (b->get().acc >= secondary::DPS_TICK_S) {
            b->get().acc -= secondary::DPS_TICK_S;
            Entity ev = em.create_entity();
            storage.add_component<DamageEvent>(ev,
                DamageEvent{e, b->get().dps * secondary::DPS_TICK_S});
        }
        if (b->get().time_left <= 0.0f) storage.remove_component<Burn>(e);
    }
}
