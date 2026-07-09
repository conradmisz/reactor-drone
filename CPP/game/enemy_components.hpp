#ifndef ENEMY_COMPONENTS_HPP
#define ENEMY_COMPONENTS_HPP

/**
 * EnemyTag — empty marker component identifying enemy entities.
 * Systems query for EnemyTag to find enemies without inspecting other data.
 */
struct EnemyTag {};

/**
 * PathFollower — tracks an enemy's progress along the computed path.
 *
 * waypoint_index: index into computed_path of the waypoint the enemy is
 *                 currently moving TOWARD. Starts at 1 (enemy spawns at waypoint 0).
 * progress:       interpolation factor [0.0, 1.0] from waypoint[index-1] to waypoint[index].
 * speed:          movement speed in pixels per second.
 */
struct PathFollower {
    int waypoint_index = 1;
    float progress = 0.0f;
    float speed = 64.0f;
};

/**
 * Health — tracks enemy hit points.
 * current starts equal to max_hp. Towers reduce current via DamageApplySystem.
 * armor_multiplier: fraction of damage that penetrates armor (1.0 = no armor,
 *                   0.5 = 50% damage reduction).
 */
struct Health {
    float current = 100.0f;
    float max_hp = 100.0f;
    float armor_multiplier = 1.0f;
};

/**
 * HealthBarTag — empty marker for the floating health-bar entities.
 *
 * HealthBarSystem spawns two tagged bar entities (background + fill) above each enemy
 * every frame and destroys the previous frame's tagged bars, so the tag is how it finds
 * and recycles them.
 */
struct HealthBarTag {};

#endif // ENEMY_COMPONENTS_HPP
