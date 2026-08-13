#ifndef ARENA_CONFIG_HPP
#define ARENA_CONFIG_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "engine/ecs/systems/bloom_system.hpp"  // BloomConfig (v3 Tier 1)
#include "engine/ecs/systems/postfx_system.hpp" // PostFxConfig (v3 Tier 4)

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
    // v2 Phase 5a: enemy sprites are pure luminance, so their colour is this
    // per-arena tint, applied at spawn (WaveSpawnerSystem) and never recomputed —
    // which is what lets enemies alive across an arena shift keep the old colour.
    // Defaults to white, i.e. the untinted sprite.
    uint8_t enemy_r = 255, enemy_g = 255, enemy_b = 255;
    // v2 Phase 5b: this arena's enemies hue-cycle instead of holding enemy_*
    // (tick_enemy_tint in flash_system.hpp). The backdrop deliberately does not.
    bool tie_dye = false;
    std::string wall_image;        // boundary-ring segment sprite (relative to assets/images/)
    std::string obstacle_image;    // obstacle sprite (empty = flat Color rect)
    std::string hazard_image;      // hazard sprite (empty = flat Color rect)
    std::vector<BackdropLayer> backdrop_layers;
    std::vector<ObstacleDef> obstacles;
    std::vector<HazardDef> hazards;
    // Iteration 3 (D51): the specialty enemy this theme fields (#9), as an index
    // into enemy_types; -1 = none. `specialty_tier` is what makes the SECOND pass
    // over the same four themes (waves 26-50) harder than the first.
    int specialty_unit = -1;
    int specialty_tier = 1;
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

/**
 * ShipDef — one selectable player ship (Lane F, D82).
 *
 * A ship is exactly a variant of the three PlayerConfig fields that describe
 * "what you fly": its sprite, its idle clip and its weapon. No new component,
 * no ship system — `apply_ship` overlays it onto PlayerConfig at run start, the
 * same place and the same discipline as apply_difficulty.
 *
 * `unlock_score` is compared against the *lifetime* score in saves/meta.json
 * (see meta_save.hpp); 0 = always available.
 */
struct ShipDef {
    std::string name = "Standard";
    std::string sidecar;
    std::string idle_clip = "idle";
    WeaponConfig weapon;
    int unlock_score = 0;
    // D184: the hull's identity hue, matching the baked sprite art. Shots are
    // fired in its complement (255-c per channel) so ordnance can never read
    // as ship — the rule D108's hardcoded red approximated for the one ship
    // that existed then. Default is the Standard drone's cyan.
    uint8_t color_r = 90, color_g = 220, color_b = 255;
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
    // --- Iteration 3 (D51): what this type DOES beyond seeking. `behavior` is an
    // EnemyBehavior::kind name ("shooter", "spitter", "miner", "bulwark",
    // "splitter", "boss"); empty = a plain seeker, i.e. every type shipped so far.
    // Parsed in Phase 0 and inert until the lane that owns the kind lands. ---
    std::string behavior;          // behavior_kinds name; empty = SEEKER
    int   behavior_tier = 1;       // escalation step within the kind (moon_1/2/3)
    float fire_interval = 0.0f;    // seconds between actions; 0 = the kind's default
    float shot_speed = 260.0f;     // projectile speed for shooting kinds
    float shot_damage = 8.0f;      // projectile contact damage for shooting kinds
    // Iteration 3 (D67): the wave at which this type starts being *injected* into
    // the stream on the specialty cadence, independent of any wave's `types` list.
    // 0 = never injected (the default, i.e. every type shipped before iteration 3
    // and the four specialty units, which are chosen by their arena instead).
    // This is how moon_1/2/3 arrive at waves 3/15/30 without editing 50 wave rows.
    int first_wave = 0;
    int currency = 1;   // value of each currency pickup this type drops
    // v2 Phase 5: P(a kill of this type drops anything at all). Sparks pay
    // rarely, hulks pay reliably, so target prioritisation has a reason to exist
    // and credits stop falling off every single body. 1.0 = always (the default,
    // i.e. pre-Phase-5 behaviour for any type that omits it).
    float drop_chance = 1.0f;
};

