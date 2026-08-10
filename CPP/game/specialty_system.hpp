#ifndef SPECIALTY_SYSTEM_HPP
#define SPECIALTY_SYSTEM_HPP

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "arena_config.hpp"
#include "aim_math.hpp"
#include <cmath>

/**
 * SpecialtySystem — the four per-arena specialty units (#9, D68).
 *
 * One system rather than four, because all four are the same shape: an
 * EnemyBehavior countdown that does something on expiry. The kinds:
 *
 *   SPITTER  (Bio-lab)  leaves a short-lived poison patch behind it
 *   MINER    (Foundry)  drops proximity mines behind it
 *   BULWARK  (Core)     frontal damage reduction, slow turn rate
 *   SPLITTER (Prism)    splits into two smaller units on death
 *
 * SPLITTER lives in EnemyDeathSystem instead — its whole behaviour is a death
 * event, and that is the system that already owns the death event's RNG order.
 *
 * A deployed mine is itself an EnemyBehavior{MINER, tier 0} entity with no
 * EnemyTag: tier 0 is "the mine", tiers >= 1 are "the thing that drops mines".
 * That reuses the scaffolded component instead of registering a MineTag, which
 * costs edits in three shared files (code-standards, ECS).
 *
 * Lane N (D122): a mine is destroyable — a player shot that overlaps an armed
 * mine sets it off where it stands and is consumed. Shooting one from range is
 * therefore how you clear it safely. Done with a direct overlap test here rather
 * than by giving the mine a Collider + Health + EnemyTag: EnemyTag is what the
 * wave-clear check, the minimap, the arena clamp and the loot drop all key off,
 * so a mine wearing it would stall a wave and drop coins.
 */

namespace specialty {

/// Tuning per kind. Constants rather than a config block: every one of them is a
/// derived feel number the arena's own enemy row already gates (fire_interval,
/// shot_damage), and a knob nobody would turn is speculative generality.
constexpr float PATCH_LIFETIME   = 3.0f;   // poison patch seconds
constexpr float PATCH_SIZE       = 96.0f;
constexpr float MINE_SIZE        = 44.0f;
constexpr float MINE_ARM_DELAY   = 0.8f;   // seconds before a fresh mine can trigger
constexpr float MINE_LIFETIME    = 14.0f;  // a mine nobody walks into eventually rots
constexpr float MINE_TRIGGER     = 90.0f;  // proximity radius (px)
// Lane N (D121): 150 -> 100 (a box, so 75px -> 50px of reach from the mine).
// At 75 the blast caught a drone that had already turned and left; at 50 it is
// about the trigger's own bite, so walking into a mine still hurts and backing
// off the moment it lights up is now a real escape.
constexpr float MINE_BLAST_SIZE  = 100.0f;
constexpr float MINE_BLAST_TIME  = 0.35f;
constexpr float BULWARK_ARC      = 1.0472f;   // 60 degrees half-arc, i.e. a 120 deg shield
constexpr float BULWARK_ARMOR    = 0.35f;     // damage multiplier inside the arc
constexpr float BULWARK_TURN     = 0.9f;      // radians/sec — slow enough to flank

/**
 * Is `attacker_angle` inside the bulwark's frontal arc? Pure, so the flanking
 * rule unit-tests without a game loop. Both angles are radians, measured the
 * atan2 way; `half_arc` is half the shield's width.
 */
inline bool inside_arc(float facing, float attacker_angle, float half_arc) {
    return std::fabs(aim_math::wrap_pi(attacker_angle - facing)) <= half_arc;
}

}  // namespace specialty

class SpecialtySystem {
public:
    void set_config(const GameConfig* cfg) { cfg_ = cfg; }

    void update(ComponentStorage& storage, EntityManager& entity_manager,
                Blackboard& blackboard);

private:
    const GameConfig* cfg_ = nullptr;
};

#endif  // SPECIALTY_SYSTEM_HPP
