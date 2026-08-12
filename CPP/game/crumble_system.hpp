#ifndef CRUMBLE_SYSTEM_HPP
#define CRUMBLE_SYSTEM_HPP

#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "arena_config.hpp"

/**
 * crumble_system — geometry that dies (#9, Lane U, D146).
 *
 * An obstacle with `hp > 0` in its arena row takes projectile damage, darkens as
 * it cracks, and on destruction becomes debris, an open sight-line, and a hole in
 * the pathfinding grid. The cover you kite around is no longer permanent, and the
 * wall your Ricochet Coils counted on can stop existing mid-fight.
 *
 * WHERE THE DAMAGE COMES FROM. `ProjectileHitSystem` already resolves a shot
 * against a solid; the only change there is that a solid carrying `Health` gets a
 * `DamageEvent` before the shot is spent, so obstacle damage rides the exact same
 * `DamageEvent` -> `DamageApplySystem` path enemies use. This system never looks
 * at projectiles — it looks at obstacles whose hull has run out.
 *
 * Determinism: sim-side, and no RNG at all. Debris is emitted through the
 * existing particle emitters (which draw from the engine-seeded particle stream),
 * and the crack shading is a pure function of the remaining HP fraction, so the
 * R2 draw discipline has nothing to observe here.
 *
 * Inert default: every obstacle ships `hp: 0`, which means "indestructible" and
 * skips Health entirely — an arena that authors no HP behaves exactly as before.
 */
class CrumbleSystem {
public:
    /**
     * Called on every arena apply/shift: `defs` is the incoming arena's obstacle
     * rows, in the same order `spawn_arena_props` created the entities.
     * Rebuilds the live-obstacle list from scratch, so a shift back to a
     * previously-wrecked arena arrives whole (each arena re-forms between visits
     * — a run does not get to strip the map permanently).
     */
    void set_arena(const std::vector<ObstacleDef>& defs);

    /**
     * Resolve destroyed obstacles: debris, collider removal, and a rebuilt
     * obstacle list. Returns true when the list changed, i.e. when the caller
     * must re-rasterise the A* grid — the rebuild itself stays in main.cpp
     * beside the other `enemy_seek.set_arena` call, so there is exactly one place
     * that knows how the grid is built.
     */
    bool update(ComponentStorage& component_storage, EntityManager& entity_manager,
                Blackboard& blackboard);

    /// The obstacles that are still standing. Stable address; contents change on
    /// a destruction, which is why the A* rebuild reads it through this getter.
    const std::vector<ObstacleDef>& live_obstacles() const { return live_; }

    int destroyed_this_run() const { return destroyed_; }
    void reset_counter() { destroyed_ = 0; }

private:
    std::vector<ObstacleDef> live_;
    int destroyed_ = 0;
};

#endif  // CRUMBLE_SYSTEM_HPP
