#ifndef PLAYER_COMPONENTS_HPP
#define PLAYER_COMPONENTS_HPP

#include "engine/ecs/components.hpp"

/**
 * "Reactor Drone" game-specific components (Class-110 final project).
 *
 * PlayerTag, ShipState, and ContactDamage are the three genuinely new
 * components from the design doc. WeaponStats is the design's "modified
 * TowerStats" — the same fire-rate/damage idea, attached to the moving player
 * and extended with the projectile parameters an aimed shooter needs.
 *
 * The enemy seek-speed reuses the engine's existing PathFollower.speed (the
 * design's "SeekPlayer is PathFollower stripped down"), so no new component is
 * needed for enemy movement.
 */

/// PlayerTag — marks the single player-controlled drone entity.
struct PlayerTag {};

/**
 * ShipState — all per-run shop/economy state, on the player entity.
 *
 * Gameplay Phase 2 replaced the XP/level component that used to live in this
 * storage slot (D1: one economy, not two). Deliberately one fat struct rather
 * than eight small components: registering a component type costs edits in five
 * files (component_storage.hpp/.cpp, destruction.cpp), so the whole shop economy
 * pays that cost once. Phases 3-4 fill the currently-unused fields; they are
 * declared now so no further engine edit is needed.
 */
struct ShipState {
    int currency = 0;
    int keys = 0;                 // rare drop: opens the shop on demand (D2)
    float shield = 0.0f, shield_max = 0.0f, shield_regen = 0.0f, shield_delay = 0.0f;
    float speed_mult = 1.0f;
    int item_id = -1;             // equipped passive item (Phase 4); -1 = none
    int consumable_id = -1;       // held consumable (Phase 4); -1 = none
    int buff_id = -1;             // active timed buff (Phase 4); -1 = none
    float buff_timer = 0.0f;
    int upg_counts[8] = {0};      // purchases per shop upgrade (escalating price)

    // --- Iteration 3 (D51). Declared in the scaffolding phase so the dash, the
    // gear-upgrade and the boss-active lanes never have to edit this shared
    // header (and re-register the component) in parallel. All inert until the
    // lane that owns them lands. ---
    float dash_cd = 0.0f;         // seconds until the thruster dash is ready
    float dash_timer = 0.0f;      // seconds of dash remaining (>0 = dashing)
    int   active_id = -1;         // boss-reward active item; -1 = none
    float active_cd = 0.0f;       // seconds until the active can fire again
    int   gear_levels[8] = {0};   // upgrade level per owned gear row (#11)
};

/**
 * Equipment ids stored in ShipState (Gameplay Phase 4, D6/D7).
 *
 * These are *code* constants, not JSON row indices: ShopSystem maps a catalogue
 * entry's `effect` string onto one of them (D26), so the catalogue can be
 * re-ordered without silently changing what a saved id means. PickupSystem's
 * ITEM_MAGNET_CORE aliases the first of them, which is what fixes it at 0.
 *
 * buff_id reuses the *consumable* ids — the only timed buff is Overdrive, so a
 * second parallel enum would have exactly one entry.
 */
namespace item_ids {
enum : int { MAGNET_CORE = 0, REPULSOR_FIELD = 1, REACTIVE_PLATING = 2, SALVAGER = 3 };
}
namespace consumable_ids {
enum : int { REPAIR_KIT = 0, OVERDRIVE = 1, EMP_BURST = 2, PHASE_SHIFT = 3 };
}

/// Pickup.kind values. Currency is the common drop; Key is the rare one (D2).
/// Health and Shield (Iteration 3, D51) are not drops at all — SustainSpawnSystem
/// places them around the arena on a timer — but they ride the same component and
/// the same collection path, so they are kinds rather than a second pickup type.
enum class PickupKind : int { Currency = 0, Key = 1, Health = 2, Shield = 3 };

/**
 * Pickup — a collectible dropped by a dead enemy (D5).
 *
 * kind selects which ShipState counter it credits; value is how much. Collection
 * and magnet steering are PickupSystem's job. magnet_speed is the pull speed the
 * Magnet Core item (Phase 4) uses; it is carried per-pickup so drops can differ.
 */
struct Pickup {
    int kind = static_cast<int>(PickupKind::Currency);
    int value = 1;
    float magnet_speed = 400.0f;
};

/**
 * ContactDamage — an enemy's combat payload.
 *
 * amount:   health removed from the player on contact.
 * score:    points awarded to the player when this enemy dies.
 * currency: value of each currency pickup this enemy drops (Phase 2; this field
 *           used to be `xp`).
 */
struct ContactDamage {
    float amount = 10.0f;
    int score = 10;
    int currency = 1;
    // v2 Phase 5: this enemy type's EnemyType::drop_chance, carried to the kill
    // site so EnemyDeathSystem does not have to re-derive which type died. 1.0 on
    // hazards, which carry a ContactDamage but never die, so it is inert there.
    float drop_chance = 1.0f;
};

/**
 * WeaponStats — the player's gun, tuned live by the shop (Phase 3).
 *
 * fire_rate:          shots per second (PlayerFireSystem gates on 1/fire_rate).
 * damage:             damage per projectile.
 * projectile_speed:   projectile travel speed in pixels/second.
 * projectile_lifetime:seconds before a projectile self-destructs (Lifetime).
 * spread:             random aim jitter in radians (0 = perfectly accurate).
 * cooldown_remaining: seconds until the weapon can fire again (0 = ready).
 */
struct WeaponStats {
    float fire_rate = 4.0f;
    float damage = 20.0f;
    float projectile_speed = 500.0f;
    float projectile_lifetime = 1.2f;
    float spread = 0.0f;
    float cooldown_remaining = 0.0f;
};

/**
 * Flash component (v2 hit feedback)
 *
 * A brief coloured flare on an entity. The FlashSystem decrements time_left each
 * frame and writes a Tint (additive glow toward {r,g,b}, fading with
 * time_left/duration) onto the entity; when time_left hits zero it removes both
 * the Flash and the Tint. Used for player-hit (red) and enemy-damaged (white).
 */
struct Flash {
    float time_left = 0.0f;   // Seconds remaining
    float duration = 0.1f;    // Total duration (for intensity = time_left/duration)
    uint8_t r = 255, g = 255, b = 255;
};

#endif // PLAYER_COMPONENTS_HPP
