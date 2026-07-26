#ifndef ARENA_CONFIG_HPP
#define ARENA_CONFIG_HPP

#include <cstdint>
#include <string>
#include <vector>

/**
 * Typed, data-driven configuration for the "Reactor Drone" arena, parsed from
 * GameData.json (game-side; the engine loader ignores these blocks). Everything
 * tunable lives here so the game can be re-balanced without recompiling.
 */

/**
 * BackdropLayer — one tiled parallax layer (v2, Phase 5). `scroll_factor` in
 * [0,1] is how attached the layer is to the camera: 1 = glued (farthest, never
 * moves), 0 = fully detached (nearest, moves 1:1). Draw offset per axis is
 * camera * (1 - scroll_factor); see parallax.hpp. Layers draw in list order
 * (first = backmost), so the first layer should be opaque.
 */
struct BackdropLayer {
    std::string image;             // texture path relative to assets/
    float scroll_factor = 1.0f;    // 1 = far/static, 0 = near/full-motion
};

struct ArenaConfig {
    float radius = 320.0f;         // arena radius in world units (play area)
    float spawn_radius = 340.0f;   // ring radius enemies spawn on
    float center_x = 480.0f;       // arena center (world)
    float center_y = 330.0f;
    std::string backdrop;          // optional single backdrop image (legacy; unused)
    std::vector<BackdropLayer> backdrop_layers;  // tiled parallax layers, back-to-front
};

/**
 * ObstacleDef / HazardDef — static arena props (v2, Phase 6). Both are
 * bottom-left-origin AABBs in world coordinates. Obstacles are solid (block the
 * drone and stop shots); hazards carry ContactDamage and hurt the drone on
 * contact but let it pass. `damage` is the health removed per i-frame window.
 */
struct ObstacleDef { float x = 0.0f, y = 0.0f, w = 40.0f, h = 40.0f; };
struct HazardDef   { float x = 0.0f, y = 0.0f, w = 40.0f, h = 40.0f; float damage = 10.0f; };

/**
 * ArenaDef — one themed arena (v2, Phase 6): its parallax backdrop plus obstacle
 * and hazard layouts. `first_wave` is the (1-based) wave at which this arena
 * becomes active; the active arena is the last one whose first_wave <= the
 * current wave (see active_arena_index). Arena geometry (radius/centre/spawn
 * ring) stays shared on ArenaConfig — only the theme + props swap.
 */
struct ArenaDef {
    std::string name;
    int first_wave = 1;
    std::string wall_image;        // boundary-ring segment sprite (relative to assets/images/)
    std::vector<BackdropLayer> backdrop_layers;
    std::vector<ObstacleDef> obstacles;
    std::vector<HazardDef> hazards;
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

/**
 * FeedbackConfig — screen-shake and hit-flash tuning (v2, Phase 4). All balance
 * knobs; the math lives in feedback.hpp. max_shake_px and trauma_decay_per_sec
 * drive the camera shake; the trauma_* values are how much trauma each event
 * adds; flash_duration and the two colours drive the per-entity hit flashes.
 */
struct FeedbackConfig {
    float max_shake_px = 18.0f;         // camera offset at full trauma
    float trauma_decay_per_sec = 1.6f;  // linear trauma bleed-off
    float trauma_player_hit = 0.6f;     // trauma added when the player is hit
    float trauma_enemy_death = 0.25f;   // trauma added when an enemy dies
    float flash_duration = 0.12f;       // hit-flash lifetime (seconds)
    uint8_t player_flash_r = 255, player_flash_g = 70,  player_flash_b = 70;   // red
    uint8_t enemy_flash_r  = 255, enemy_flash_g  = 255, enemy_flash_b  = 255;  // white
};

/**
 * PathfindingConfig — enemy A* tuning (v2, Phase 7). `cell_size` is the grid
 * resolution the obstacle layout is rasterised at; `clearance` grows each
 * obstacle so paths keep a body's width away from walls; `repath_interval` is how
 * often a blocked enemy recomputes its route (seconds). All balance knobs.
 */
struct PathfindingConfig {
    float repath_interval = 0.35f;
    int   cell_size = 40;
    float clearance = 24.0f;
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
    std::vector<ArenaDef> arenas;  // v2 Phase 6: themed arenas swapped by wave
    FeedbackConfig feedback;
    PathfindingConfig pathfinding;  // v2 Phase 7: enemy A* tuning
    int victory_wave = 0;          // 0 = survive all waves; N = win after clearing wave N
    float xp_level2 = 5.0f;        // XP needed for level 2
    float xp_multiplier = 1.5f;    // threshold growth per level
    unsigned int seed = 1234u;     // RNG seed for spread/spawn/upgrades
};

/**
 * Index of the active arena for a given 1-based `wave`: the last arena whose
 * first_wave <= wave. Returns -1 when `arenas` is empty; clamps to 0 when the
 * wave precedes every arena's activation. Pure — unit/property-tested.
 */
inline int active_arena_index(const std::vector<ArenaDef>& arenas, int wave) {
    if (arenas.empty()) return -1;
    int idx = 0;
    for (int i = 0; i < static_cast<int>(arenas.size()); ++i) {
        if (arenas[static_cast<size_t>(i)].first_wave <= wave) idx = i;
    }
    return idx;
}

/// Parse GameData.json into a GameConfig. Missing fields fall back to defaults;
/// throws std::runtime_error only on an unopenable file or malformed JSON.
GameConfig load_arena_config(const std::string& file_path);

#endif // ARENA_CONFIG_HPP
