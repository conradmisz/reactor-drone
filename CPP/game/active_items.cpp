#include "active_items.hpp"

#include "aim_math.hpp"
#include "collision_layers.hpp"
#include "enemy_components.hpp"
#include "item_system.hpp"        // ship_of, push_enemies_out
#include "player_components.hpp"
#include "tower_components.hpp"   // ProjectileTag/Data, DamageEvent, BeamTag, NO_TARGET
#include <algorithm>
#include <cmath>

namespace actives {
namespace {

constexpr float TAU = 6.28318530717958647692f;

// --- missiles -------------------------------------------------------------
constexpr int   MISSILE_COUNT   = 8;
constexpr float MISSILE_SPEED   = 420.0f;
constexpr float MISSILE_TURN    = 3.4f;    // radians/sec of homing
constexpr float MISSILE_R       = 11.0f;
constexpr float MISSILE_AOE     = 200.0f;  // detonation radius
// Proximity fuse. It must trip BEFORE the rocket physically touches its target,
// or ProjectileHitSystem takes it on contact and the blast never happens — that
// path pays single-target damage, not the AoE below. Bigger rocket (R 7 -> 11)
// means contact happens sooner, so the fuse moved out with it.
constexpr float MISSILE_FUSE    = 56.0f;   // detonate this close to the target
constexpr float MISSILE_SALVO   = 0.45f;   // gap before the second wave

// --- laser ----------------------------------------------------------------
constexpr int   BEAM_COUNT      = 4;
constexpr float BEAM_LENGTH     = 620.0f;
constexpr float BEAM_WIDTH      = 26.0f;
constexpr float BEAM_HOLD       = 0.9f;    // "set" before the sweep
constexpr float BEAM_SWEEP_RATE = 7.5f;    // radians/sec once sweeping

// Blackboard keys. Presentation-free simulation state, so they belong here and
// not on a component (D28/D41).
constexpr const char* K_BEAM_T   = "active.beam_t";     // <0 = no beam live
constexpr const char* K_BEAM_A   = "active.beam_angle";
constexpr const char* K_BEAM_DMG = "active.beam_dps";
constexpr const char* K_FIELD_T  = "active.field_t";    // seconds of sphere left
constexpr const char* K_FIELD_R  = "active.field_radius";
constexpr const char* K_FIELD_P  = "active.field_push";
constexpr const char* K_CD_MULT  = "ship.active_cd_mult";  // boss-reward upgrades
constexpr const char* K_SALVO_T  = "active.salvo_t";    // <=0 = no wave pending

bool centre_of(ComponentStorage& s, Entity e, float& cx, float& cy) {
    auto p = s.get_component<Position>(e);
    auto z = s.get_component<Size>(e);
    if (!p.has_value() || !z.has_value()) return false;
    cx = p->get().x + z->get().width * 0.5f;
    cy = p->get().y + z->get().height * 0.5f;
    return true;
}

Entity nearest_enemy(ComponentStorage& s, float x, float y) {
    Entity best = NO_TARGET;
    float best_d = 0.0f;
    for (Entity e : s.entities_with_component<EnemyTag>()) {
        float ex, ey;
        if (!centre_of(s, e, ex, ey)) continue;
        const float d = (ex - x) * (ex - x) + (ey - y) * (ey - y);
        if (best == NO_TARGET || d < best_d) { best = e; best_d = d; }
    }
    return best;
}

/// `reach` is how far the particles travel in their 0.4 s life — the visual
/// radius. Keep it equal to whatever gameplay radius the burst is standing in
/// for, or the blast lies about its own size.
void burst(ComponentStorage& s, EntityManager& em, float x, float y,
           float rate, float life, uint8_t r, uint8_t g, uint8_t b,
           float reach = 120.0f, float size = 8.0f) {
    Entity host = em.create_entity();
    s.add_component<Position>(host, Position{x, y});
    ParticleEmitter e;
    e.shape = EmitterShape::Point;
    e.additive = true;
    e.emission_rate = rate;
    e.particle_lifetime = 0.4f;
    e.min_speed = reach * 0.2f / e.particle_lifetime;
    e.max_speed = reach / e.particle_lifetime;
    e.cone_half_angle = 180.0f;
    e.start_size = size; e.end_size = 0.0f;
    e.start_r = r; e.start_g = g; e.start_b = b; e.start_a = 255;
    e.end_r = 255; e.end_g = 255; e.end_b = 255; e.end_a = 0;
    s.add_component<ParticleEmitter>(host, e);
    s.add_component<Lifetime>(host, Lifetime{life});
}

/// `spin` offsets the fan; the second salvo passes half a step so the two waves
/// interleave instead of flying the first wave's lines again.
void launch_missiles(ComponentStorage& s, EntityManager& em,
                     float px, float py, const ActiveItemDef& def, float spin = 0.0f) {
    for (int i = 0; i < MISSILE_COUNT; ++i) {
        const float a = spin + TAU * static_cast<float>(i) / static_cast<float>(MISSILE_COUNT);
        Entity m = em.create_entity();
        s.add_component<Position>(m, Position{px - MISSILE_R, py - MISSILE_R});
        s.add_component<Velocity>(m, aim_math::velocity_from_angle(a, MISSILE_SPEED));
        s.add_component<Size>(m, Size{MISSILE_R * 2.0f, MISSILE_R * 2.0f});
        s.add_component<Color>(m, Color{255, 190, 90, 255});
        // A rocket, not a dot: the sprite is authored facing right and the
        // heading below turns it, so flip_when_left must stay off (D2 art rule).
        s.add_component<Images>(m, Images{{"v2/projectile_rocket.png"}, 0});
        s.add_component<Rotation>(m, Rotation{a, 0.0f, false});
        s.add_component<Collider>(m, Collider{MISSILE_R * 2.0f, MISSILE_R * 2.0f,
                                              layers::PROJECTILE, layers::PROJECTILE_MASK});
        s.add_component<CircleCollider>(m, CircleCollider{MISSILE_R, 0.0f, 0.0f});
        s.add_component<Lifetime>(m, Lifetime{def.duration});
        s.add_component<ProjectileTag>(m, ProjectileTag{});
        // The target doubles as the "this is a missile" marker: a player shot
        // always carries NO_TARGET (PlayerFireSystem), so a live target is the
        // one thing that distinguishes the two without a new component type.
        // ponytail: a missile whose target dies stops homing and flies straight.
        // Re-acquiring would need a real marker; the 4 s fuse bounds the cost.
        s.add_component<ProjectileData>(m,
            ProjectileData{nearest_enemy(s, px, py), MISSILE_SPEED, def.amount});
        s.add_component<RenderLayer>(m, RenderLayer{5});
        ParticleEmitter trail;
        trail.shape = EmitterShape::Point;
        trail.additive = true;
        trail.emission_rate = 55.0f;      // 8 x 55/s x 0.3s = ~130 live particles
        trail.particle_lifetime = 0.3f;
        trail.min_speed = 0.0f; trail.max_speed = 20.0f;
        trail.cone_half_angle = 180.0f;
        trail.start_r = 255; trail.start_g = 180; trail.start_b = 70; trail.start_a = 230;
        trail.end_r = 180; trail.end_g = 40; trail.end_b = 20; trail.end_a = 0;
        trail.start_size = 7.0f; trail.end_size = 0.0f;
        trail.offset_x = MISSILE_R; trail.offset_y = MISSILE_R;
        s.add_component<ParticleEmitter>(m, trail);
    }
}

/// Home + fuse every missile in flight. Runs whatever the equipped active is:
/// a missile outlives the frame it was fired on, and may outlive the swap.
void tick_missiles(ComponentStorage& s, EntityManager& em, Blackboard& blackboard, float dt) {
    for (Entity m : s.entities_with_component<ProjectileTag>()) {
        if (s.has_component<DestroyRequest>(m)) continue;
        auto data = s.get_component<ProjectileData>(m);
        if (!data.has_value() || data->get().target == NO_TARGET) continue;   // a plain shot
        float mx, my;
        if (!centre_of(s, m, mx, my)) continue;
        float tx, ty;
        if (!em.is_alive(data->get().target) || !centre_of(s, data->get().target, tx, ty))
            continue;   // target gone: it keeps its heading and its fuse never trips

        auto vel = s.get_component<Velocity>(m);
        if (!vel.has_value()) continue;
        const float cur = std::atan2(vel->get().dy, vel->get().dx);
        const float want = std::atan2(ty - my, tx - mx);
        float d = aim_math::wrap_pi(want - cur);
        d = std::max(-MISSILE_TURN * dt, std::min(MISSILE_TURN * dt, d));
        vel->get() = aim_math::velocity_from_angle(cur + d, data->get().speed);
        if (auto rot = s.get_component<Rotation>(m); rot.has_value())
            rot->get().angle = cur + d;   // the rocket flies nose-first

        if ((tx - mx) * (tx - mx) + (ty - my) * (ty - my) > MISSILE_FUSE * MISSILE_FUSE)
            continue;

        // Detonation: AoE, not the single-target hit ProjectileHitSystem would
        // have given it. Everything inside the blast takes the full amount.
        for (Entity e : s.entities_with_component<EnemyTag>()) {
            float ex, ey;
            if (!centre_of(s, e, ex, ey)) continue;
            if ((ex - mx) * (ex - mx) + (ey - my) * (ey - my) > MISSILE_AOE * MISSILE_AOE)
                continue;
            Entity ev = em.create_entity();
            s.add_component<DamageEvent>(ev, DamageEvent{e, data->get().damage});
        }
        burst(s, em, mx, my, 900.0f, 0.10f, 255, 190, 90, MISSILE_AOE, 14.0f);
        // telemetry: write-only observation, nothing in the sim reads tm.* (the
        // player.hit_bearing precedent) — cannot move the replay canary.
        blackboard.set<double>("tm.bombs", blackboard.get_or<double>("tm.bombs", 0.0) + 1.0);
        s.add_component<DestroyRequest>(m, DestroyRequest{});
    }
}

void clear_beams(ComponentStorage& s) {
    for (Entity b : s.entities_with_component<BeamTag>())
        s.add_component<DestroyRequest>(b, DestroyRequest{});
}

void tick_laser(ComponentStorage& s, EntityManager& em, Blackboard& bb, float dt) {
    float t = bb.get_or<float>(K_BEAM_T, -1.0f);
    if (t < 0.0f) return;
    const float total = BEAM_HOLD + TAU / BEAM_SWEEP_RATE;
    t += dt;
    if (t >= total) {
        bb.set<float>(K_BEAM_T, -1.0f);
        clear_beams(s);
        return;
    }
    bb.set<float>(K_BEAM_T, t);

    float angle = bb.get_or<float>(K_BEAM_A, 0.0f);
    if (t > BEAM_HOLD) angle += BEAM_SWEEP_RATE * dt;
    bb.set<float>(K_BEAM_A, angle);

    float px, py;
    Entity player = 0;
    if (items::ship_of(s, player) == nullptr || !centre_of(s, player, px, py)) {
        bb.set<float>(K_BEAM_T, -1.0f);
        clear_beams(s);
        return;
    }

    // The beams are recycled, not respawned: one entity per cardinal, moved and
    // rotated each frame (the HealthBarSystem/LaserBeamSystem idiom).
    std::vector<Entity> beams = s.entities_with_component<BeamTag>();
    while (static_cast<int>(beams.size()) < BEAM_COUNT) {
        Entity b = em.create_entity();
        s.add_component<BeamTag>(b, BeamTag{});
        s.add_component<Size>(b, Size{BEAM_LENGTH, BEAM_WIDTH});
        s.add_component<Color>(b, Color{160, 230, 255, 220});
        s.add_component<Rotation>(b, Rotation{0.0f, 0.0f, false});
        s.add_component<Position>(b, Position{px, py});
        s.add_component<RenderLayer>(b, RenderLayer{5});
        ParticleEmitter e;
        e.shape = EmitterShape::Line;
        e.line_dx = BEAM_LENGTH; e.line_dy = 0.0f;
        e.additive = true;
        e.emission_rate = 90.0f;      // 4 x 90/s x 0.3s = ~110 live particles
        e.particle_lifetime = 0.3f;
        e.min_speed = 0.0f; e.max_speed = 30.0f;
        e.cone_half_angle = 180.0f;
        e.start_size = 7.0f; e.end_size = 0.0f;
        e.start_r = 160; e.start_g = 230; e.start_b = 255; e.start_a = 220;
        e.end_r = 40; e.end_g = 90; e.end_b = 200; e.end_a = 0;
        s.add_component<ParticleEmitter>(b, e);
        beams.push_back(b);
    }

    const float dps = bb.get_or<float>(K_BEAM_DMG, 30.0f);
    for (int i = 0; i < BEAM_COUNT && i < static_cast<int>(beams.size()); ++i) {
        const float a = angle + TAU * static_cast<float>(i) / static_cast<float>(BEAM_COUNT);
        Entity b = beams[static_cast<size_t>(i)];
        // The quad is rotated about its own centre, so it is placed at the ray's
        // MIDPOINT — anchoring it on the muzzle would draw half the beam behind
        // the drone.
        if (auto p = s.get_component<Position>(b); p.has_value()) {
            p->get().x = px + std::cos(a) * BEAM_LENGTH * 0.5f - BEAM_LENGTH * 0.5f;
            p->get().y = py + std::sin(a) * BEAM_LENGTH * 0.5f - BEAM_WIDTH * 0.5f;
        }
        if (auto r = s.get_component<Rotation>(b); r.has_value())
            r->get().angle = a;   // Rotation::angle is radians
        if (auto e = s.get_component<ParticleEmitter>(b); e.has_value()) {
            e->get().line_dx = std::cos(a) * BEAM_LENGTH;
            e->get().line_dy = std::sin(a) * BEAM_LENGTH;
        }
        for (Entity enemy : s.entities_with_component<EnemyTag>()) {
            float ex, ey;
            if (!centre_of(s, enemy, ex, ey)) continue;
            if (ray_distance(px, py, a, BEAM_LENGTH, ex, ey) > BEAM_WIDTH) continue;
            Entity ev = em.create_entity();
            s.add_component<DamageEvent>(ev, DamageEvent{enemy, dps * dt});
        }
    }
}

void tick_field(ComponentStorage& s, EntityManager& em, Blackboard& bb, float dt) {
    float t = bb.get_or<float>(K_FIELD_T, 0.0f);
    if (t <= 0.0f) return;
    t -= dt;
    bb.set<float>(K_FIELD_T, std::max(0.0f, t));

    Entity player = 0;
    if (items::ship_of(s, player) == nullptr) return;
    float px, py;
    if (!centre_of(s, player, px, py)) return;

    const float radius = bb.get_or<float>(K_FIELD_R, 180.0f);
    const float push = bb.get_or<float>(K_FIELD_P, 420.0f);
    // The sphere is a no-entry zone, so it holds enemies out every frame rather
    // than knocking them back once — same clamped shove as the Repulsor Field.
    items::push_enemies_out(s, px, py, radius, push, dt);

    if (t <= 0.0f) burst(s, em, px, py, 300.0f, 0.10f, 120, 220, 255);
}

}  // namespace

const ActiveItemDef* active_def(const std::vector<ActiveItemDef>& list, int id) {
    for (const ActiveItemDef& d : list)
        if (active_id_for(d.effect) == id) return &d;
    return nullptr;
}

float ray_distance(float ox, float oy, float angle, float length, float px, float py) {
    const float dx = px - ox, dy = py - oy;
    const float t = dx * std::cos(angle) + dy * std::sin(angle);
    if (t < 0.0f || t > length) return 1e9f;
    const float hx = ox + std::cos(angle) * t, hy = oy + std::sin(angle) * t;
    return std::sqrt((px - hx) * (px - hx) + (py - hy) * (py - hy));
}

void tick(ComponentStorage& storage, EntityManager& entity_manager,
          Blackboard& blackboard, const GameConfig& cfg, bool fire_pressed) {
    const float dt = static_cast<float>(blackboard.get_or<double>("delta_time", 0.0));
    if (dt <= 0.0f) return;

    // Upkeep first: a missile, beam or sphere outlives the frame it was fired on
    // and must keep running even if the active is swapped out underneath it.
    tick_missiles(storage, entity_manager, blackboard, dt);
    tick_laser(storage, entity_manager, blackboard, dt);
    tick_field(storage, entity_manager, blackboard, dt);

    Entity player = 0;
    ShipState* ship = items::ship_of(storage, player);
    if (ship == nullptr || ship->active_id < 0) {
        // A fresh run rebuilds ShipState with active_id -1, so this is also where
        // a laser or sphere left live by the previous run is forgotten — the
        // Blackboard outlives spawn_world and the beam entities do not.
        blackboard.set<float>(K_BEAM_T, -1.0f);
        blackboard.set<float>(K_FIELD_T, 0.0f);
        blackboard.set<float>(K_SALVO_T, 0.0f);
        return;
    }
    if (ship->active_cd > 0.0f) ship->active_cd = std::max(0.0f, ship->active_cd - dt);

    const ActiveItemDef* def = active_def(cfg.actives, ship->active_id);
    if (def == nullptr) return;
    // Boss rewards upgrade the held active by shortening its cooldown; the
    // multiplier is a blackboard key rather than a sixth ShipState float.
    const float cd = def->cooldown * blackboard.get_or<float>(K_CD_MULT, 1.0f);

    float px, py;
    if (!centre_of(storage, player, px, py)) return;

    // The pending second wave. Same one-float-on-the-blackboard countdown the
    // beam and the sphere use, fired from wherever the drone has drifted to.
    if (ship->active_id == ids::MISSILES) {
        const float salvo = blackboard.get_or<float>(K_SALVO_T, 0.0f);
        if (salvo > 0.0f) {
            blackboard.set<float>(K_SALVO_T, salvo - dt);
            if (salvo - dt <= 0.0f)
                launch_missiles(storage, entity_manager, px, py, *def,
                                TAU / (2.0f * MISSILE_COUNT));
        }
    }

    if (ship->active_id == ids::REPULSOR_FIELD) {
        auto h = storage.get_component<Health>(player);
        if (!h.has_value()) return;
        if (!field_should_fire(h->get().current, h->get().max_hp, ship->active_cd)) return;
        h->get().current = h->get().max_hp;         // heal to full
        blackboard.set<float>(K_FIELD_T, def->duration);
        blackboard.set<float>(K_FIELD_R, cfg.shop.repulsor_radius * 1.4f);
        blackboard.set<float>(K_FIELD_P, def->amount);
        items::push_enemies_out(storage, px, py, cfg.shop.repulsor_radius * 1.4f,
                                def->amount * 4.0f, dt);   // the initial knock-back
        burst(storage, entity_manager, px, py, 500.0f, 0.12f, 120, 220, 255);
        ship->active_cd = cd;
        blackboard.set<std::string>("hud_message", def->name + " engaged");
        blackboard.set<float>("hud_message_timer", 2.0f);
        return;
    }

    if (!fire_pressed || ship->active_cd > 0.0f) return;
    ship->active_cd = cd;
    blackboard.set<std::string>("hud_message", def->name + " fired");
    blackboard.set<float>("hud_message_timer", 1.5f);

    if (ship->active_id == ids::MISSILES) {
        launch_missiles(storage, entity_manager, px, py, *def);
        blackboard.set<float>(K_SALVO_T, MISSILE_SALVO);   // wave two
    } else if (ship->active_id == ids::LASER) {
        blackboard.set<float>(K_BEAM_T, 0.0f);
        blackboard.set<float>(K_BEAM_A, 0.0f);
        blackboard.set<float>(K_BEAM_DMG, def->amount);
    }
}

}  // namespace actives