struct WaveDef {
    int count = 5;                 // enemies in this wave (fixed-count mode)
    float spawn_interval = 0.8f;   // seconds between spawns
    float delay = 1.0f;            // seconds before the wave starts
    std::vector<int> types;        // indices into enemy_types (empty = all)
    // v2 Gameplay Phase 1: >0 turns the wave *timed* — spawn for `duration`
    // seconds after the delay, ignoring `count`. hp/speed multipliers scale the
    // shared enemy_types instead of needing new ones.
    float duration = 0.0f;
    float hp_mult = 1.0f;
    float speed_mult = 1.0f;
    // Iteration 3 (D51): this wave is a boss wave (every 10th). Parsed in Phase 0,
    // authored by Lane A with the 50-wave table, and consumed by BossSystem in
    // Phase 8 — so the wave table is written once, not twice.
    bool boss = false;
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
    // v3 Tier 3 (D196): impact feel. Hit-stop freezes SIMULATION TIME (dt=0)
    // for N frames while frames keep advancing; the zoom punch scales the
    // camera by 1 + zoom_punch * trauma^2 (same curve as shake).
    int   hitstop_frames_kill = 2;      // per ordinary enemy death
    int   hitstop_frames_boss = 6;      // when a boss dies
    float zoom_punch = 0.045f;          // camera zoom gain at full trauma
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

/**
 * EconomyConfig — currency/key drop tuning (Gameplay Phase 2, D5). Every number
 * the drop economy depends on lives here so it can be re-balanced without a
 * rebuild (R6). Drop counts are inclusive bounds on a uniform roll.
 */
struct EconomyConfig {
    int min_drops = 1;                  // currency pickups per kill, lower bound
    int max_drops = 3;                  // ... upper bound
    float key_drop_chance = 0.02f;      // P(a kill also drops a shop key)
    float pickup_lifetime = 12.0f;      // seconds before an uncollected drop fades
    float pickup_size = 16.0f;          // draw + collection radius basis (px)
    float pickup_scatter = 26.0f;       // max offset from the corpse (px)
    float pickup_magnet_speed = 420.0f; // Magnet Core pull speed (Phase 4)
    float pickup_magnet_radius = 220.0f;// Magnet Core pull range (Phase 4)
};

/**
 * ShopUpgradeDef — one row of the shop catalogue (Gameplay Phase 3).
 *
 * `effect` is the string ShopSystem switches on; the JSON order is also the slot
 * in ShipState.upg_counts, so rows may be re-priced or renamed freely but
 * re-ordering them re-labels a player's existing purchase counts. `price` is the
 * first purchase; each later one costs price * ShopConfig::price_growth^count.
 */
struct ShopUpgradeDef {
    std::string name = "Upgrade";
    std::string effect;        // hull | shield | speed | fire_rate | damage | extra_shot
    int price = 50;            // cost of the first purchase
    float amount = 1.0f;       // effect magnitude per purchase
    int max_stacks = 8;        // 0 = unlimited
    // Gameplay Phase 4: the same struct also describes an item or consumable row.
    // Those are *equipped*, not stacked, so max_stacks is ignored for them and
    // `duration` (seconds) is what Overdrive and Phase Shift run on.
    float duration = 0.0f;
};

/**
 * ShopConfig — the shop catalogue and its two shared knobs (Phase 3, D1/R6).
 * Everything the shop costs or grants is data; the code only knows the effect
 * names. Max 8 upgrades — that is the width of ShipState.upg_counts.
 */
struct ShopConfig {
    float price_growth = 1.5f;        // multiplier per repeat purchase
    float shield_regen_delay = 5.0f;  // seconds without damage before shields regen
    // Iteration 3 (#12, D54): regen per second as a fraction of shield_max. Was a
    // hardcoded 0.2 in shop_system.cpp, which refilled a full bank in 5 s and made
    // shields the only stat that mattered. 0.08 is ~12 s.
    float shield_regen_frac = 0.08f;
    std::vector<ShopUpgradeDef> upgrades;
    // Gameplay Phase 4 (D6/D7): the shop's second page. One item and one
    // consumable can be equipped at a time, so these never stack and never
    // escalate in price. `effect` maps to an item_ids/consumable_ids constant.
    // Max 8 rows combined — that is what the 1-8 keys can reach.
    std::vector<ShopUpgradeDef> items;
    std::vector<ShopUpgradeDef> consumables;
    float repulsor_radius = 140.0f;   // Repulsor Field reach (px); push speed is its `amount`
};

/**
 * Iteration-3 config blocks (D51). All four are parsed in the scaffolding phase
 * and consumed later by the lane that owns them, so no lane has to add a block to
 * this shared header while another is editing it. Every value is a balance knob
 * and every default is deliberately inert: a data file with none of these blocks
 * behaves exactly like the pre-iteration-3 game.
 */

/// SustainConfig — periodic health/shield pickups (#10, Lane B).
struct SustainConfig {
    float interval = 0.0f;        // seconds between placements; 0 = feature off
    int   max_live = 3;           // pickups of these kinds alive at once
    float health_amount = 25.0f;  // hull restored by one green scrap
    float shield_amount = 20.0f;  // shield restored by one cell
    float shield_weight = 0.35f;  // P(a placement is a shield rather than health)
    float min_player_dist = 220.0f;  // never place one in the player's lap
};

/// DashConfig — the thruster dash (#5, Lane B).
struct DashConfig {
    float speed = 900.0f;         // burst speed during the dash (px/s)
    float duration = 0.15f;       // seconds the burst lasts
    float cooldown = 2.5f;        // seconds before it is ready again
    float damage = 30.0f;         // dealt to each enemy the dash passes through
    int   charges = 1;            // bursts held at once; +1 per boss killed (#10)
};

/// BatteryConfig — the primary-fire battery (#9). Draining it to empty locks the
/// trigger until it is full again; the recharge rate is constant either way.
struct BatteryConfig {
    float fire_time = 12.0f;      // seconds of continuous fire from full
    float recharge_time = 3.0f;   // seconds from empty to full
};

/// MinimapConfig — the arena minimap (#7, Lane B).
struct MinimapConfig {
    bool  enabled = false;        // off until Lane B lands it
    float x = 640.0f, y = 430.0f; // bottom-left corner in the 800x600 design canvas
    float size = 140.0f;          // square edge length
    int   max_blips = 120;        // hard cap; the overflow is logged, never silent
};

/**
 * SpecialtyConfig — how the two iteration-3 injections reach the spawn stream
 * (#3/#9, D67). Both are *cadences over the spawn counter*, not RNG, so a replay
 * of a seed spawns the same units in the same order.
 *
 * `by_arena` maps an ArenaDef::name onto an enemy_types row name; the loader
 * resolves it into ArenaDef::specialty_unit. A name map rather than an index
 * written into each arena entry: the arena entries and the enemy_types list are
 * authored by different lanes, and an index would silently rot if either moved.
 */
struct SpecialtyPick { std::string arena; std::string type; };

struct SpecialtyConfig {
    int every_n_spawns = 0;       // one arena specialty unit every N spawns; 0 = off
    int moon_every_n_spawns = 0;  // one unlocked shooter every N spawns; 0 = off
    std::vector<SpecialtyPick> by_arena;
    float tier2_hp_mult = 1.6f;   // second pass (waves 26-50): same unit, harder
    float tier2_speed_mult = 1.15f;
};

/// BossConfig — the every-10th-wave boss (#4, Lane D).
struct BossConfig {
    float health = 3000.0f;       // base HP at the first boss; scales per appearance
    float health_growth = 1.6f;   // multiplier per subsequent boss
    float size = 260.0f;
    float contact_damage = 30.0f;
    float summon_interval = 6.0f; // seconds between waves of adds
    int   summon_count = 4;       // adds per summon
    int   reward_choices = 3;     // actives offered on the kill
    // Iteration 3 (D72): the last boss wave is a different fight, not just a
    // bigger one — extra HP on top of the growth curve and extra adds per summon.
    float final_mult = 1.8f;      // health multiplier on the LAST boss wave
    int   final_summon_bonus = 4; // extra adds per summon on the last boss wave
    // Fraction of max HP at which the wave-50 boss asks for the arena shift.
    // See the SEAM comment in boss_system.cpp — nothing consumes it yet.
    float shift_hp_frac = 0.5f;
};

/// ActiveItemDef — one boss-reward active (#4/new-feature note, Lane D).
struct ActiveItemDef {
    std::string name = "Active";
    std::string effect;           // missiles | laser | repulsor_field
    float cooldown = 30.0f;       // the note fixes this at 30 s for all three
    float amount = 40.0f;         // effect magnitude (damage, radius, heal)
    float duration = 5.0f;        // for the effects that persist
};

/**
 * TrailConfig — v3 Tier 7 position-history trails. Presentation only: the
 * history is sampled by the render pass and never read by a gameplay system.
 *
 * `min_spacing` is world units between retained samples. It doubles as the
 * hit-stop guard — with delta_time at 0 nothing moves, so no sample is taken
 * and no trail eats itself (see trail_math::push_sample).
 *
 * `vertex_budget` caps TOTAL ribbon verts per frame across all trails.
 * build_ribbon emits 2 per point, so the default 4000 is ~2000 points, and
 * trails degrade (tail-first, then dropped whole) before the framerate does.
 */
struct TrailConfig {
    bool  enabled       = true;
    int   max_points    = 14;     // history length per entity
    float min_spacing   = 3.0f;   // world units between retained samples
    float shot_width    = 7.0f;   // head width, player + enemy shots
    float drone_width   = 9.0f;   // head width, the player hull
    float dash_width    = 16.0f;  // head width while dash_timer > 0
    int   vertex_budget = 4000;
};

/**
 * DifficultyDef — one selectable run difficulty (Gameplay Phase B, D50).
 *
 * A difficulty is *only* a set of multipliers over the one authored wave table;
 * there is no second table to keep in sync (the same reasoning as D10, which
 * kept per-wave multipliers instead of new enemy types). Everything it scales is
 * enemy-side: the user's call was explicitly that Hard must not make the player's
 * economy harsher, so no player or shop field appears here.
 *
 * `type_lookahead` is the one non-scalar: it pulls each wave's enemy roster
 * forward by N waves, which is how "enemy types unlock earlier" happens without
 * authoring a second `types` list per difficulty.
 */
struct DifficultyDef {
    std::string name = "Normal";
    float count_mult = 1.0f;           // enemies per fixed-count wave
    float spawn_interval_mult = 1.0f;  // <1 = a faster stream (same wave, less spacing)
    float hp_mult = 1.0f;
    float speed_mult = 1.0f;
    float currency_mult = 1.0f;        // credit value of each drop
    float hazard_damage_mult = 1.0f;
    int   type_lookahead = 0;          // waves of enemy-type unlock pulled forward
    // Iteration 3 (D73): "more lethal ... boss" was the one hard-mode ask that
    // could not ship until the boss did. ONE field, scaled in apply_difficulty
    // alongside everything else, rather than a boss-specific difficulty path.
    // Scales boss HP and boss contact damage together.
    float boss_mult = 1.0f;
};

struct GameConfig {
    ArenaConfig arena;
    PlayerConfig player;
    std::vector<EnemyType> enemy_types;
    std::vector<WaveDef> waves;
    std::vector<ArenaDef> arenas;  // v2 Phase 6: themed arenas swapped by wave
    FeedbackConfig feedback;
    PathfindingConfig pathfinding;  // v2 Phase 7: enemy A* tuning
    int victory_wave = 0;          // 0 = survive all waves; N = win after clearing wave N
    // v2 Gameplay Phase 1: waves now advance only on a cleared arena, so an
    // unreachable enemy would soft-lock the run. After this many seconds of a
    // finished-but-uncleared wave, the stragglers are force-killed.
    float wave_stall_timeout = 30.0f;
    EconomyConfig economy;         // v2 Gameplay Phase 2: currency/key drops
    ShopConfig shop;               // v2 Gameplay Phase 3: catalogue + prices
    std::vector<DifficultyDef> difficulties;  // Phase B: run difficulties, index 0 = default
    std::vector<ShipDef> ships;    // Lane F: selectable ships, index 0 = the default hull
    // Iteration 3 (D51) — parsed now, consumed by the lane that owns each.
    SustainConfig sustain;         // #10 health/shield pickups
    DashConfig dash;               // #5 thruster dash
    BatteryConfig battery;         // #9 primary-fire battery
    MinimapConfig minimap;         // #7 minimap
    BossConfig boss;               // #4 boss every 10 waves
    SpecialtyConfig specialty;     // #3/#9 spawn-stream injections
    std::vector<ActiveItemDef> actives;  // boss-reward active items
    BloomConfig bloom;             // v3 Tier 1: render-target bloom
    PostFxConfig postfx;           // v3 Tier 4: SPIR-V post-process (GPU renderer only)
    TrailConfig trails;            // v3 Tier 7: position-history neon trails
    unsigned int seed = 1234u;     // RNG seed for spread/spawn/drops
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

/**
 * Scale a freshly-loaded GameConfig by a difficulty (Phase B, D50). In-place and
 * NOT idempotent — the caller must always start from an unscaled copy of the
 * loaded config, never re-apply on top of an already-scaled one.
 *
 * Pure apart from the mutation, so it unit-tests without a game loop.
 */
/**
 * Overlay a ship onto the player config (Lane F, D82). Like apply_difficulty it
 * must run on a fresh copy of the loaded config, never on top of another ship —
 * the caller re-copies `base_config` first, so there is exactly one place where
 * either overlay happens.
 *
 * Empty sidecar / idle_clip mean "keep the base ship's", so a ship entry that
 * only changes the weapon needs no art fields.
 */
inline void apply_ship(PlayerConfig& player, const ShipDef& s) {
    if (!s.sidecar.empty())   player.sidecar   = s.sidecar;
    if (!s.idle_clip.empty()) player.idle_clip = s.idle_clip;
    player.weapon = s.weapon;
}

inline void apply_difficulty(GameConfig& cfg, const DifficultyDef& d) {
    const size_t n = cfg.waves.size();

    // Type unlocks first, and out of place: pulling wave i+k's roster forward has
    // to read the *original* rosters, so it cannot fold into the scaling loop.
    // An empty `types` already means "every type", so merging into one could only
    // narrow it — those waves are left alone.
    if (d.type_lookahead > 0) {
        std::vector<std::vector<int>> rosters(n);
        for (size_t i = 0; i < n; ++i) {
            std::vector<int> t = cfg.waves[i].types;
            if (!t.empty()) {
                for (int k = 1; k <= d.type_lookahead; ++k) {
                    size_t j = i + static_cast<size_t>(k);
                    if (j >= n) break;
                    const std::vector<int>& next = cfg.waves[j].types;
                    if (next.empty()) { t.clear(); break; }  // "all types" wins
                    t.insert(t.end(), next.begin(), next.end());
                }
                std::sort(t.begin(), t.end());
                t.erase(std::unique(t.begin(), t.end()), t.end());
            }
            rosters[i] = std::move(t);
        }
        for (size_t i = 0; i < n; ++i) cfg.waves[i].types = std::move(rosters[i]);
    }

    for (WaveDef& w : cfg.waves) {
        w.count = std::max(1, static_cast<int>(std::lround(
            static_cast<double>(w.count) * static_cast<double>(d.count_mult))));
        // Floored: a stream faster than one spawn per ~3 frames is a spike, not a
        // difficulty, and the particle budget (2000) is not far above it.
        w.spawn_interval = std::max(0.05f, w.spawn_interval * d.spawn_interval_mult);
        w.hp_mult    *= d.hp_mult;
        w.speed_mult *= d.speed_mult;
    }
    // Timed waves ignore `count`, so their extra pressure is all in the interval
    // above — deliberate: stretching `duration` would lengthen the run instead.

    for (EnemyType& t : cfg.enemy_types) {
        t.currency = std::max(1, static_cast<int>(std::lround(
            static_cast<double>(t.currency) * static_cast<double>(d.currency_mult))));
    }
    for (ArenaDef& a : cfg.arenas) {
        for (HazardDef& h : a.hazards) h.damage *= d.hazard_damage_mult;
    }

    // Iteration 3 (D67): the moon shooters are not in any wave's `types` list —
    // they are injected on a spawn cadence from their own first_wave. That is a
    // second unlock axis, so type_lookahead has to pull it forward too, or Hard's
    // "enemy types arrive earlier" promise would silently skip half the roster.
    if (d.type_lookahead > 0) {
        for (EnemyType& t : cfg.enemy_types) {
            if (t.first_wave > 1) {
                t.first_wave = std::max(1, t.first_wave - d.type_lookahead);
            }
        }
    }

    // D73: the boss is enemy-side like everything else here, so it scales with
    // the same one-field discipline rather than a boss-specific difficulty path.
    cfg.boss.health *= d.boss_mult;
    cfg.boss.contact_damage *= d.boss_mult;
}

/// Parse GameData.json into a GameConfig. Missing fields fall back to defaults;
/// throws std::runtime_error only on an unopenable file or malformed JSON.
GameConfig load_arena_config(const std::string& file_path);

#endif // ARENA_CONFIG_HPP
