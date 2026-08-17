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
 * tint_phase:     hue offset in [0,1) for the tie-dye arena's colour cycle (v2,
 *                 Phase 5b). Randomised at spawn from the spawner's seeded RNG so
 *                 the swarm is a spread of hues rather than one marching colour,
 *                 and so replays stay deterministic. Ignored outside tie-dye
 *                 arenas. Lives here rather than on a new component because it is
 *                 per-enemy state and PathFollower is already on every enemy.
 * waypoint_index/progress: unused in v2 (kept for the Class-090 path-follow API).
 */
struct PathFollower {
    int waypoint_index = 1;
    float progress = 0.0f;
    float speed = 64.0f;
    float repath_timer = 0.0f;
    float target_x = 0.0f;
    float target_y = 0.0f;
    float tint_phase = 0.0f;
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
 * EnemyShot — marker for a projectile FIRED BY an enemy (Iteration 3, D51).
 *
 * Deliberately a bare tag: an enemy shot is otherwise the same recipe as a player
 * shot (Position/Velocity/Size/Collider/Lifetime/ParticleEmitter) with a
 * `ContactDamage` and the `layers::ENEMY_SHOT` collider layer. That means the
 * player takes damage from it through the path PlayerDamageSystem already runs —
 * anything carrying ContactDamage hurts the drone — and no second damage system
 * has to exist. The tag is only so EnemyFireSystem can find its own shots to
 * expire them on contact.
 */
struct EnemyShot {
    // v3 Tier 7: the neon ribbon's colour, per enemy spec. Same reasoning as
    // ProjectileTag — a Color component would draw the old square underneath.
    uint8_t r = 255, g = 80, b = 80;
};

/**
 * EnemyBehavior — what makes an enemy something other than a seeker (D51).
 *
 * One struct for every non-default enemy in the iteration-3 plan: the moon
 * shooters, the four per-arena specialty units, and the boss. One fat struct
 * rather than one component per behaviour for the same reason ShipState is one:
 * registering a component type is an edit in three shared files, so the whole
 * behaviour axis pays that cost once, here, and the lanes that follow add only
 * `kind` values.
 *
 * kind:     behaviour_kinds constant (see below). SEEKER is the plain enemy.
 * tier:     escalation step for the same kind — moon_1/2/3, and the harder
 *           second-pass specialty units (waves 26-50).
 * timer:    seconds until this enemy's next action (fire, drop, summon). A plain
 *           float countdown, never an RNG draw, so replays stay deterministic.
 * cooldown: seconds between actions; `timer` is reset to it.
 * aim:      the behaviour's working angle (radians) — facing for the bulwark's
 *           frontal armour, current sweep angle for the boss.
 */
struct EnemyBehavior {
    int kind = 0;
    int tier = 1;
    float timer = 0.0f;
    float cooldown = 2.0f;
    float aim = 0.0f;
    // Engine suite (D148): the authored bullet pattern this emitter runs, as an
    // index into GameConfig::patterns; -1 = none, which is every enemy shipped so
    // far. `cursor` is the op it is on and `phase` the running spiral angle.
    // THREE FIELDS ON AN EXISTING STRUCT, not a new component type: registering
    // one is an edit in three shared files (Invariant 6), and this is exactly the
    // "prefer a field on an existing struct" case the standards call out.
    int pattern = -1;
    int cursor = 0;
    float phase = 0.0f;
};

/**
 * EnemyBehavior::kind values. Code constants, never JSON row indices — the same
 * discipline as item_ids (D26), so re-ordering GameData.json can never silently
 * turn a mine-dropper into a boss. The loader maps a `behavior` string onto one
 * of these.
 *
 * Lane ownership: SHOOTER is Phase 6, SPITTER/MINER/BULWARK/SPLITTER are Phase 7,
 * BOSS is Phase 8. They are all declared here in Phase 0 so no later lane has to
 * edit this shared header.
 */
namespace behavior_kinds {
enum : int {
    SEEKER   = 0,   // default: no behaviour, EnemySeekSystem is the whole enemy
    SHOOTER  = 1,   // moon types: fires EnemyShot projectiles on a timer
    SPITTER  = 2,   // Bio-lab: leaves a short-lived damaging patch behind it
    MINER    = 3,   // Foundry: drops proximity mines
    BULWARK  = 4,   // Core: frontal damage reduction, slow turn rate
    SPLITTER = 5,   // Prism: splits into two smaller units on death (PROPOSED)
    BOSS     = 6,   // every 10th wave; summons adds, themed to the live arena
};
}

/**
 * HealthBarTag — empty marker for the floating health-bar entities.
 *
 * HealthBarSystem spawns two tagged bar entities (background + fill) above each enemy
 * every frame and destroys the previous frame's tagged bars, so the tag is how it finds
 * and recycles them.
 */
struct HealthBarTag {};

// ---------------------------------------------------------------------------
// Gameplay pack v2.3 tier 3 (D221): secondary-fire status effects. Defined here
// (not secondary_fire.hpp) because component_storage.hpp must see them to
// register storage — the ShipState "five files" rule.
// ---------------------------------------------------------------------------

/// Enemy on fire (Flak). Refreshed to LINGER_S on every exposure; deals `dps`
/// in DPS_TICK_S bites so the damage path stays the one DamageEvent road.
struct Burn {
    float time_left = 3.0f;
    float dps = 4.0f;
    float acc = 0.0f;
};

/// Enemy slowed by the blizzard. Restores the exact original speed on expiry.
struct Chill {
    float time_left = 0.0f;
    float orig_speed = 0.0f;
    // D232 (Cryolator): Frostbite rides the same component — stacks each cut
    // 10% speed; four freeze the target for `frozen_t` seconds, then clear.
    int   stacks = 0;
    float stack_cd = 0.0f;     // seconds until the next stack may apply
    float frozen_t = 0.0f;     // >0 = frozen solid
};

/// The traveling blizzard ring itself (an entity with Position/Velocity/
/// Lifetime/Size; this tag holds its slow factor).
struct BlizzardTag {
    float slow_mult = 0.45f;
    float dps = 0.0f;          // D232 (Plasma Wake): >0 = the field also burns
};


#endif // ENEMY_COMPONENTS_HPP
