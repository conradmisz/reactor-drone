#include "surge_system.hpp"

#include <algorithm>
#include <cmath>

#include "engine/ecs/components.hpp"
#include "collision_layers.hpp"
#include "enemy_components.hpp"
#include "player_components.hpp"

namespace {

constexpr float TAU = 6.28318530717958647692f;

/// How hard a coolant flood slows what stands in it, as a velocity multiplier at
/// magnitude 1. Applied per frame to velocities, not to the speed stat, so it
/// costs no state and ends the instant the field does.
constexpr float FLOOD_SLOW = 0.45f;

/// Sweep line angular rate (radians/sec) and half-width in world px.
constexpr float SWEEP_RATE = 0.9f;

}  // namespace

SurgeSystem::Effect SurgeSystem::effect_for(const std::string& s) {
    if (s == "slow_field")    return Effect::SlowField;
    if (s == "sweep_line")    return Effect::SweepLine;
    if (s == "eruption")      return Effect::Eruption;
    if (s == "gravity_storm") return Effect::GravityStorm;
    return Effect::Unknown;
}

void SurgeSystem::clear(ComponentStorage& storage, EntityManager& entity_manager) {
    (void)entity_manager;
    for (Live& l : live_)
        if (l.has_carrier) storage.add_component<DestroyRequest>(l.carrier, DestroyRequest{});
    live_.clear();
}

void SurgeSystem::fire(ComponentStorage& storage, EntityManager& entity_manager,
                       const SurgeDef& def, float roll_a, float roll_b) {
    if (static_cast<int>(live_.size()) >= MAX_LIVE) return;
    const Effect eff = effect_for(def.effect);
    if (eff == Effect::Unknown) return;
    if (cfg_ == nullptr) return;

    Live l;
    l.effect = eff;
    l.magnitude = def.magnitude;
    l.radius = def.radius;
    l.remaining = def.duration;
    l.telegraph = def.telegraph;

    // Placement: a point on a random bearing, a random fraction of the way out.
    // sqrt on the radial roll keeps the distribution even over the DISC rather
    // than crowding the centre — the same reason a spawn ring is not uniform in r.
    const float bearing = roll_a * TAU;
    const float dist = std::sqrt(roll_b) * cfg_->arena.radius * 0.72f;
    l.x = cfg_->arena.center_x + std::cos(bearing) * dist;
    l.y = cfg_->arena.center_y + std::sin(bearing) * dist;
    l.angle = bearing;

    // The two damaging effects carry their damage on an ordinary ContactDamage
    // entity, which is how the Phase-6 hazards work — so they need no damage
    // system of their own, and PlayerDamageSystem picks them up unchanged.
    if (eff == Effect::Eruption || eff == Effect::SweepLine) {
        Entity c = entity_manager.create_entity();
        const float size = eff == Effect::Eruption ? l.radius * 2.0f : 64.0f;
        storage.add_component<Position>(c, Position{l.x - size * 0.5f, l.y - size * 0.5f});
        storage.add_component<Size>(c, Size{size, size});
        storage.add_component<Color>(c, Color{255, 120, 40, 190});
        storage.add_component<Collider>(c,
            Collider{size, size, layers::HAZARD, layers::HAZARD_MASK});
        storage.add_component<ContactDamage>(c,
            ContactDamage{12.0f * l.magnitude, 0, 0});
        storage.add_component<Tint>(c, Tint{255, 140, 50, 255, true});
        storage.add_component<RenderLayer>(c, RenderLayer{3});
        l.carrier = c;
        l.has_carrier = true;
    }
    live_.push_back(l);
}

