#ifndef ACTIVE_ITEMS_HPP
#define ACTIVE_ITEMS_HPP

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "arena_config.hpp"
#include <string>

/**
 * actives — the three boss-reward active items (D71/D74).
 *
 * All three sit on ShipState.active_cd, a 30 s cooldown fixed by the design note.
 * `E` fires the two aimed ones; the repulsion device is not a key at all — it
 * auto-triggers below 20 % hull, which is the whole point of it.
 *
 *   missiles       two salvos of 8 radial homing rockets, AoE on detonation
 *   laser          4 cardinal beams, hold ~0.9 s, then a fast 360 deg sweep
 *   repulsor_field heal to full, shove enemies out, hold a 5 s no-entry sphere
 *
 * ponytail: free functions in a header + one .cpp, the item_system.hpp idiom.
 * The only cross-frame state is three floats, and those live on the Blackboard
 * (D28/D41) rather than justifying a class or a fifth component type.
 */
namespace actives {

/// Ids stored in ShipState.active_id. Code constants mapped from the catalogue's
/// `effect` string (D26), never a row index.
namespace ids {
enum : int { MISSILES = 0, LASER = 1, REPULSOR_FIELD = 2 };
}

/// Catalogue `effect` -> ShipState.active_id. -1 = not an active.
inline int active_id_for(const std::string& effect) {
    if (effect == "missiles")       return ids::MISSILES;
    if (effect == "laser")          return ids::LASER;
    if (effect == "repulsor_field") return ids::REPULSOR_FIELD;
    return -1;
}

/// The catalogue row a held active id came from, or nullptr.
const ActiveItemDef* active_def(const std::vector<ActiveItemDef>& actives, int id);

/// Fraction of max hull below which the repulsion device fires itself.
constexpr float FIELD_TRIGGER_FRAC = 0.20f;

/**
 * Should the repulsion device fire? Pure, so the "exactly at the threshold"
 * boundary is a unit test rather than a playtest. `>=` is deliberate on the
 * threshold: at exactly 20 % the device has NOT fired — it is a *below* 20 %
 * effect, so a drone sitting on the line still owns its panic button.
 */
inline bool field_should_fire(float health, float max_hp, float cooldown_left) {
    if (cooldown_left > 0.0f || max_hp <= 0.0f) return false;
    return health > 0.0f && health / max_hp < FIELD_TRIGGER_FRAC;
}

/**
 * Distance from point (px,py) to the ray starting at (ox,oy) along `angle`,
 * counting only the forward half and only out to `length`. Returns a large value
 * for anything behind the muzzle. Pure — this is the laser's whole hit test.
 */
float ray_distance(float ox, float oy, float angle, float length, float px, float py);

/**
 * One frame of active-item work: cooldown, the auto-trigger, the E-key trigger,
 * and the per-frame upkeep of anything already in flight (missile homing, beam
 * sweep, the field sphere). Safe to call with no active equipped.
 */
void tick(ComponentStorage& storage, EntityManager& entity_manager,
          Blackboard& blackboard, const GameConfig& cfg, bool fire_pressed);

}  // namespace actives

#endif  // ACTIVE_ITEMS_HPP
