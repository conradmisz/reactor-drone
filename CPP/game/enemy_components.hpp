#ifndef ENEMY_COMPONENTS_HPP
#define ENEMY_COMPONENTS_HPP

/**
 * EnemyTag — empty marker component identifying enemy entities.
 * Systems query for EnemyTag to find enemies without inspecting other data.
 */
struct EnemyTag {};

/**
 * PathFollower — an enemy's seek speed plus its A* repath state (v2, Phase 7).
 *
 * speed:          movement speed in pixels per second (the only field used by
 *                 the straight-line seek path).
 * repath_timer:   seconds until the next A* recompute while line-of-sight to the
 *                 player is blocked; counts down by dt. 0 = repath this frame.
 * target_x/_y:    world point the enemy currently steers toward when pathing
 *                 (centre of the next path cell); refreshed on each repath.
 * waypoint_index/progress: unused in v2 (kept for the Class-090 path-follow API).
 */
struct PathFollower {
    int waypoint_index = 1;
    float progress = 0.0f;
    float speed = 64.0f;
    float repath_timer = 0.0f;
    float target_x = 0.0f;
    float target_y = 0.0f;
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
