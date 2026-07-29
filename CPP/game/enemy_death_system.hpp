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

    /// Spawn this kill's loot at (cx, cy). Always consumes exactly 2 + 2*max_drops
    /// RNG draws regardless of outcome — see the determinism note above.
    void drop_loot(ComponentStorage& component_storage,
                   EntityManager& entity_manager,
                   float cx, float cy, int currency_value);

    // Explosion sprite, loaded once on first death.
    std::optional<sidecar_loader::LoadedSprite> effect_;
    bool effect_failed_ = false;
    const sidecar_loader::LoadedSprite* effect_sprite();
};

#endif // ENEMY_DEATH_SYSTEM_HPP
