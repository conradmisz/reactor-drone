#ifndef BULLET_PATTERN_HPP
#define BULLET_PATTERN_HPP

#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "arena_config.hpp"

/**
 * bullet_pattern — the danmaku engine (#2, Lane Y, D148).
 *
 * Bosses and elite enemies fire authored bullet-hell choreography — rings,
 * spirals, aimed fans, timed waits — and a new pattern is a `GameData.json` row,
 * not C++. A tiny interpreter over a fixed op list, ticked from the `pattern`
 * hook, spawning through the *existing* `enemy_fire::spawn_shot` path so a
 * pattern shot is exactly an enemy shot: same collider layer, same
 * ContactDamage, same "no damage system of its own" (D51).
 *
 * DETERMINISM. The interpreter contains **no RNG at all** — variation is
 * authored, not rolled — so it cannot perturb the sim RNG stream under any
 * conditional. Its whole state is a cursor and two floats per emitter, kept on
 * the existing `EnemyBehavior` component (`timer` / `cooldown` / `aim`), so it
 * needs no new component type (Invariant 6).
 *
 * MCU headroom: the interpreter is a switch over a fixed op array; shot counts
 * are bounded by data, and `MAX_SHOTS_PER_OP` bounds them again in code so a
 * mistyped `count` cannot cost a frame.
 */
namespace bullet_pattern {

/// Hard ceiling on one op's burst. The real constraint on a danmaku pattern here
/// is the collision and particle budget (each shot carries a 40/s trail, ~10 live
/// particles, against a 4000 global cap), so an op that asks for 500 shots is a
/// typo, not a design.
inline constexpr int MAX_SHOTS_PER_OP = 64;

/// Op names, resolved from the `type` string — never a row index (D26).
enum class OpKind { Ring, Fan, Spiral, Aimed, Wait, Unknown };

inline OpKind op_kind_for(const std::string& s) {
    if (s == "ring")   return OpKind::Ring;
    if (s == "fan")    return OpKind::Fan;
    if (s == "spiral") return OpKind::Spiral;
    if (s == "aimed")  return OpKind::Aimed;
    if (s == "wait")   return OpKind::Wait;
    return OpKind::Unknown;
}

/// Index of the pattern named `name`, or -1. Resolved once per emitter rather
/// than per frame; a name (not an index) is what an EnemyType stores, so a
/// re-ordered `patterns` array cannot silently swap one boss's attack for
/// another's.
int pattern_index(const std::vector<BulletPatternDef>& patterns, const std::string& name);

/**
 * The angles one op emits this step, given the emitter's running spiral phase and
 * the angle to the player. Pure and engine-free, so the whole choreography is
 * unit-testable without a world: the system is a thin shell that calls this and
 * spawns a shot per angle.
 *
 * `out` is cleared first and never exceeds MAX_SHOTS_PER_OP.
 */
void op_angles(const BulletPatternOp& op, float phase, float aim_angle,
               std::vector<float>& out);

/**
 * Advance every pattern emitter one frame and fire what it asks for.
 *
 * An enemy runs a pattern when its EnemyType names one. The cursor and timers
 * live on the enemy's own EnemyBehavior, so two enemies of the same type run the
 * same pattern independently and a dead one takes its state with it.
 */
void tick(ComponentStorage& storage, EntityManager& entity_manager,
          Blackboard& blackboard, const GameConfig& cfg, float dt);

}  // namespace bullet_pattern

#endif  // BULLET_PATTERN_HPP
