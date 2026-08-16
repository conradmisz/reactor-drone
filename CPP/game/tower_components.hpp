#ifndef TOWER_COMPONENTS_HPP
#define TOWER_COMPONENTS_HPP

#include "engine/ecs/components.hpp"
#include <string>
#include <vector>

/**
 * NO_TARGET — sentinel for "this tower has no target".
 *
 * Entity id 0 is a VALID, live entity (the EntityManager hands out 0 first and
 * recycles it), so 0 must NOT double as the "no target" marker — doing so makes
 * an enemy that lands on id 0 permanently un-targetable. Use the max id value,
 * which the EntityManager never assigns to a live entity.
 */
inline constexpr Entity NO_TARGET = static_cast<Entity>(-1);  // == UINT32_MAX

/**
 * TowerTag — empty marker component identifying tower entities.
 * Systems query for TowerTag to find towers without inspecting other data.
 */
struct TowerTag {};

/**
 * TowerStats — stores tower combat parameters.
 *
 * range:             attack radius in pixels (Euclidean distance from tower center)
 * fire_rate:         shots per second
 * damage:            damage dealt per projectile
 * cooldown_remaining: seconds until tower can fire again (0 = ready)
 * target_entity:     entity ID of current target (0 = no target)
 * tower_type:        weapon type identifier ("cannon" or "laser")
 */
struct TowerStats {
    float range = 150.0f;
    float fire_rate = 1.0f;
    float damage = 25.0f;
    float cooldown_remaining = 0.0f;
    Entity target_entity = NO_TARGET;
    std::string tower_type = "cannon";
};

/**
 * CannonFireRequest — empty tag signaling CannonSpawnSystem should
 * create a projectile this frame. Set by TowerFireSystem, consumed
 * by CannonSpawnSystem.
 */
struct CannonFireRequest {};

/**
 * LaserFireRequest — empty tag signaling LaserFireSystem should
 * activate the laser this frame. Set by TowerFireSystem, consumed
 * by LaserFireSystem.
 */
struct LaserFireRequest {};

/**
 * LaserFiring — tracks an active laser beam's state.
 * Attached to a tower entity while the beam is visible.
 *
 * target_entity: entity ID of the enemy being hit
 * elapsed:       seconds since the laser activated (starts at 0.0f)
 * duration:      total active time in seconds (default 0.15s)
 */
struct LaserFiring {
    Entity target_entity = 0;
    float elapsed = 0.0f;
    float duration = 0.15f;
};

/**
 * DamageEvent — represents pending damage to be processed.
 * Attached to a dedicated "event entity" (not the target).
 * Multiple DamageEvents can exist simultaneously.
 *
 * target_entity: entity ID of the entity to receive damage
 * amount:        raw damage before armor reduction
 */
struct DamageEvent {
    Entity target_entity = 0;
    float amount = 0.0f;
};

/**
 * ProjectileTag — empty marker component identifying projectile entities.
 */
/**
 * ProjectileTag — marks a shot, and carries the colour its neon ribbon draws
 * in (v3 Tier 7).
 *
 * The colour lives HERE rather than in a Color component because a shot with a
 * Color renders as the solid square it used to be: RenderSystem's walk draws
 * Images, else Color, else nothing. Dropping Color is what makes the ribbon the
 * only visual. Fields on an already-registered tag also avoid the ~30 lines of
 * ComponentStorage boilerplate a brand-new component needs.
 */
struct ProjectileTag {
    uint8_t r = 255, g = 200, b = 120;
};

/**
 * ProjectileData — stores projectile movement and damage parameters.
 *
 * target:  entity ID of the target enemy
 * speed:   movement speed in pixels per second
 * damage:  damage to apply on hit
 * bounces: ricochets left before a wall kills the shot (D98). 0 = the original
 *          behaviour, a shot that stops dead on the first solid surface. A field
 *          here rather than a new component: ProjectileData is already every
 *          projectile's per-shot record and is already registered, so this costs
 *          no component_storage or CMake edit.
 */
struct ProjectileData {
    Entity target = 0;
    float speed = 300.0f;
    float damage = 25.0f;
    int bounces = 0;
    // Gameplay pack (D221): a piercing shot (Moonshot crescent) damages each
    // enemy it passes through exactly once — `hit` is this shot's ledger, the
    // dash_system state.hit idiom — and dies only on walls or lifetime.
    bool pierce = false;
    bool incendiary = false;   // Flak slag: hits apply/refresh a Burn (secondary_fire.hpp)
    std::vector<Entity> hit;

    ProjectileData() = default;
    ProjectileData(Entity t, float s, float d, int b = 0, bool p = false)
        : target(t), speed(s), damage(d), bounces(b), pierce(p) {}
};

/**
 * TowerAnimationClips — the idle and fire animation clips for a tower, resolved
 * from its sprite-sheet sidecar once at placement time.
 *
 * TowerAnimationSystem swaps the tower's Animation between these two clips:
 * the looping idle clip by default, the one-shot fire clip on the frame the
 * tower fires, returning to idle when the one-shot completes. Caching both clips
 * here keeps TowerAnimationSystem free of any sidecar parsing or asset loading
 * (no frame/grid magic numbers in the system — they live in the clips).
 *
 * has_fire is false when the sidecar defines no "fire" clip; such towers keep
 * their idle animation and are skipped by TowerAnimationSystem.
 */
struct TowerAnimationClips {
    Animation idle;            // looping idle clip (the tower's resting state)
    Animation fire;            // one-shot fire clip (played on each shot)
    bool has_fire = false;     // true only if the sidecar defined a "fire" clip
};

/**
 * TowerAiming — per-facing on-screen barrel angles for a directional tower.
 *
 * facing_angles[i] is the on-screen angle (degrees; 0 = screen-right, CCW) at which
 * frame i's barrel points, baked by the generator from the render camera. TowerAimingSystem
 * picks the facing whose angle is closest to the direction from the tower to its target and
 * writes that index to SpriteSheet.current_frame, so the tower visually points at its target.
 * Empty when the tower's sidecar is not directional (then it is not aimed).
 */
struct TowerAiming {
    std::vector<float> facing_angles;
};

/**
 * BeamTag — empty marker for laser-beam entities.
 *
 * LaserBeamSystem spawns one tagged beam entity per active laser each frame (a
 * rotated, textured quad from tower to target) and destroys the previous frame's
 * tagged beams, so the tag is how the system finds and recycles them.
 */
struct BeamTag {};

#endif // TOWER_COMPONENTS_HPP