void SurgeSystem::update(ComponentStorage& storage, EntityManager& entity_manager,
                         Blackboard& blackboard, ForceFieldSystem& forces,
                         int arena_index, int wave, bool wave_changed, float dt) {
    if (cfg_ == nullptr) return;

    // --- schedule ---------------------------------------------------------
    // R2: the scheduler takes EXACTLY three draws whenever it ticks a wave, before
    // any conditional. Whether an arena has a surge table, whether a row is
    // eligible, and whether the chance passes all decide which draws are USED —
    // never how many are taken. Retuning or deleting a surge table therefore
    // cannot shift a single later draw in this stream (and the stream is private
    // to this system, so it cannot shift a spawn or a loot roll at all).
    if (wave_changed) {
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        const float roll_chance = unit(rng_);
        const float roll_place = unit(rng_);
        const float roll_extra = unit(rng_);
        draws_ += 3;

        if (arena_index >= 0 && arena_index < static_cast<int>(cfg_->arenas.size())) {
            const ArenaDef& a = cfg_->arenas[static_cast<size_t>(arena_index)];
            // First eligible row wins; the table is authored in priority order.
            for (const SurgeDef& d : a.surges) {
                if (wave < d.first_wave) continue;
                if (d.last_wave > 0 && wave > d.last_wave) continue;
                if (roll_chance >= d.chance) continue;
                fire(storage, entity_manager, d, roll_place, roll_extra);
                break;
            }
        }
    }
    if (live_.empty() || dt <= 0.0f) return;

    // --- tick + apply -----------------------------------------------------
    for (Live& l : live_) {
        if (l.telegraph > 0.0f) {
            // Telegraph: the region exists and glows, but does not bite yet. The
            // carrier's damage is what is gated, not its visibility — a warning
            // the player cannot see is not a warning.
            l.telegraph -= dt;
            if (l.has_carrier) {
                if (auto cd = storage.get_component<ContactDamage>(l.carrier); cd.has_value())
                    cd->get().amount = l.telegraph > 0.0f ? 0.0f : 12.0f * l.magnitude;
            }
            continue;
        }
        l.remaining -= dt;

        switch (l.effect) {
            case Effect::SlowField: {
                // Everything inside the flood is dragged, drone included. Applied
                // to velocity per frame, so it is self-cancelling the moment the
                // body leaves — no state, nothing to restore, nothing to leak.
                const float keep = 1.0f - (1.0f - FLOOD_SLOW) * l.magnitude;
                auto drag = [&](Entity e) {
                    auto p = storage.get_component<Position>(e);
                    auto v = storage.get_component<Velocity>(e);
                    if (!p.has_value() || !v.has_value()) return;
                    const float dx = p->get().x - l.x, dy = p->get().y - l.y;
                    if (dx * dx + dy * dy >= l.radius * l.radius) return;
                    v->get().dx *= keep;
                    v->get().dy *= keep;
                };
                for (Entity e : storage.entities_with_component<EnemyTag>()) drag(e);
                for (Entity e : storage.entities_with_component<PlayerTag>()) drag(e);
                break;
            }
            case Effect::SweepLine: {
                // A rotating arm: the carrier rides the arc, so the "line" is a
                // moving hazard box rather than a half-plane test per body — one
                // moving entity the existing collision path already handles.
                l.angle += SWEEP_RATE * dt;
                if (l.angle > TAU) l.angle -= TAU;
                if (l.has_carrier) {
                    auto p = storage.get_component<Position>(l.carrier);
                    auto sz = storage.get_component<Size>(l.carrier);
                    if (p.has_value() && sz.has_value()) {
                        p->get().x = cfg_->arena.center_x +
                                     std::cos(l.angle) * l.radius - sz->get().width * 0.5f;
                        p->get().y = cfg_->arena.center_y +
                                     std::sin(l.angle) * l.radius - sz->get().height * 0.5f;
                    }
                }
                break;
            }
            case Effect::GravityStorm: {
                // Straight through Lane T's API — the storm owns no physics of its
                // own. Re-registered every frame with a one-frame lifetime, so an
                // event that ends leaves nothing behind to unregister.
                ForceFieldSystem::Source s;
                s.x = l.x; s.y = l.y;
                s.radius = l.radius * 2.2f;
                s.strength = 420.0f * l.magnitude;
                s.lifetime = dt;
                forces.add_source(s);
                break;
            }
            case Effect::Eruption:
            case Effect::Unknown:
            default:
                break;   // the carrier IS the eruption; nothing to tick
        }
    }

    // --- retire -----------------------------------------------------------
    for (Live& l : live_) {
        if (l.remaining > 0.0f || l.telegraph > 0.0f) continue;
        if (l.has_carrier) storage.add_component<DestroyRequest>(l.carrier, DestroyRequest{});
    }
    live_.erase(std::remove_if(live_.begin(), live_.end(),
                               [](const Live& l) {
                                   return l.remaining <= 0.0f && l.telegraph <= 0.0f;
                               }),
                live_.end());

    blackboard.set<int>("surge.live", static_cast<int>(live_.size()));
}
