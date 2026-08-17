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
 *   charge_shot     55 Iron — a passive-refill charge BANK (D231): holding
 *                   RMB pumps the bank into the shot, release fires it, the
 *                   unspent remainder stays banked. Damage scales with charge.
 *   crescent_burst  Moonshot — 8 piercing crescents radially, instant.
 *   lava_stream     Flak — hold-to-breathe flamethrower on a fuel tank
 *                   (D230): hold drains STREAM_S seconds of fire, release
 *                   refills; every tick damages + ignites the cone.
 *   blizzard        Hailstorm — one traveling snow ring; enemies caught in it
 *                   are chilled (PathFollower.speed scaled) for its duration.
 */

namespace secondary {
constexpr float CHARGE_MAX_S   = 2.5f;   // full charge after this many held seconds
// Playtest #5 item 8 (D231): the charge is a BANK, like the flamethrower's
// fuel — it refills passively, holding RMB spends it into the shot, and
// whatever you don't spend stays banked. No cooldown; the bank is the gate.
constexpr float CHARGE_REFILL_S = 8.0f;  // empty bank -> full while not holding
constexpr float DPS_TICK_S     = 0.5f;   // burn damage cadence
constexpr float BURN_LINGER_S  = 3.0f;
constexpr float STREAM_S       = 3.0f;   // fuel tank: seconds of continuous fire
constexpr float STREAM_STEP_S  = 0.07f;  // seconds between damage ticks
constexpr float FUEL_RECHARGE_S = 6.0f;  // empty -> full while not firing (D230)
// Playtest #3 item 3 (D229): the flame cone IS the weapon — every tick hits the
// whole cone. ponytail: feel numbers, tune in playtest.
constexpr float STREAM_RANGE      = 425.0f;  // px reach of the cone (D236 +25% again)
constexpr float STREAM_HALF_ANGLE = 0.28f;   // rad (~16 deg)
constexpr float STREAM_TICK_DMG   = 4.6f;    // per enemy per tick (D231 +15%)

/// Charge fraction [0,1] from held seconds. Pure — unit-tested.
inline float charge_frac(float held_s) {
    if (held_s <= 0.0f) return 0.0f;
    return held_s >= CHARGE_MAX_S ? 1.0f : held_s / CHARGE_MAX_S;
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
