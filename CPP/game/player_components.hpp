#ifndef PLAYER_COMPONENTS_HPP
#define PLAYER_COMPONENTS_HPP

#include "engine/ecs/components.hpp"

/**
 * "Reactor Drone" game-specific components (Class-110 final project).
 *
 * PlayerTag, Experience, and ContactDamage are the three genuinely new
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
 * Experience — the player's XP/level progression.
 *
 * xp accumulates from kills; when it reaches threshold the level increments and
 * threshold is raised by the xp_curve multiplier (see ExperienceSystem).
 */
struct Experience {
    float xp = 0.0f;         // XP accumulated toward the next level
    int level = 1;           // Current level (starts at 1)
    float threshold = 5.0f;  // XP required to reach the next level
    float multiplier = 1.5f; // threshold *= multiplier on each level-up
};

/**
 * ContactDamage — an enemy's combat payload.
 *
 * amount: health removed from the player on contact.
 * score:  points awarded to the player when this enemy dies.
 * xp:     experience granted to the player when this enemy dies.
 *
 * (The design's "ContactDamage" plus the per-enemy score/xp values from the
 * enemy_types data live together here as the enemy's combat numbers.)
 */
struct ContactDamage {
    float amount = 10.0f;
    int score = 10;
    int xp = 1;
};

/**
 * WeaponStats — the player's gun, tuned live by UpgradeSystem.
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

#endif // PLAYER_COMPONENTS_HPP
