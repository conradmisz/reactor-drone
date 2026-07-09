#ifndef WAVE_SPAWNER_SYSTEM_HPP
#define WAVE_SPAWNER_SYSTEM_HPP

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/sidecar_loader.hpp"
#include "arena_config.hpp"
#include <string>
#include <unordered_map>
#include <random>

/**
 * WaveSpawnerSystem — spawns enemies around the arena ring in escalating waves.
 *
 * Class-090's spawner, changed to place enemies at a random angle on the arena's
 * spawn ring (instead of a path entrance) and to give them a seek Velocity +
 * PathFollower.speed (steered at the player by EnemySeekSystem) instead of a grid
 * path. Reads its waves/enemy_types from the injected GameConfig. Publishes
 * "wave"/"total_waves" to the Blackboard and sets "all_waves_complete" when the
 * last wave (or the victory wave) has finished spawning.
 */
class WaveSpawnerSystem {
public:
    void set_config(const GameConfig* cfg);

    void update(Blackboard& blackboard,
                EntityManager& entity_manager,
                ComponentStorage& component_storage);

    void reset() {
        current_wave_ = 0;
        enemies_spawned_ = 0;
        elapsed_time_ = 0.0f;
        spawn_timer_ = 0.0f;
        all_waves_complete_ = false;
    }

    int current_wave_index() const { return current_wave_; }
    int enemies_spawned_in_wave() const { return enemies_spawned_; }
    bool all_complete() const { return all_waves_complete_; }
    int total_waves() const;

private:
    const sidecar_loader::LoadedSprite* resolve_sprite(const std::string& sidecar,
                                                       const std::string& clip);
    std::unordered_map<std::string, sidecar_loader::LoadedSprite> sprite_cache_;

    const GameConfig* cfg_ = nullptr;
    std::mt19937 rng_{1234u};

    int current_wave_ = 0;
    int enemies_spawned_ = 0;
    float elapsed_time_ = 0.0f;
    float spawn_timer_ = 0.0f;
    bool all_waves_complete_ = false;
};

#endif // WAVE_SPAWNER_SYSTEM_HPP
