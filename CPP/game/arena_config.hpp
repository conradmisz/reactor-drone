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
// Engine-suite Phase 0 (D138): `hp` makes an obstacle destructible (#9, Lane U).
// 0 = indestructible, the default — every arena shipped so far is unchanged.
struct ObstacleDef { float x = 0.0f, y = 0.0f, w = 40.0f, h = 40.0f; float hp = 0.0f; };
struct HazardDef   { float x = 0.0f, y = 0.0f, w = 40.0f, h = 40.0f; float damage = 10.0f; };

/**
 * ArenaDef — one themed arena (v2, Phase 6): its parallax backdrop plus obstacle
 * and hazard layouts. `first_wave` is the (1-based) wave at which this arena
 * becomes active; the active arena is the last one whose first_wave <= the
 * current wave (see active_arena_index). Arena geometry (radius/centre/spawn
 * ring) stays shared on ArenaConfig — only the theme + props swap.
 */
/**
 * SurgeDef — one reactor surge event an arena can schedule mid-wave (engine-suite
 * #7, Lane X). Parsed in Phase 0 (D138) and inert until surge_system lands: an
 * empty `surges` list per arena is the shipped default.
 */
struct SurgeDef {
    std::string effect;           // slow_field | sweep_line | eruption | gravity_storm
    int   first_wave = 1;         // wave window this surge may fire in...
    int   last_wave  = 0;         // ...0 = no upper bound
    float chance = 0.0f;          // P(fires in an eligible wave); 0 = never
    float magnitude = 1.0f;       // effect-specific strength
    float duration = 4.0f;        // seconds the effect lives
    float radius = 160.0f;        // region size (circle radius / line half-width)
    float telegraph = 1.5f;       // warning-glow seconds before it goes live
};

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
    // Engine-suite Phase 0 (D138): this arena's surge-event table (#7, Lane X).
    // Empty = no surges, i.e. every arena shipped so far.
    std::vector<SurgeDef> surges;
    // Roguelite phase 5 (design §4): this theme's signature mechanic, resolved
    // onto the ArenaDef and read in exactly one place each
    // (arena_mechanics.hpp) — the Foundry-mines shape.
    //   light_radius  > 0 turns The Shroud on: everything past this many px
    //                 from the drone fades toward invisible.
    //   drift_x/y     px/s of current pushing the drone, the enemies and the
    //                 loot. 0,0 = no current.
    float light_radius = 0.0f;
    float drift_x = 0.0f, drift_y = 0.0f;
};

struct WeaponConfig {
    float fire_rate = 4.0f;
    float damage = 20.0f;
    float projectile_speed = 500.0f;
    float projectile_lifetime = 1.2f;
    float spread = 0.0f;
    // Gameplay pack (D221): per-weapon projectile half-size and piercing.
    float projectile_size = 6.0f;
    bool pierce = false;
    // Playtest #1 item 9 (D227): render as a fixed-length blaster bolt rather
    // than a position-history tracer ribbon.
    bool bolt = false;
    float bolt_length = 26.0f;   // px, along the heading
};

