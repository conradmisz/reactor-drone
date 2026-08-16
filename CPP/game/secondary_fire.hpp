#ifndef SECONDARY_FIRE_HPP
#define SECONDARY_FIRE_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "enemy_components.hpp"   // Burn / Chill / BlizzardTag

/**
 * Secondary fire (gameplay pack v2.3, D221/D223) — every weapon's right-mouse
 * attack, on one shared cooldown slot per ship. The tick_shields idiom: free
 * functions called from main.cpp, dispatching on the Blackboard weapon keys
 * published by start_run ("weapon.secondary", "weapon.secondary_cd_max") so
 * this file stays catalogue-blind.
 *
 * Behaviors by id:
 *   charge_shot     55 Iron — hold to charge (cap CHARGE_MAX_S), release for a
 *                   big shot; damage AND cooldown scale with the hold.
 *   crescent_burst  Moonshot — 8 piercing crescents radially, instant.
 *   lava_stream     Flak — 3 s forward slag stream; hits set enemies on fire
 *                   (Burn: small damage for 3 s after last exposure).
 *   blizzard        Hailstorm — one traveling snow ring; enemies caught in it
 *                   are chilled (PathFollower.speed scaled) for its duration.
 */

namespace secondary {
constexpr float CHARGE_MAX_S   = 2.5f;   // full charge after this many held seconds
constexpr float CHARGE_MIN_CD  = 2.0f;   // tap floor for the scaled cooldown
constexpr float DPS_TICK_S     = 0.5f;   // burn damage cadence
constexpr float BURN_LINGER_S  = 3.0f;
constexpr float STREAM_S       = 3.0f;   // lava stream duration
constexpr float STREAM_STEP_S  = 0.07f;  // seconds between slag droplets

/// Charge fraction [0,1] from held seconds. Pure — unit-tested.
inline float charge_frac(float held_s) {
    if (held_s <= 0.0f) return 0.0f;
    return held_s >= CHARGE_MAX_S ? 1.0f : held_s / CHARGE_MAX_S;
}
/// Cooldown for a charge shot released at `frac` of full charge (max = cd_max,
/// spec: shorter press -> shorter cooldown). Pure — unit-tested.
inline float charge_cooldown(float frac, float cd_max) {
    const float cd = cd_max * frac;
    return cd < CHARGE_MIN_CD ? CHARGE_MIN_CD : cd;
}
/// Charge-shot damage multiplier: 1x tap -> 4x full. Pure — unit-tested.
inline float charge_damage_mult(float frac) { return 1.0f + 3.0f * frac; }
}  // namespace secondary

/// Per-frame secondary-fire input/cooldown/dispatch. Reads "mouse2.held".
void tick_secondary_fire(ComponentStorage& storage, EntityManager& em,
                         Blackboard& bb, float dt);

/// Burn DoT + chill bookkeeping (apply/expire). Called right after
/// tick_secondary_fire, before shields/damage resolve.
void tick_burns_and_chills(ComponentStorage& storage, EntityManager& em, float dt);

#endif  // SECONDARY_FIRE_HPP
