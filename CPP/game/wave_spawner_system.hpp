#ifndef WAVE_SPAWNER_SYSTEM_HPP
#define WAVE_SPAWNER_SYSTEM_HPP

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/sidecar_loader.hpp"
#include "arena_config.hpp"
#include "obstacles.hpp"   // Vec2
#include <cmath>
#include <string>
#include <unordered_map>
#include <random>

/**
 * ring_spawn_point — where an enemy enters (v2, Upgrade Phase 2).
 *
 * `spawn_radius` from the player on `angle` (just off-screen), then pulled back
 * onto the arena circle if that lands outside it, so spawns near the wall stay
 * in play. Pure, so it unit-tests without a game loop.
 */
inline Vec2 ring_spawn_point(float player_x, float player_y, float angle,
                             float spawn_radius,
                             float center_x, float center_y, float arena_radius) {
    float x = player_x + spawn_radius * std::cos(angle);
    float y = player_y + spawn_radius * std::sin(angle);
    float dx = x - center_x, dy = y - center_y;
    float d = std::sqrt(dx * dx + dy * dy);
    if (d > arena_radius && d > 0.0001f) {
        x = center_x + dx / d * arena_radius;
        y = center_y + dy / d * arena_radius;
    }
    return {x, y};
}

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
