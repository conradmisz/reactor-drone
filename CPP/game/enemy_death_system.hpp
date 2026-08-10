#ifndef ENEMY_DEATH_SYSTEM_HPP
#define ENEMY_DEATH_SYSTEM_HPP

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/sidecar_loader.hpp"
#include "arena_config.hpp"       // EconomyConfig
#include <optional>
#include <random>

/**
 * EnemyDeathSystem — turns dead enemies (Health <= 0) into score, loot, and a
 * short death animation, then marks them for destruction.
 *
 * Per kill: adds the enemy's ContactDamage.score to Blackboard "score", drops
 * 1-3 currency Pickup entities (plus, rarely, a shop key) at the corpse, spawns
 * a one-shot explosion sprite with a matching Lifetime, and attaches a
 * DestroyRequest. Double-reward is prevented by skipping enemies already marked
 * for destruction.
 *
 * Determinism (R2): the drop rolls are drawn *unconditionally*, once per kill,
 * before anything branches on them — the same discipline as the spawn-angle draw
 * in WaveSpawnerSystem. A draw that happens only on one code path desynchronises
 * every later roll and breaks `--seed` replay.
 */
/**
 * loot_place — keep a dropped coin out of the things that would make picking it
 * up a punishment (Lane K, D101).
 *
 * The user's note: money must not share space with obstacles, hazards, enemy
 * mines or other pickups. A rejection loop over fresh RNG draws is the obvious
 * build and the wrong one here — it would draw a variable number of times per
 * kill and desynchronise the replay stream (the R2 discipline `drop_loot`
 * spells out at length). So the search draws **nothing**: the scattered point
 * comes from the RNG exactly as before, and if it lands on something the coin is
 * nudged along a fixed golden-angle spiral of candidates until one is free. The
 * whole search is a pure function of the point, so it cannot move an RNG draw
 * (D101 — the same reasoning as Lane B's spiral, D56).
 */
namespace loot_place {

/// Candidate offsets tried after the drawn point. Fixed, and a coin that finds
/// no free spot keeps its drawn position — worse placement is better than a
/// loop that could run long.
constexpr int SEARCH_STEPS = 16;

/// True if an axis-aligned box of half-extent `half` centred on (cx, cy)
/// overlaps an obstacle, a hazard patch, a deployed mine or an existing pickup.
bool blocked(ComponentStorage& storage, float cx, float cy, float half);

/// Nudge (x, y) to the nearest free spot on the spiral, within `reach` pixels.
/// No-op when the point is already free. Draws no random numbers, ever.
void nudge_free(ComponentStorage& storage, float& x, float& y, float half, float reach);

}  // namespace loot_place

class EnemyDeathSystem {
public:
    /// Drop tuning + the RNG seed. Call once at startup; re-seeds the drop RNG.
    void set_economy(const EconomyConfig& economy, unsigned int seed) {
        economy_ = economy;
        rng_.seed(seed);
    }

    void update(ComponentStorage& component_storage,
                EntityManager& entity_manager,
                Blackboard& blackboard);

private:
    EconomyConfig economy_;
    std::mt19937 rng_{1234u};

    /// Spawn this kill's loot at (cx, cy). `drop_chance` is P(this kill drops
    /// anything at all) — the dead enemy type's EnemyType::drop_chance, carried in
    /// on its ContactDamage. Always consumes exactly 3 + 2*max_drops RNG draws
    /// regardless of outcome — see the determinism note above.
    void drop_loot(ComponentStorage& component_storage,
                   EntityManager& entity_manager,
                   float cx, float cy, int currency_value, float drop_chance);

    // Explosion sprite, loaded once on first death.
    std::optional<sidecar_loader::LoadedSprite> effect_;
    bool effect_failed_ = false;
    const sidecar_loader::LoadedSprite* effect_sprite();
};

#endif // ENEMY_DEATH_SYSTEM_HPP
