#ifndef ARENA_CONFIG_HPP
#define ARENA_CONFIG_HPP

#include <string>
#include <vector>

/**
 * Typed, data-driven configuration for the "Reactor Drone" arena, parsed from
 * GameData.json (game-side; the engine loader ignores these blocks). Everything
 * tunable lives here so the game can be re-balanced without recompiling.
 */

struct ArenaConfig {
    float radius = 320.0f;         // arena radius in world units (play area)
    float spawn_radius = 340.0f;   // ring radius enemies spawn on
    float center_x = 480.0f;       // arena center (world)
    float center_y = 330.0f;
    std::string backdrop;          // optional backdrop image filename
};

struct WeaponConfig {
    float fire_rate = 4.0f;
    float damage = 20.0f;
    float projectile_speed = 500.0f;
    float projectile_lifetime = 1.2f;
    float spread = 0.0f;
};

struct PlayerConfig {
    float start_health = 100.0f;
    float move_speed = 260.0f;
    float invuln_window = 0.8f;
    float start_x = 460.0f;
    float start_y = 310.0f;
    float size = 40.0f;
    std::string sidecar;           // sprite sidecar (relative to assets/)
    std::string idle_clip = "idle";
    WeaponConfig weapon;
};

struct EnemyType {
    std::string name = "drone";
    std::string sidecar;           // sprite sidecar (relative to assets/)
    std::string clip = "march";
    float speed = 60.0f;
    float health = 30.0f;
    float contact_damage = 10.0f;
    float size = 40.0f;
    int score = 10;
    int xp = 1;
};

struct WaveDef {
    int count = 5;                 // enemies in this wave
    float spawn_interval = 0.8f;   // seconds between spawns
    float delay = 1.0f;            // seconds before the wave starts
    std::vector<int> types;        // indices into enemy_types (empty = all)
};

struct Upgrade {
    std::string stat;              // fire_rate | damage | projectile_speed | max_health | spread
    float amount = 0.0f;
    float weight = 1.0f;
    std::string label;             // HUD text, e.g. "Rapid Fire"
};

struct GameConfig {
    ArenaConfig arena;
    PlayerConfig player;
    std::vector<EnemyType> enemy_types;
    std::vector<WaveDef> waves;
    std::vector<Upgrade> upgrades;
    int victory_wave = 0;          // 0 = survive all waves; N = win after clearing wave N
    float xp_level2 = 5.0f;        // XP needed for level 2
    float xp_multiplier = 1.5f;    // threshold growth per level
    unsigned int seed = 1234u;     // RNG seed for spread/spawn/upgrades
};

/// Parse GameData.json into a GameConfig. Missing fields fall back to defaults;
/// throws std::runtime_error only on an unopenable file or malformed JSON.
GameConfig load_arena_config(const std::string& file_path);

#endif // ARENA_CONFIG_HPP
