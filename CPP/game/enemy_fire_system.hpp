#ifndef ENEMY_FIRE_SYSTEM_HPP
#define ENEMY_FIRE_SYSTEM_HPP

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "arena_config.hpp"
#include "enemy_components.hpp"
#include <cmath>
#include <string>

/**
 * EnemyFireSystem — enemy projectiles and the three moon shooters (#3, D66).
 *
 * A shot is the PlayerFireSystem recipe with a different collider layer:
 * Position + Velocity + Size + Color + Collider(ENEMY_SHOT) + ContactDamage +
 * Lifetime + EnemyShot + a trail emitter. It therefore needs **no damage system**
 * — PlayerDamageSystem already hurts the drone for anything carrying
 * ContactDamage in its CollidedWith, which is how the Phase-6 hazards work.
 *
 * Firing is a per-entity float countdown on EnemyBehavior::timer, never an RNG
 * draw, so replay determinism is free (D66).
 */

namespace enemy_fire {

/// Map a GameData `behavior` string onto a behavior_kinds constant. Effect
/// strings, never row indices (D26) — a re-ordered enemy_types list can never
/// silently turn a spitter into a boss. Unknown/empty = SEEKER.
inline int behavior_kind_for(const std::string& s) {
    if (s == "shooter")  return behavior_kinds::SHOOTER;
    if (s == "spitter")  return behavior_kinds::SPITTER;
    if (s == "miner")    return behavior_kinds::MINER;
    if (s == "bulwark")  return behavior_kinds::BULWARK;
    if (s == "splitter") return behavior_kinds::SPLITTER;
    if (s == "boss")     return behavior_kinds::BOSS;
    return behavior_kinds::SEEKER;
}

/**
 * turn_toward — rotate `current` toward `target` by at most `max_delta` radians,
 * taking the short way round. Pure, so the tier-2 tracking clamp unit-tests
 * without a game loop. `max_delta <= 0` snaps to the target (no clamp).
 */
inline float turn_toward(float current, float target, float max_delta) {
    constexpr float TAU = 6.28318530717958647692f;
    float d = target - current;
    while (d >  TAU * 0.5f) d -= TAU;
    while (d < -TAU * 0.5f) d += TAU;
    if (max_delta > 0.0f) {
        if (d >  max_delta) d =  max_delta;
        if (d < -max_delta) d = -max_delta;
    }
    return current + d;
}

/// Per-tier tuning for the moon shooters. Tier 3 (laser) pierces — it is the one
/// shot that is not destroyed by the thing it hits.
struct ShotSpec {
    float speed_mult = 1.0f;
    float lifetime = 2.4f;
    float radius = 7.0f;
    float turn_rate = 0.0f;   // radians/sec of tracking; 0 = fire-and-forget
    bool  pierce = false;
    uint8_t r = 255, g = 120, b = 200;
};

inline ShotSpec shot_spec(int tier) {
    switch (tier) {
        case 2:  return ShotSpec{1.6f, 2.6f, 7.0f, 2.2f, false, 255, 170, 90};
        case 3:  return ShotSpec{3.2f, 1.6f, 5.0f, 0.0f, true,  180, 120, 255};
        default: return ShotSpec{1.0f, 2.4f, 8.0f, 0.0f, false, 255, 100, 190};
    }
}

/**
 * moon_muzzle_frac — how far ahead of its own centre a moon shooter's mouth is,
 * as a fraction of the entity's size (#3, D109).
 *
 * The crescent (`_crescent` in assets/generator/v2/make_sprites.py) is a lit
 * disc of radius r minus a bite of radius br offset dx toward the facing
 * direction, in a 128px frame. On the axis the lit body ends at dx-br, which is
 * BEHIND the centre — so a shot spawned at the centre leaves through the back of
 * the moon, which is the bug. The horn tips sit at the circle-circle
 * intersection hx = (r^2 - br^2 + dx^2) / (2 dx): the middle of the mouth's
 * aperture, and therefore the muzzle. Numbers mirror make_sprites' per-tier
 * (r, dx, br) table; the formula is kept rather than the three results so the
 * derivation is visible when that art changes.
 */
inline float moon_muzzle_frac(int tier) {
    constexpr float FRAME = 128.0f;             // make_sprites.py S
    float r = 32.0f, dx = 26.0f, br = 30.0f;    // tier 1
    if (tier == 2)     { r = 37.0f; dx = 28.0f; br = 33.0f; }
    else if (tier >= 3) { r = 42.0f; dx = 27.0f; br = 36.0f; }
    return ((r * r - br * br + dx * dx) / (2.0f * dx)) / FRAME;
}

/**
 * Build one enemy projectile at (cx, cy) travelling along `angle`. Shared with
 * BossSystem, which fires the same recipe for the boss's borrowed attack.
 * `tier` rides on the shot's own EnemyBehavior so the expire step knows whether
 * it pierces without needing a second component.
 */
Entity spawn_shot(ComponentStorage& storage, EntityManager& entity_manager,
                  float cx, float cy, float angle, float speed, float damage,
                  int tier);

/**
 * The enemy_types row that drives a given behaviour kind/tier, or nullptr.
 *
 * A spawned enemy does not record which row it came from, and adding a field for
 * it would touch every enemy in the game to serve four of them. An exact
 * kind+tier match wins; otherwise the first row of that kind, which is what makes
 * a second-pass (tier-2) specialty unit read its own tier-1 row's numbers.
 */
const EnemyType* type_for(const GameConfig* cfg, int kind, int tier);

/// Centre of the single player drone. False when there is no player.
bool player_centre(ComponentStorage& storage, float& px, float& py);

}  // namespace enemy_fire

class EnemyFireSystem {
public:
    void set_config(const GameConfig* cfg) { cfg_ = cfg; }

    /// Ticks every SHOOTER's countdown, fires the ones that are ready, then
    /// expires any non-piercing shot that reported a collision this frame.
    void update(ComponentStorage& storage, EntityManager& entity_manager,
                Blackboard& blackboard);

private:
    const GameConfig* cfg_ = nullptr;
};

#endif  // ENEMY_FIRE_SYSTEM_HPP
