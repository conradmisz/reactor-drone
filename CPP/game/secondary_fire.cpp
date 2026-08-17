#include "secondary_fire.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "player_components.hpp"   // PlayerTag, ShipState, WeaponStats
#include "enemy_components.hpp"    // EnemyTag, PathFollower
#include "tower_components.hpp"    // ProjectileTag, ProjectileData, DamageEvent
#include "collision_layers.hpp"
#include "aim_math.hpp"
#include "active_items.hpp"   // ids:: (D232 loadout-gated passives)

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
                  float half_size, float lifetime, bool pierce, bool incendiary,
                  bool crescent = false, bool chunk = false, bool wake = false) {
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
        static_cast<uint8_t>(std::min(255, sb + 60)),
        /*bolt=*/false, /*slag=*/chunk, /*crescent=*/crescent});
    ProjectileData pd{NO_TARGET, speed, damage, 0, pierce};
    pd.incendiary = incendiary;
    pd.wake = wake;                 // D232: Plasma Wake drops patches en route
    pd.wake_x = cx; pd.wake_y = cy;
    s.add_component<ProjectileData>(shot, pd);
    s.add_component<RenderLayer>(shot, RenderLayer{5});
    // Playtest #5 item 9 (D231): the charge slug earns a fat spark tracer —
    // the same ember treatment the Flak chunk wears (player_fire_system).
    if (chunk) {
        ParticleEmitter trail;
        trail.shape = EmitterShape::Point;
        trail.additive = true;
        trail.emission_rate = 340.0f;      // D232 items 11+12: longer, louder
        trail.particle_lifetime = 0.6f;
        trail.min_speed = 0.0f; trail.max_speed = 40.0f;
        trail.cone_half_angle = 180.0f;
        trail.start_r = static_cast<uint8_t>(std::min(255, sr + 90));
        trail.start_g = static_cast<uint8_t>(std::min(255, sg + 55));
        trail.start_b = static_cast<uint8_t>(std::min(255, sb + 70));
        trail.start_a = 230;
        trail.end_r = static_cast<uint8_t>(sr / 2);
        trail.end_g = static_cast<uint8_t>(sg / 2);
        trail.end_b = static_cast<uint8_t>(sb / 2);
        trail.end_a = 0;
        trail.start_size = 8.0f; trail.end_size = 0.0f;
        trail.offset_x = half_size; trail.offset_y = half_size;
        s.add_component<ParticleEmitter>(shot, trail);
    }
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

        const bool held = bb.get_or<bool>("mouse2.held", false);
        const bool jammed = bb.get_or<float>("ship.no_fire", 0.0f) > 0.0f;

        // Playtest #4 item 5 (D230, supersedes the D229 3s-burst): the
        // flamethrower runs on FUEL, not a cooldown. Hold to breathe fire
        // while the gauge drains; release and it refills. stream_timer is the
        // fuel tank in seconds (full = STREAM_S, seeded by start_run), and
        // the shared HUD gauge shows it.
        if (id == "lava_stream") {
            const bool breathing = held && !jammed && ship.stream_timer > 0.0f;
            if (breathing) {
                ship.stream_timer = std::max(0.0f, ship.stream_timer - dt);
                ship.stream_acc += dt;
                const bool cryo = ship.active_id == actives::ids::CRYOLATOR;
                const float fdur = bb.get_or<float>("fb.flash_duration", 0.12f);
                const uint8_t fr = static_cast<uint8_t>(bb.get_or<int>("fb.enemy_flash_r", 255));
                const uint8_t fg = static_cast<uint8_t>(bb.get_or<int>("fb.enemy_flash_g", 255));
                const uint8_t fbb = static_cast<uint8_t>(bb.get_or<int>("fb.enemy_flash_b", 255));
                Muzzle m;
                while (ship.stream_acc >= secondary::STREAM_STEP_S) {
                    ship.stream_acc -= secondary::STREAM_STEP_S;
                    if (!player_muzzle(storage, player, m)) break;
                    const float dirx = std::cos(m.angle), diry = std::sin(m.angle);
                    const float cos_half = std::cos(secondary::STREAM_HALF_ANGLE);
                    for (Entity e : storage.entities_with_component<EnemyTag>()) {
                        auto ep = storage.get_component<Position>(e);
                        auto ez = storage.get_component<Size>(e);
                        if (!ep.has_value() || !ez.has_value()) continue;
                        const float er = ez->get().width * 0.5f;
                        const float dx = ep->get().x + er - m.x;
                        const float dy = ep->get().y + ez->get().height * 0.5f - m.y;
                        const float dist = std::sqrt(dx * dx + dy * dy);
                        if (dist > secondary::STREAM_RANGE + er) continue;
                        const float align = dist > 1.0f ? (dx * dirx + dy * diry) / dist : 1.0f;
                        // Playtest #8 item 1 (D234): point-blank is ALWAYS in
                        // the fire — an enemy you are standing on can sit just
                        // behind the cone axis and the angle test alone read
                        // that as "not in the flame". Inside arm's reach the
                        // cone check is waived.
                        constexpr float POINT_BLANK = 70.0f;
                        if (dist > POINT_BLANK + er && align < cos_half) continue;
                        // Playtest #6 item 15 (D232): the flame is hottest on
                        // its axis — 1.5x dead centre falling to 0.7x at the
                        // cone edge.
                        const float heat = std::max(0.7f, 0.7f + 0.8f *
                            (align - cos_half) / (1.0f - cos_half));
                        Entity ev = em.create_entity();
                        storage.add_component<DamageEvent>(ev,
                            DamageEvent{e, secondary::STREAM_TICK_DMG * heat});
                        // Playtest #4 item 5: the damage was landing but
                        // INVISIBLE — no hit flash. Now every tick flashes.
                        storage.add_component<Flash>(e, Flash{fdur, fdur, fr, fg, fbb});
                        if (cryo) {
                            // D232 (Cryolator): ice, not fire — Frostbite
                            // stacks on a 0.5 s cadence; 4 = frozen solid.
                            auto pf = storage.get_component<PathFollower>(e);
                            if (pf.has_value()) {
                                auto ch = storage.get_component<Chill>(e);
                                if (!ch.has_value()) {
                                    Chill c{1.5f, pf->get().speed};
                                    c.stacks = 1; c.stack_cd = 0.25f;   // D236: freeze twice as fast
                                    pf->get().speed = c.orig_speed * 0.9f;
                                    storage.add_component<Chill>(e, c);
                                } else {
                                    Chill& c = ch->get();
                                    c.time_left = 1.5f;
                                    if (c.stack_cd <= 0.0f && c.stacks < 4 && c.frozen_t <= 0.0f) {
                                        c.stacks += 1;
                                        c.stack_cd = 0.25f;   // D236: freeze twice as fast
                                        if (c.stacks >= 4) {
                                            c.frozen_t = bb.get_or<float>("active.freeze_s", 2.0f);
                                            pf->get().speed = 0.0f;
                                        } else {
                                            pf->get().speed =
                                                c.orig_speed * (1.0f - 0.10f * c.stacks);
                                        }
                                    }
                                }
                            }
                        } else if (auto burn = storage.get_component<Burn>(e); burn.has_value())
                            burn->get().time_left = secondary::BURN_LINGER_S;
                        else
                            storage.add_component<Burn>(e, Burn{});
                    }
// Playtest #7 item 1 (D233): the flame emitter RIDES the player
                    // (maintained after this loop) — the old stationary puff
                    // hosts kept radiating from where the drone USED to be.
                    // Only the short spark garnish still spawns per step.
                    Entity sparks = em.create_entity();
                    storage.add_component<Position>(sparks, Position{m.x, m.y});
                    storage.add_component<Lifetime>(sparks, Lifetime{0.07f});
                    ParticleEmitter sp;
                    sp.shape = EmitterShape::Point;
                    sp.additive = true;
                    sp.emission_rate = 260.0f;
                    sp.particle_lifetime = 0.4f;
                    sp.min_speed = 420.0f; sp.max_speed = 700.0f;
                    sp.direction = m.angle * 57.29578f;
                    sp.cone_half_angle = 7.0f;
                    sp.start_r = 255; sp.start_g = 250; sp.start_b = cryo ? 255 : 220; sp.start_a = 255;
                    if (cryo) { sp.end_r = 140; sp.end_g = 200; sp.end_b = 255; sp.end_a = 0; }
                    else      { sp.end_r = 255; sp.end_g = 170; sp.end_b = 60;  sp.end_a = 0; }
                    sp.start_size = 3.0f; sp.end_size = 0.0f;
                    storage.add_component<ParticleEmitter>(sparks, sp);
                }
                // The main flame: ONE emitter on the player entity, so its
                // origin can never trail the hull. Re-pointed every frame.
                Muzzle fm;
                if (player_muzzle(storage, player, fm)) {
                    ParticleEmitter fire;
                    fire.shape = EmitterShape::Point;
                    fire.additive = true;
                    fire.emission_rate = 420.0f;
                    fire.particle_lifetime = 0.62f;   // reach ~= speed * lifetime (D236 range up)
                    fire.min_speed = 420.0f; fire.max_speed = 880.0f;
                    fire.direction = fm.angle * 57.29578f;
                    fire.cone_half_angle = 14.0f;
                    if (cryo) {
                        fire.start_r = 200; fire.start_g = 240; fire.start_b = 255; fire.start_a = 235;
                        fire.end_r = 60;    fire.end_g = 120;   fire.end_b = 220;  fire.end_a = 0;
                    } else {
                        fire.start_r = 255; fire.start_g = 210; fire.start_b = 90; fire.start_a = 235;
                        fire.end_r = 200;   fire.end_g = 40;    fire.end_b = 10;   fire.end_a = 0;
                    }
                    fire.start_size = 9.0f; fire.end_size = 2.0f;
                    if (auto z = storage.get_component<Size>(player); z.has_value()) {
                        fire.offset_x = z->get().width * 0.5f;
                        fire.offset_y = z->get().height * 0.5f;
                    }
                    if (auto pe = storage.get_component<ParticleEmitter>(player); pe.has_value()) {
                        fire.emit_accumulator = pe->get().emit_accumulator;   // keep the carry
                        pe->get() = fire;
                    } else
                        storage.add_component<ParticleEmitter>(player, fire);
                }
            } else {
                if (storage.has_component<ParticleEmitter>(player))
                    storage.remove_component<ParticleEmitter>(player);   // flame off
                ship.stream_acc = 0.0f;
                // Playtest #9 (D235): the tank refills ONLY while the trigger
                // is RELEASED — holding an empty tank used to sputter-refire
                // every few frames as drips of fuel trickled back in.
                if (!held)
                    ship.stream_timer = std::min(secondary::STREAM_S,
                        ship.stream_timer + dt * secondary::STREAM_S / secondary::FUEL_RECHARGE_S);
            }
            bb.set<float>("ship.secondary_frac",
                          ship.stream_timer / secondary::STREAM_S);
            return;   // one player
        }

        if (id == "charge_shot") {
            // D231 item 8: the bank refills whenever it is not being spent.
            if (!held || jammed)
                ship.charge_bank = std::min(secondary::CHARGE_MAX_S,
                    ship.charge_bank + dt * secondary::CHARGE_MAX_S / secondary::CHARGE_REFILL_S);
            if (held && !jammed && ship.charge_bank > 0.0f) {
                if (ship.secondary_charge < 0.0f) {
                    ship.secondary_charge = 0.0f;
                    // Playtest #4 item 1 (D230): energy visibly GATHERS into
                    // the hull while charging — short-lived motes ringing the
                    // drone (the freed player emitter slot; removed on release).
                    // Playtest #6 item 8 (D232): bigger, and in the WEAPON's
                    // projectile colour (ship.shot_*), not a fixed gold.
                    const uint8_t gr = static_cast<uint8_t>(bb.get_or<int>("ship.shot_r", 255));
                    const uint8_t gg = static_cast<uint8_t>(bb.get_or<int>("ship.shot_g", 236));
                    const uint8_t gb = static_cast<uint8_t>(bb.get_or<int>("ship.shot_b", 160));
                    ParticleEmitter gather;
                    gather.shape = EmitterShape::Circle;
                    gather.radius = 44.0f;
                    gather.additive = true;
                    gather.emission_rate = 170.0f;
                    gather.particle_lifetime = 0.28f;
                    gather.min_speed = 0.0f; gather.max_speed = 16.0f;
                    gather.start_r = gr; gather.start_g = gg; gather.start_b = gb; gather.start_a = 50;
                    gather.end_r = static_cast<uint8_t>(std::min(255, gr + 60));
                    gather.end_g = static_cast<uint8_t>(std::min(255, gg + 60));
                    gather.end_b = static_cast<uint8_t>(std::min(255, gb + 60));
                    gather.end_a = 245;
                    gather.start_size = 1.5f; gather.end_size = 7.0f;
                    if (auto z = storage.get_component<Size>(player); z.has_value()) {
                        gather.offset_x = z->get().width * 0.5f;
                        gather.offset_y = z->get().height * 0.5f;
                    }
                    storage.add_component<ParticleEmitter>(player, gather);
                }
                const float take = std::min(dt, ship.charge_bank);
                ship.charge_bank -= take;
                ship.secondary_charge += take;
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
                storage.remove_component<ParticleEmitter>(player);   // the gather motes
                Muzzle m;
                if (frac > 0.0f && player_muzzle(storage, player, m)) {
                    // Playtest #1 item 7: a full charge must read as PHYSICALLY
                    // bigger, not a slightly fatter dot. Half-size 10 -> 38 px
                    // (a 76px slug at full hold) and it pierces when charged
                    // past half, so the payoff is visible as well as numeric.
                    // Playtest #3 item 6 (D229): the slug renders as the fat
                    // hot-cored chunk (the slag path), scaled by the charge —
                    // a beefed-up Flak primary, not a standard small shot.
                    // Playtest #4 item 2 (D230): wider, and ALWAYS piercing —
                    // the slug is a sweep that damages everything it crosses
                    // (the pierce ledger keeps it to once per enemy), so a
                    // charged release visibly connects instead of dying on the
                    // first body it grazes.
                    // Playtest #6 items 10+11 (D232): the slug rides the
                    // Twin Barrel fan (ship.extra_shots, the D184-style key)
                    // and flies 936 px/s (+30% on D231's 720).
                    const int slugs = 1 + std::max(0, bb.get_or<int>("ship.extra_shots", 0));
                    constexpr float FAN = 0.09f;
                    for (int b2 = 0; b2 < slugs; ++b2) {
                        const float a2 = m.angle
                            + (static_cast<float>(b2) - (slugs - 1) * 0.5f) * FAN;
                        spawn_shot(storage, em, bb, m.x, m.y, a2,
                                   936.0f, wpn.damage * secondary::charge_damage_mult(frac),
                                   16.0f + 34.0f * frac, 1.4f, true, false,
                                   /*crescent=*/false, /*chunk=*/true,
                                   ship.active_id == actives::ids::PLASMA_WAKE);
                    }
                    // D231 item 8: no cooldown — the bank is the gate.
                }
            }
            if (!bb.get_or<bool>("ship.secondary_charging", false))
                bb.set<float>("ship.secondary_frac",
                              ship.charge_bank / secondary::CHARGE_MAX_S);
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
            // Playtest #4 item 6 (D230): burst crescents run 1.6x the primary's
            // width — overlap between neighbours is fine, the ring should wall.
            for (int i = 0; i < N; ++i)
                spawn_shot(storage, em, bb, m.x, m.y, m.angle + TWO_PI * i / N,
                           wpn.projectile_speed, wpn.damage,
                           wpn.projectile_size * 1.6f, 0.6f, true, false,
                           /*crescent=*/true, /*chunk=*/false,
                           ship.active_id == actives::ids::PLASMA_WAKE);
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
    // D232 (Plasma Wake): wake shots drop a plasma patch every 50 px of
    // travel. The patch is a STATIONARY "blizzard" with dps — the existing
    // ring loop below does the slowing and now the burning too.
    for (Entity shot : storage.entities_with_component<ProjectileTag>()) {
        auto pdata = storage.get_component<ProjectileData>(shot);
        if (!pdata.has_value() || !pdata->get().wake) continue;
        auto sp = storage.get_component<Position>(shot);
        auto sz = storage.get_component<Size>(shot);
        if (!sp.has_value() || !sz.has_value()) continue;
        ProjectileData& pd = pdata->get();
        const float cx = sp->get().x + sz->get().width * 0.5f;
        const float cy = sp->get().y + sz->get().height * 0.5f;
        const float dx = cx - pd.wake_x, dy = cy - pd.wake_y;
        if (dx * dx + dy * dy < 50.0f * 50.0f) continue;
        pd.wake_x = cx; pd.wake_y = cy;
        auto tag = storage.get_component<ProjectileTag>(shot);
        Entity patch = em.create_entity();
        constexpr float PR2 = 34.0f;
        storage.add_component<Position>(patch, Position{cx - PR2, cy - PR2});
        storage.add_component<Size>(patch, Size{PR2 * 2.0f, PR2 * 2.0f});
        storage.add_component<Lifetime>(patch, Lifetime{2.5f});
        BlizzardTag bt;
        bt.slow_mult = 0.75f;   // the spec's 25% slow
        bt.dps = 12.0f;         // guesstimate per owner; tune in playtest
        storage.add_component<BlizzardTag>(patch, bt);
        ParticleEmitter glow;
        glow.shape = EmitterShape::Circle;
        glow.radius = PR2;
        glow.additive = true;
        glow.emission_rate = 60.0f;
        glow.particle_lifetime = 0.5f;
        glow.min_speed = 0.0f; glow.max_speed = 14.0f;
        const uint8_t wr = tag.has_value() ? tag->get().r : 180;
        const uint8_t wg = tag.has_value() ? tag->get().g : 255;
        const uint8_t wb = tag.has_value() ? tag->get().b : 200;
        glow.start_r = wr; glow.start_g = wg; glow.start_b = wb; glow.start_a = 220;
        glow.end_r = static_cast<uint8_t>(wr / 3);
        glow.end_g = static_cast<uint8_t>(wg / 3);
        glow.end_b = static_cast<uint8_t>(wb / 3);
        glow.end_a = 0;
        glow.start_size = 5.0f; glow.end_size = 0.0f;
        glow.offset_x = PR2; glow.offset_y = PR2;
        storage.add_component<ParticleEmitter>(patch, glow);
    }

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
            // D232 (Plasma Wake): a field with dps also burns what stands in it.
            const float dps = storage.get_component<BlizzardTag>(ring)->get().dps;
            if (dps > 0.0f && dt > 0.0f) {
                Entity ev = em.create_entity();
                storage.add_component<DamageEvent>(ev, DamageEvent{e, dps * dt});
            }
        }
    }

    // D235 (playtest #9): frostbitten enemies wear the cold — a faint icy
    // additive tint per stack, and a hard white-blue casing tint when frozen
    // (the encasing shell itself is drawn by the render pass). FlashSystem
    // may steal the Tint for a hit flash; it comes back next frame.
    for (Entity e : storage.entities_with_component<Chill>()) {
        auto ch = storage.get_component<Chill>(e);
        const Chill& c = ch->get();
        if (c.frozen_t > 0.0f) {
            storage.add_component<Tint>(e, Tint{225, 245, 255, 235, true});
        } else if (c.stacks > 0) {
            storage.add_component<Tint>(e, Tint{140, 200, 255,
                static_cast<uint8_t>(40 + 30 * c.stacks), true});
        }
    }

    // Chill expiry restores the exact original speed. D232: Frostbite rides
    // the same component — the stack cadence ticks down here, and a frozen
    // target thaws (stacks cleared, speed restored) when frozen_t runs out.
    for (Entity e : storage.entities_with_component<Chill>()) {
        auto ch = storage.get_component<Chill>(e);
        Chill& c = ch->get();
        if (c.stack_cd > 0.0f) c.stack_cd -= dt;
        if (c.frozen_t > 0.0f) {
            c.frozen_t -= dt;
            if (c.frozen_t <= 0.0f) {
                c.stacks = 0;
                if (auto pf = storage.get_component<PathFollower>(e); pf.has_value())
                    pf->get().speed = c.orig_speed;
                storage.remove_component<Chill>(e);
                if (storage.has_component<Tint>(e)) storage.remove_component<Tint>(e);
            }
            continue;   // frozen: the ordinary expiry clock is paused
        }
        c.time_left -= dt;
        if (c.time_left > 0.0f) continue;
        if (auto pf = storage.get_component<PathFollower>(e); pf.has_value())
            pf->get().speed = c.orig_speed;
        storage.remove_component<Chill>(e);
        if (c.stacks > 0 && storage.has_component<Tint>(e))
            storage.remove_component<Tint>(e);
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