struct PlayerConfig {
    float start_health = 100.0f;
    float start_shield = 0.0f;     // gameplay pack (D221): shield at spawn (per-ship)
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
 * WeaponDef — one installable weapon (gameplay pack v2.3, D221/D222).
 *
 * Weapons used to be an anonymous WeaponConfig embedded in each ship; the pack
 * makes them first-class so any owned drone can install any owned weapon. The
 * projectile colour lives here (supersedes D184's ship-complement rule — the
 * spec assigns each weapon its own identity colour), and the primary-fire
 * battery is per-weapon (`fire_time`/`recharge_time`, the D192 #9 knobs).
 * `secondary`/`secondary_cd` are parsed now, consumed by the secondary-fire
 * tier (the D51 convention: inert until its owner lands).
 */
struct WeaponDef {
    std::string name = "55 Iron";
    WeaponConfig stats;
    float fire_time = 12.0f;       // battery seconds of continuous fire
    float recharge_time = 3.0f;    // battery seconds empty -> full
    uint8_t color_r = 255, color_g = 70, color_b = 70;
    std::string secondary;         // secondary-fire behavior id ("" = none yet)
    float secondary_cd = 10.0f;
};

/**
 * CosmeticColorDef — one paint (gameplay pack v2.3 tier 7, D221). A color is
 * ONE purchase usable in every slot type (ship body via its own baked atlas,
 * trail tint, projectile tint). `granted_by` names the ship that grants it
 * free (ownership derives, D81); "" = shop-only, bought with scrap.
 */
struct CosmeticColorDef {
    std::string name = "Cyan";
    uint8_t r = 90, g = 220, b = 255;
    int price = 0;
    std::string sidecar;         // body atlas for this paint (relative to assets/)
    std::string granted_by;      // ship whose purchase grants it; "" = shop-only
};

/// Index of `name` in `colors`, or -1. Pure.
inline int find_color(const std::vector<CosmeticColorDef>& colors, const std::string& name) {
    for (size_t i = 0; i < colors.size(); ++i)
        if (colors[i].name == name) return static_cast<int>(i);
    return -1;
}

/// Index of `name` in `weapons`, or -1. Pure — unit-tested.
inline int find_weapon(const std::vector<WeaponDef>& weapons, const std::string& name) {
    for (size_t i = 0; i < weapons.size(); ++i)
        if (weapons[i].name == name) return static_cast<int>(i);
    return -1;
}

/**
 * ShipDef — one selectable player ship (Lane F, D82; stats + ownership added by
 * the gameplay pack, D221).
 *
 * A ship is now a stat profile (hull/shield/speed/dash), a default weapon (by
 * WeaponDef name), a special-attribute id, and a scrap price. Ownership is
 * price-based: `scrap_cost` 0 = always owned, otherwise the ship must appear in
 * MetaSave::owned_ships (bought in the hangar). `locked` ships are neither
 * owned nor purchasable regardless of price (the 4th drone, pre-release).
 * `unlock_score` (lifetime-score gating) retired with D221 call #1.
 */
struct ShipDef {
    std::string name = "Falcon";
    std::string sidecar;
    std::string idle_clip = "idle";
    std::string default_weapon;    // WeaponDef name granted with the ship
    std::string special;           // special-attribute id ("" = none); consumed by ship-specials tier
    float hull = 100.0f;
    float shield = 0.0f;           // shield the run STARTS with (Gryphon)
    float speed = 260.0f;
    float dash_mult = 1.0f;        // scales DashConfig::speed (distance = speed*duration)
    int scrap_cost = 0;            // 0 = always owned
    bool locked = false;           // true = unreleased (never owned/purchasable)
    // D184: the hull's identity hue, matching the baked sprite art. (Shot colour
    // moved to WeaponDef with the pack; this hue still drives trails/UI.)
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
    // Engine-suite Phase 0 (D138): the authored bullet pattern this type fires
    // (#2, Lane Y) — a BulletPatternDef::name, dispatched from the enemy-fire
    // hook. Empty = whatever `behavior` already does, i.e. every type today.
    std::string pattern;
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
    // v3 Tier 3 (D209): impact feel. Hit-stop freezes SIMULATION TIME (dt=0)
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
    // Gameplay pack (D221) spec: slightly richer drops before the first boss.
    int early_bonus_wave = 10;          // waves BELOW this get the floor below
    int early_min_drops = 1;            // min_drops floor in those waves
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
    // Gameplay pack (D221) spec: more pickups late, all of them bigger.
    int   late_wave = 16;            // 1-based wave at which the cap grows
    int   late_max_live = 4;         // cap from late_wave on
    float pickup_scale = 1.5f;       // draw/collection scale vs economy pickup_size
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

/// ScrapConfig — the persistent between-run currency awards (gameplay pack,
/// D221 call #5). All first-pass numbers; a tuning table, not a design.
struct ScrapConfig {
    int per_wave = 5;         // per wave cleared
    int boss_bonus = 25;      // extra per boss wave cleared (10/20/30)
    int victory_bonus = 100;  // extra for finishing wave 30
};

/// Scrap earned by a run: `waves_cleared` full waves (victory = all of them),
/// one boss bonus per boss wave inside that count. Pure — unit-tested.
inline int scrap_for_run(int waves_cleared, bool victory, const ScrapConfig& c,
                         int boss_every = 10, int total_waves = 30) {
    if (waves_cleared < 0) waves_cleared = 0;
    if (waves_cleared > total_waves) waves_cleared = total_waves;
    int bosses = boss_every > 0 ? waves_cleared / boss_every : 0;
    return waves_cleared * c.per_wave + bosses * c.boss_bonus
         + (victory ? c.victory_bonus : 0);
}

/**
 * Shuffle the run's arena rotation (gameplay pack, D221 call #6). The authored
 * `first_wave` ladder is FIXED — only the occupants shuffle. Two rules from the
 * owner: the last slot (Singularity, wave 30) is pinned, and no "Prism*" arena
 * may open the run. Assumes `arenas` is authored in ascending first_wave order,
 * which the data test pins. Deterministic for a given rng state — the caller
 * seeds from the run seed, so replays and resumes reproduce the same order.
 */
template <class URNG>
inline void shuffle_arena_order(std::vector<ArenaDef>& arenas, URNG& rng) {
    if (arenas.size() < 3) return;
    std::vector<int> ladder;
    ladder.reserve(arenas.size());
    for (const ArenaDef& a : arenas) ladder.push_back(a.first_wave);
    std::shuffle(arenas.begin(), arenas.end() - 1, rng);   // finale pinned
    if (arenas.front().name.rfind("Prism", 0) == 0) {
        for (size_t i = 1; i + 1 < arenas.size(); ++i) {
            if (arenas[i].name.rfind("Prism", 0) != 0) { std::swap(arenas[0], arenas[i]); break; }
        }
    }
    for (size_t i = 0; i < arenas.size(); ++i) arenas[i].first_wave = ladder[i];
}

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
    // Gameplay pack (D221 call #7): the 2-phase enrage. Below enrage_frac the
    // boss attacks faster (cadence multiplied), denser (bonus shots/patches)
    // and hunts harder (speed multiplied). First-pass tuning table.
    float enrage_frac = 0.5f;
    float enrage_cadence_mult = 0.6f;
    float enrage_speed_mult = 2.0f;
    int   enrage_volley_bonus = 6;
    int   enrage_patch_bonus = 2;
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
 * Engine-suite Phase-0 config blocks (D138). Same playbook as the iteration-3
 * blocks above: all parsed now, each consumed later by the lane that owns it, so
 * no lane edits this shared header while another is building. Every default is
 * deliberately inert — a data file with none of these blocks plays exactly like
 * the pre-suite game, and the replay canary is the proof.
 */

/// TimescaleConfig — Temporal Overload bullet-time (#1, Lane P). Sim-side: the
/// scale is a pure function of sim state, so replays stay identical.
struct TimescaleConfig {
    bool  enabled = false;        // false => the multiplier is exactly 1.0f
    float kill_scale = 0.45f;     // target scale on a kill-chain beat
    float kill_hold = 0.22f;      // seconds the beat holds before easing back
    int   chain_kills = 3;        // kills inside chain_window to trigger the beat
    float chain_window = 1.2f;    // seconds
    float hull_scale = 0.6f;      // target scale while hull is critical
    float hull_frac = 0.15f;      // "critical" = hull below this fraction
    float ease_per_sec = 6.0f;    // exponential ease rate toward the target
    float min_scale = 0.35f;      // floor — the sim never fully stops
};

/// DirectorConfig — the adaptive pacing hand (#8, Lane Q). Scales wave-spawn
/// *spacing* only; never counts, never skips the table.
struct DirectorConfig {
    bool  enabled = false;        // false => the spacing multiplier is exactly 1.0f
    float min_mult = 0.7f;        // pressure arrives early when cruising
    float max_mult = 1.3f;        // spawns hold off after a scramble
    float damage_weight = 1.0f;   // stress from recent damage taken
    float kill_weight = 0.5f;     // relief from recent kills
    float hull_weight = 1.0f;     // standing stress from missing hull
    float ema_per_sec = 0.8f;     // EMA rate the recent-events signals decay at
};

/// ResonanceConfig — the resonance grid, the arena as a physics display (Lane R).
/// Render-only: no sim system may ever read grid state.
struct ResonanceConfig {
    bool  enabled = false;
    // D151: the lattice is sized FROM THE ARENA (configure_for_arena), because the
    // first version's fixed 40x28 covered 1600 px of a 2800 px arena and visibly
    // stopped short of the wall. Only the pitch is authored.
    float spacing = 64.0f;        // px between nodes; 512/8, commensurate with the
                                  // 512px backdrop tiles on the frames both show
    float stiffness = 60.0f;      // spring constant toward rest position
    float damping = 5.0f;         // velocity bleed per second
    float impulse_scale = 90.0f;  // maps fx_events::Impulse.strength to a kick
    float max_offset = 26.0f;     // px a node may leave its rest position
    // PEAK line colour, not a resting one: at rest the lattice is not drawn at
    // all (D151), which is what stops it reading as clutter over the backdrop.
    uint8_t r = 120, g = 215, b = 255, a = 190;
};

/// ForceConfig — the force-field layer (#3, Lane T). Inert by zero registered
/// sources rather than a flag: the accumulation pass iterates nothing.
struct ForceConfig {
    int max_sources = 32;         // fixed-capacity source array (MCU headroom)
};

/// PaletteDef / PaletteConfig — the palette engine (#5, Lane W). The lane's spec
/// resolves the feasibility gate (true LUT vs palette-driven tinting); either
/// way a palette is a named row of colours selected by arena/state/unlock.
struct PaletteDef {
    std::string name;
    std::vector<uint32_t> colors;  // 0xRRGGBB entries; roles defined by Lane W
};

struct PaletteConfig {
    bool enabled = false;
    std::vector<PaletteDef> palettes;
};

/// BulletPatternOp / BulletPatternDef — the danmaku language (#2, Lane Y). A
/// pattern is an op list interpreted from the enemy-fire hook; new patterns are
/// data, not C++. No RNG in the interpreter — variation is authored.
struct BulletPatternOp {
    std::string type;             // ring | fan | spiral | aimed | wait
    int   count = 8;              // shots this op emits
    float speed = 220.0f;         // projectile speed
    float spread_deg = 360.0f;    // arc the shots cover (fan/ring)
    float angular_vel_deg = 0.0f; // spiral rotation per second
    float interval = 0.0f;        // seconds between shots within the op; 0 = burst
    float wait = 0.0f;            // seconds (type == "wait")
};

struct BulletPatternDef {
    std::string name;
    bool loop = true;             // restart the op list when it finishes
    std::vector<BulletPatternOp> ops;
};

/// AudioConfig — chip-synth audio (#4, Lane Z). false => no device is opened.
struct AudioConfig {
    bool  enabled = false;
    float master_volume = 0.8f;
    int   sample_rate = 48000;
    int   voices = 8;             // fixed voice pool: pulse x3, saw x2, tri, noise x2
};

/// FlightReportConfig — the run-as-artifact recorder (#10, Lane S). Passive:
/// reads sim state into fixed ring buffers, renders on terminal screens only.
struct FlightReportConfig {
    bool enabled = false;
    int  sample_every_n = 6;      // player position every N frames
    int  max_samples = 4096;      // fixed ring buffer (MCU headroom)
    // Where the report square sits, in the 800x600 UI design canvas (Lane S).
    float x = 250.0f, y = 150.0f, size = 300.0f;
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
    std::vector<WeaponDef> weapons; // gameplay pack (D221): installable weapons
    std::vector<CosmeticColorDef> cosmetic_colors; // tier 7: paints
    ScrapConfig scrap;              // gameplay pack (D221): persistent-currency awards
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
    // Engine-suite Phase 0 (D138) — parsed now, consumed by the lane that owns each.
    TimescaleConfig timescale;     // #1 Temporal Overload (Lane P)
    DirectorConfig director;       // #8 Adaptive Director (Lane Q)
    ResonanceConfig resonance;     // RG Resonance Grid (Lane R). NOT `grid` —
                                   // top-level JSON "grid" is the baseline loader's.
    FlightReportConfig flight_report;  // #10 Flight Report (Lane S)
    ForceConfig forces;            // #3 Force-Field Layer (Lane T)
    PaletteConfig palettes;        // #5 Palette Engine (Lane W)
    std::vector<BulletPatternDef> patterns;  // #2 Bullet-Pattern Language (Lane Y)
    AudioConfig audio;             // #4 Chip-Synth Audio (Lane Z)
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
    // Gameplay pack (D221): the stat profile overlays here; the weapon does NOT —
    // weapons are first-class WeaponDefs, overlaid by start_run from the equipped
    // loadout (any owned drone can install any owned weapon).
    player.start_health = s.hull;
    player.start_shield = s.shield;
    player.move_speed   = s.speed;
}

/// Overlay the equipped weapon onto the player config + battery (same discipline
/// as apply_ship: fresh config copy, exactly one application site in start_run).
inline void apply_weapon(PlayerConfig& player, BatteryConfig& battery, const WeaponDef& w) {
    player.weapon         = w.stats;
    battery.fire_time     = w.fire_time;
    battery.recharge_time = w.recharge_time;
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
