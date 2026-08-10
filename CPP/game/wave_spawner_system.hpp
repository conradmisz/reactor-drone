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
 * unlocked_injections — the enemy_types rows eligible for cadence injection at
 * `wave` (Iteration 3, D67): every row with a positive `first_wave` that the run
 * has reached. Pure, so the moon-unlock schedule unit-tests without a game loop.
 */
inline std::vector<int> unlocked_injections(const std::vector<EnemyType>& types, int wave) {
    std::vector<int> out;
    for (size_t i = 0; i < types.size(); ++i) {
        if (types[i].first_wave > 0 && types[i].first_wave <= wave)
            out.push_back(static_cast<int>(i));
    }
    return out;
}

/**
 * injected_type — the enemy_types row this spawn should use instead of the wave's
 * roster pick, or -1 for "use the roster" (Iteration 3, D67).
 *
 * Two cadences over the 0-based spawn counter, never RNG, so a replay of a seed
 * spawns the same units in the same order. The arena's specialty unit outranks a
 * moon on a spawn that both would claim: the specialty unit is the rarer of the
 * two and is what makes an arena feel like itself.
 */
inline int injected_type(int spawn_index, const SpecialtyConfig& sp,
                         int specialty_unit, const std::vector<int>& moons) {
    const int n = spawn_index + 1;
    if (sp.every_n_spawns > 0 && specialty_unit >= 0 && n % sp.every_n_spawns == 0)
        return specialty_unit;
    if (sp.moon_every_n_spawns > 0 && !moons.empty() && n % sp.moon_every_n_spawns == 0)
        return moons[static_cast<size_t>(n / sp.moon_every_n_spawns - 1) % moons.size()];
    return -1;
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

    /// v2 Phase 5a: the colour newly-spawned enemies are tinted. main.cpp pushes
    /// the active ArenaDef's enemy_tint here on every arena swap. A setter rather
    /// than a config/arena-index lookup because "which arena is live" is main's
    /// state, and captured-at-spawn is what makes enemies alive across a shift
    /// keep their old colour for free.
    void set_enemy_tint(uint8_t r, uint8_t g, uint8_t b) {
        enemy_r_ = r; enemy_g_ = g; enemy_b_ = b;
    }

    /**
     * Iteration 3 (D70): hold the wave open. BossSystem raises this the frame it
     * spawns a boss and drops it once the reward has been taken, which is how
     * "the wave clears when the boss dies" is expressed without the spawner
     * knowing anything about bosses. Spawning is unaffected — only the clear (and
     * therefore the stall force-kill, which would otherwise execute the boss).
     */
    void set_clear_hold(bool held) { clear_hold_ = held; }
    bool clear_hold() const { return clear_hold_; }

    void update(Blackboard& blackboard,
                EntityManager& entity_manager,
                ComponentStorage& component_storage);

    void reset() {
        clear_hold_ = false;
        current_wave_ = 0;
        enemies_spawned_ = 0;
        elapsed_time_ = 0.0f;
        spawn_timer_ = 0.0f;
        stall_timer_ = 0.0f;
        all_waves_complete_ = false;
        wave_just_cleared_ = false;
    }

    int current_wave_index() const { return current_wave_; }
    int enemies_spawned_in_wave() const { return enemies_spawned_; }
    bool all_complete() const { return all_waves_complete_; }
    int total_waves() const;

    /// True for the single update in which a wave finished and the arena went
    /// empty. The shop phase hooks onto this edge.
    bool wave_just_cleared() const { return wave_just_cleared_; }

private:
    void spawn_enemy(const WaveDef& wave, EntityManager& entity_manager,
                     ComponentStorage& component_storage);

    const sidecar_loader::LoadedSprite* resolve_sprite(const std::string& sidecar,
                                                       const std::string& clip);
    std::unordered_map<std::string, sidecar_loader::LoadedSprite> sprite_cache_;

    const GameConfig* cfg_ = nullptr;
    std::mt19937 rng_{1234u};
    uint8_t enemy_r_ = 255, enemy_g_ = 255, enemy_b_ = 255;  // active arena's enemy tint

    int current_wave_ = 0;
    int enemies_spawned_ = 0;
    float elapsed_time_ = 0.0f;
    float spawn_timer_ = 0.0f;
    float stall_timer_ = 0.0f;      // seconds a finished wave has waited on stragglers
    bool all_waves_complete_ = false;
    bool wave_just_cleared_ = false;
    bool clear_hold_ = false;       // BossSystem holds a boss wave open (D70)
};

#endif // WAVE_SPAWNER_SYSTEM_HPP
