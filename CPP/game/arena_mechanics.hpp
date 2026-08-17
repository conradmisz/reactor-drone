#ifndef ARENA_MECHANICS_HPP
#define ARENA_MECHANICS_HPP

#include <algorithm>
#include <cmath>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "arena_config.hpp"        // ArenaDef
#include "enemy_components.hpp"    // EnemyTag, Health
#include "player_components.hpp"   // PlayerTag, Flash
// Pickup lives on player_components.hpp, already included above.

/**
 * arena_mechanics — the per-arena signature mechanics (roguelite phase 5,
 * design §4).
 *
 * Two themes, one mechanic each, both resolved off the live ArenaDef and both
 * read in exactly one place — the Foundry-mines shape the design doc names:
 *
 *   The Shroud  darkness. Everything past `light_radius` of the drone fades
 *               out, so enemies genuinely emerge from the black.
 *   The Drift   a directional current that pushes the drone, the enemies and
 *               the loot alike.
 *
 * ponytail: free functions in a header, the tick_shields idiom. No state — the
 * arena def is the only input and both effects are recomputed per frame, so an
 * arena shift needs no teardown.
 */
namespace arena_mechanics {

/// How dark a shrouded enemy gets at the far edge of visibility. Not 0: a
/// completely invisible enemy reads as a bug rather than as darkness, and a
/// ghost you can just make out is what makes the theme tense instead of unfair.
constexpr uint8_t SHROUD_MIN_ALPHA = 28;

/// Alpha an entity `d` px from the drone should render at, given the arena's
/// light radius. Full brightness inside the radius, fading to SHROUD_MIN_ALPHA
/// over another radius' worth of distance. Pure, so the curve is a unit test.
inline uint8_t shroud_alpha(float d, float light_radius) {
    if (light_radius <= 0.0f) return 255;
    if (d <= light_radius) return 255;
    const float t = std::min(1.0f, (d - light_radius) / light_radius);
    const float a = 255.0f + t * (static_cast<float>(SHROUD_MIN_ALPHA) - 255.0f);
    return static_cast<uint8_t>(std::lround(a));
}

/// Centre of `e`, false when it has no Position.
inline bool centre_of(ComponentStorage& cs, Entity e, float& x, float& y) {
    auto p = cs.get_component<Position>(e);
    if (!p.has_value()) return false;
    x = p->get().x; y = p->get().y;
    if (auto s = cs.get_component<Size>(e); s.has_value()) {
        x += s->get().width * 0.5f;
        y += s->get().height * 0.5f;
    }
    return true;
}

/**
 * The Shroud: fade every enemy by its distance from the drone.
 *
 * Writes Tint, not Color: Color is the resting tint of record that FlashSystem
 * restores, so writing it here would make a hit flash fade back to a *dimmed*
 * colour and the enemy would get permanently darker every time it was shot.
 * A live Flash is left alone for the same reason tick_enemy_tint leaves it —
 * being lit up by a hit is exactly the feedback darkness must not eat.
 */
inline void tick_shroud(ComponentStorage& cs, float light_radius) {
    if (light_radius <= 0.0f) return;
    float px = 0.0f, py = 0.0f;
    bool have_player = false;
    for (Entity p : cs.entities_with_component<PlayerTag>()) {
        have_player = centre_of(cs, p, px, py);
        break;
    }
    if (!have_player) return;

    for (Entity e : cs.entities_with_component<EnemyTag>()) {
        if (cs.has_component<Flash>(e)) continue;
        float ex = 0.0f, ey = 0.0f;
        if (!centre_of(cs, e, ex, ey)) continue;
        const float d = std::sqrt((ex - px) * (ex - px) + (ey - py) * (ey - py));
        const uint8_t a = shroud_alpha(d, light_radius);
        Color base{255, 255, 255, 255};
        if (auto c = cs.get_component<Color>(e); c.has_value()) base = c->get();
        cs.add_component<Tint>(e, Tint{base.r, base.g, base.b, a, false});
    }
}

/**
 * The Drift: a directional current, in px/s, applied to the drone, every enemy
 * and every loose pickup.
 *
 * Position, not Velocity: enemies have their velocity overwritten by
 * EnemySeekSystem every frame and the drone's by PlayerControlSystem, so a
 * current written into Velocity would simply be erased. Displacing the position
 * is what survives both — and it is why this must run AFTER the movement system
 * and BEFORE the arena clamp, so the wall still gets the last word.
 */
/// D231 item 3: the enemy shove is CLAMPED below the slowest enemy's own
/// speed — a current faster than an enemy would pin it against the wall
/// forever and stall the wave-clear gate. Pure — unit-tested.
inline float drift_enemy_scale(float dx_per_s, float dy_per_s, float cap_speed) {
    const float mag = std::sqrt(dx_per_s * dx_per_s + dy_per_s * dy_per_s);
    if (cap_speed <= 0.0f || mag <= cap_speed) return 1.0f;
    return cap_speed / mag;
}

inline void tick_drift(ComponentStorage& cs, float dx_per_s, float dy_per_s, float dt,
                       float enemy_cap_speed = -1.0f) {
    if (dt <= 0.0f || (dx_per_s == 0.0f && dy_per_s == 0.0f)) return;
    const float dx = dx_per_s * dt, dy = dy_per_s * dt;
    auto shove = [&](Entity e, float k) {
        if (auto p = cs.get_component<Position>(e); p.has_value()) {
            p->get().x += dx * k;
            p->get().y += dy * k;
        }
    };
    // The player and the loot feel the full current (D231: +35%, the drama);
    // enemies ride a capped one so the slowest can always fight upstream.
    const float ek = drift_enemy_scale(dx_per_s, dy_per_s, enemy_cap_speed);
    for (Entity e : cs.entities_with_component<PlayerTag>()) shove(e, 1.0f);
    for (Entity e : cs.entities_with_component<EnemyTag>()) shove(e, ek);
    for (Entity e : cs.entities_with_component<Pickup>()) shove(e, 1.0f);
}

}  // namespace arena_mechanics

#endif  // ARENA_MECHANICS_HPP
