/**
 * Class-110 Final Project — "Reactor Drone" arena survival shooter.
 *
 * Forked from Class-090 (tower defense). You pilot a maintenance drone in a
 * circular reactor core; enemies spawn from the ring and seek you in escalating
 * waves. Move with the arrow keys, aim with the mouse, hold the mouse button to
 * fire and SPACE to dash. Kills give score and drop currency pickups you walk over, which
 * fund the shop. Title -> play -> game-over/victory, click to (re)start.
 *
 * World space is bottom-left origin (0,0 = bottom-left, +X right, +Y up); the
 * Y-flip lives only in RenderSystem::draw_entity().
 */

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <memory>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <future>
#include <random>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <iterator>

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/destruction.hpp"
#include "engine/ecs/fx_events.hpp"
#include "engine/timer.hpp"
#include "engine/resource_manager.hpp"
#include "engine/project_paths.hpp"
#include "engine/gamedata_loader.hpp"
#include "engine/sidecar_loader.hpp"

#include "engine/ecs/systems/input_system.hpp"
#include "engine/ecs/systems/player_control_system.hpp"
#include "engine/ecs/systems/movement_system.hpp"
#include "engine/ecs/systems/collision_system.hpp"
#include "engine/ecs/systems/quadtree_strategy.hpp"
#include "engine/ecs/systems/lifetime_system.hpp"
#include "engine/ecs/systems/animation_system.hpp"
#include "engine/ecs/systems/particle_system.hpp"
#include "engine/ecs/systems/camera_system.hpp"
#include "engine/ecs/systems/render_system.hpp"
#include "engine/ecs/systems/hud_system.hpp"
#include "engine/ecs/systems/screenshot_system.hpp"
#include "engine/ecs/systems/trail_math.hpp"   // v3 Tier 7 (D213)
#include "engine/ecs/systems/resonance_grid_system.hpp"   // Lane R (D140)
#include "engine/ecs/systems/palette_system.hpp"          // Lane W (D147)

#include <nlohmann/json.hpp>

#include "net/http_client.hpp"
#include "net/net_config.hpp"
#include "cli_parser.hpp"
#include "script_loader.hpp"
#include "debug_state.hpp"
#include "arena_config.hpp"
#include "arena_mechanics.hpp"
#include "arena_vfx.hpp"
#include "explosion_fx.hpp"      // v3 Tier 11 (D216)
#include "player_components.hpp"
#include "enemy_components.hpp"
#include "collision_layers.hpp"
#include "player_aim_system.hpp"
#include "player_fire_system.hpp"
#include "enemy_seek_system.hpp"
#include "player_damage_system.hpp"
#include "projectile_hit_system.hpp"
#include "damage_apply_system.hpp"
#include "enemy_death_system.hpp"
#include "pickup_system.hpp"
// Lane B (iteration 3): sustain pickups (#10), thruster dash (#5), minimap (#7).
#include "sustain_spawn_system.hpp"
#include "dash_system.hpp"
#include "ship_specials.hpp"   // gameplay pack (D221): veil + special ids
#include "secondary_fire.hpp"   // gameplay pack (D221): right-mouse secondaries
#include "hangar_stats.hpp"     // gameplay pack (D221): hangar stat rows + pips
// Engine suite: Lane P (#1 Temporal Overload, D139).
#include "timescale_system.hpp"
// Engine suite: Lane Q (#8 Adaptive Director, D142).
#include "director_system.hpp"
// Engine suite: Lane S (#10 Flight Report, D143).
#include "flight_report.hpp"
// Engine suite: Lane T (#3 Force-Field Layer, D144).
#include "force_field_system.hpp"
// Engine suite: Lane U (#9 Destructible Arena, D146).
#include "crumble_system.hpp"
// Engine suite: Lane X (#7 Reactor Surges, D149) and Lane Y (#2 patterns, D148).
#include "surge_system.hpp"
#include "bullet_pattern.hpp"
// Lane N (iteration 5, D123): the shop-upgrade look on the drone's plume.
#include "upgrade_visuals.hpp"
#include "minimap_system.hpp"
#include "pause_stats.hpp"
#include "wave_spawner_system.hpp"
#include "shop_system.hpp"
// Iteration 3 / Lane D (#3, #9, #4). Headers only — every call site is inside
// this lane's four `// === HOOK: ... ===` blocks, and every system instance is a
// function-local static in the block that owns it, so no other lane's lines move.
#include "enemy_fire_system.hpp"
#include "specialty_system.hpp"
#include "boss_system.hpp"
#include "active_items.hpp"

// UI & menu layer (Option-040 port).
#include "engine/lua_manager.hpp"
#include "engine/ecs/systems/screen_stack_system.hpp"
#include "engine/ecs/systems/ui_render_system.hpp"
#include "engine/ecs/systems/bloom_system.hpp"
#include "engine/ecs/systems/postfx_system.hpp"
#include "engine/ecs/systems/ui_render_math.hpp"   // ui_canvas_transform (#11 dash face)
#include "engine/ecs/systems/ui_system.hpp"
#include "shield_system.hpp"
#include "item_system.hpp"
#include "game_hud_system.hpp"
#include "feedback.hpp"
#include "flash_system.hpp"
#include "parallax.hpp"
#include "obstacles.hpp"
#include "enemy_path.hpp"
#include "meta_save.hpp"
#include "prestige.hpp"
#include "run_save.hpp"
#include "settings_save.hpp"
#include "telemetry.hpp"
#include "version.hpp"   // GAME_VERSION, stamped into every run report
#include "update_check.hpp"  // in-game updater: is_newer + trusted_installer_url

struct SDL_WindowDeleter { void operator()(SDL_Window* w) const { if (w) SDL_DestroyWindow(w); } };
struct SDL_RendererDeleter { void operator()(SDL_Renderer* r) const { if (r) SDL_DestroyRenderer(r); } };
using SDL_WindowPtr = std::unique_ptr<SDL_Window, SDL_WindowDeleter>;
using SDL_RendererPtr = std::unique_ptr<SDL_Renderer, SDL_RendererDeleter>;

// Game phases (Blackboard "phase").
enum Phase { PHASE_TITLE = 0, PHASE_PLAYING = 1, PHASE_GAMEOVER = 2, PHASE_VICTORY = 3,
             PHASE_SHOP = 4, PHASE_INTERMISSION = 5, PHASE_NAME_ENTRY = 6,
             PHASE_LEADERBOARD = 7, PHASE_FEEDBACK = 8 };

// Which OS this build reports in feedback rows (specs/feedback-reports.md).
#if defined(_WIN32)
constexpr const char* RD_PLATFORM = "win";
#elif defined(__APPLE__)
constexpr const char* RD_PLATFORM = "mac";
#else
constexpr const char* RD_PLATFORM = "linux";
#endif

// Screen name of the between-waves prompt, authored in GameData.json's "screens".
constexpr const char* SCREEN_INTERMISSION = "wave_intermission";
constexpr const char* SCREEN_RUN_STATS    = "run_stats";        // gameplay pack tier 6 (D221)
constexpr const char* SCREEN_PAUSE        = "pause";
constexpr const char* SCREEN_MAIN_MENU    = "main_menu";
constexpr const char* SCREEN_RUN_SETUP    = "run_setup";    // main-menu-suite Phase A
constexpr const char* SCREEN_SAVE_SLOTS   = "save_slots";   // main-menu-suite Phase B
constexpr const char* SCREEN_SETTINGS     = "settings";     // main-menu-suite Phase C
constexpr const char* SCREEN_RECORDS      = "records";
constexpr const char* SCREEN_HOW          = "how_to_play";
constexpr const char* SCREEN_PRESTIGE     = "prestige_offer";   // Lane O (D128)
constexpr const char* SCREEN_NAME_ENTRY   = "name_entry";       // Task 7 (D195)
constexpr const char* SCREEN_LEADERBOARD  = "leaderboard";      // Task 9
constexpr const char* SCREEN_FEEDBACK     = "feedback";         // specs/feedback-reports.md

// Task 7 review round 1, Critical 2: std::async(std::launch::async)'s future
// blocks in ITS DESTRUCTOR until the request finishes (up to the 8s
// CURLOPT_TIMEOUT — see net/http_client.hpp). An abandoned /register future
// (ESC-skip, re-entering via N, or app shutdown mid-request) must never let
// that destructor run synchronously on the main thread. Moving it in here
// instead: the vector is heap-allocated once and intentionally never freed,
// so the abandoned future's destructor never runs before process exit — the
// OS reclaims it. One call site; not worth a generic task manager.
void abandon_future(std::future<net::Response>&& f) {
    if (!f.valid()) return;
    static auto* graveyard = new std::vector<std::future<net::Response>>();
    graveyard->push_back(std::move(f));
}

int main(int argc, char* argv[]) {
    auto opts = parse_command_line(argc, argv);
    if (opts.help_requested) return 0;
    if (opts.parse_error) return 1;
    if (!opts.script_file.empty()) {
        std::string script_path = opts.script_file;
        opts = load_script(script_path);
        opts.script_file = script_path;
    }
    // Determinism invariant: any scripted/headless run (--stopframe) must make
    // zero network calls, so the leaderboard/update client is disabled whenever
    // one is present. See agentProjectDocs/architecture.md Invariants #4.
    net::set_enabled(!opts.stop_frame.has_value());

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    if (!TTF_Init()) {
        std::cerr << "Failed to initialize SDL_ttf: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_WindowPtr window(SDL_CreateWindow("Reactor Drone v2", 980, 660, SDL_WINDOW_RESIZABLE));
    if (!window) { std::cerr << "Window: " << SDL_GetError() << std::endl; SDL_Quit(); return 1; }
    // v3 Tier 4 (D210): prefer the SDL GPU renderer so PostFxSystem can attach
    // SPIR-V fragment shaders to ordinary draws. Created via properties with
    // the SPIRV capability declared. Anything can refuse it — --classic-renderer,
    // a headless driver, no Vulkan — and the classic renderer is byte-for-byte
    // the pre-Tier-4 pipeline (PostFx self-disables off a non-GPU renderer).
    SDL_RendererPtr renderer;
    // D212 (v3 Tier 6a): the GPU renderer is now the DEFAULT, as the v3 plan
    // always specified; --classic-renderer is the escape hatch. The opt-in was a
    // bugs/009 stability hold, resolved by the 2026-08-11 system SDL update and
    // signed off by the 2026-08-13 windowed playtest. --gpu-renderer is kept as
    // an accepted no-op so existing scripts and the run.py forwarder still work.
    // Headless is unaffected without an explicit guard: the dummy/offscreen
    // drivers refuse a "gpu" renderer, so creation fails and the fallback below
    // lands on the classic path the canary baseline was built on.
    if (!opts.classic_renderer) {
        SDL_PropertiesID rp = SDL_CreateProperties();
        SDL_SetStringProperty(rp, SDL_PROP_RENDERER_CREATE_NAME_STRING, "gpu");
        SDL_SetPointerProperty(rp, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, window.get());
        SDL_SetBooleanProperty(rp, SDL_PROP_RENDERER_CREATE_GPU_SHADERS_SPIRV_BOOLEAN, true);
        renderer.reset(SDL_CreateRendererWithProperties(rp));
        SDL_DestroyProperties(rp);
    }
    if (!renderer) {
        renderer.reset(SDL_CreateRenderer(window.get(), nullptr));
    }
    if (!renderer) { std::cerr << "Renderer: " << SDL_GetError() << std::endl; SDL_Quit(); return 1; }
    if (opts.verbose) {
        std::cout << "Renderer: " << SDL_GetRendererName(renderer.get()) << std::endl;
    }
    // v3 Tier 0: vsync for tear-free presentation. The Timer's busy-wait stays as a
    // pacing floor; measured delta_time drives windowed play, and --seed runs force
    // set_deterministic(true), so replay determinism is untouched. Best-effort: the
    // dummy/offscreen drivers reject vsync and that is fine.
    SDL_SetRenderVSync(renderer.get(), 1);

    // === ECS + config ===
    EntityManager entity_manager;
    ComponentStorage component_storage;
    Blackboard blackboard;

    const std::string gamedata_path = project_paths::assets_dir() + "/GameData.json";
    // Engine loader sets window/camera (and ignores our arena blocks).
    load_game_data(gamedata_path, entity_manager, component_storage, blackboard);
    GameConfig config = load_arena_config(gamedata_path);
    if (opts.seed.has_value()) config.seed = static_cast<unsigned int>(opts.seed.value());

    // Engine suite (D141): --suite flips every feature on for a playtest. The
    // shipped GameData leaves them all disabled so the replay canary is
    // byte-identical by default; this is the ONE switch, applied before
    // base_config is taken so a restart mid-session keeps the suite on.
    //
    // Two of them are inert by shape rather than by a flag and so are absent
    // here: the force layer is inert with zero registered sources, and the
    // bullet patterns / arena surges are inert with empty data tables — a flag
    // could not turn either on without authored content.
    if (opts.suite) {
        config.timescale.enabled = true;
        config.director.enabled = true;
        config.resonance.enabled = true;
        config.flight_report.enabled = true;
        config.palettes.enabled = true;
        // Absent here on purpose (D151): the battle-scar layer was CUT, chip-synth
        // audio is SHELVED (its code still builds, nothing constructs it), and the
        // force layer is inert by shape rather than by a flag. Destructible arenas
        // and surges need authored DATA, so --suite writes it below — the shipped
        // rows stay untouched, or the default game would change with them.
        for (ArenaDef& a : config.arenas)
            for (ObstacleDef& o : a.obstacles)
                if (o.hp <= 0.0f) o.hp = 240.0f;   // ~12 standard shots
        // Surges are per-arena data too. One table for every arena rather than a
        // themed one each: this switch exists to make the suite VISIBLE in a
        // playtest, not to author the real content, which is a design pass.
        for (ArenaDef& a : config.arenas) {
            if (!a.surges.empty()) continue;
            SurgeDef flood;  flood.effect = "slow_field";
            flood.first_wave = 2;  flood.chance = 0.45f;  flood.duration = 7.0f;
            flood.radius = 220.0f; flood.telegraph = 1.5f;
            SurgeDef arc;    arc.effect = "sweep_line";
            arc.first_wave = 4;    arc.chance = 0.35f;    arc.duration = 9.0f;
            arc.radius = 260.0f;   arc.telegraph = 1.8f;
            SurgeDef storm;  storm.effect = "gravity_storm";
            storm.first_wave = 6;  storm.chance = 0.30f;  storm.duration = 6.0f;
            storm.radius = 240.0f; storm.telegraph = 1.2f;
            a.surges = {flood, arc, storm};
        }
        // The boss fires authored choreography instead of its stock volley.
        for (EnemyType& t : config.enemy_types)
            if (t.behavior == "boss" && t.pattern.empty()) t.pattern = "reactor_bloom";
        std::cout << "Engine suite: ON (--suite)\n";
    }

    // Phase B (D50): `config` is the *scaled* config every system holds a pointer
    // to, so difficulty is applied by re-copying this pristine one over it at the
    // start of a run. apply_difficulty is not idempotent — never scale in place.
    const GameConfig base_config = config;

    // Lane F (D80/D82): the only state that outlives a run. Read once, here, and
    // never again — it decides what the title menu offers and nothing else, so a
    // replay of a given seed and ship is identical with or without a save file.
    // The ship *choice* is deliberately not persisted, for the same reason.
    MetaSave meta = meta_load(meta_save_path());
    // Task 7: the player id is generated once, at first startup, and persisted
    // immediately — before a name is ever registered — so it survives a player
    // who skips name entry and relaunches. Never regenerated once present.
    if (meta.player_id.empty()) {
        meta.player_id = generate_uuid();
        meta_write(meta_save_path(), meta);
    }
    int selected_ship = 0;
    // Gameplay pack (D221): the hangar loadout persists. Boot on the equipped
    // ship when the save names an owned one; anything else falls back to 0.
    for (size_t i = 0; i < config.ships.size(); ++i) {
        if (config.ships[i].name == meta.equipped_ship && ship_owned(meta, config.ships[i])) {
            selected_ship = static_cast<int>(i);
            break;
        }
    }
    // Main-menu-suite Phase A: the difficulty picked on the run_setup screen,
    // consumed by LAUNCH. SPACE quick-start ignores it on purpose (always Normal).
    int setup_difficulty = 0;

    // Lane K (D100): the mid-run save, read exactly once and for exactly one
    // purpose — whether the title screen offers CONTINUE. It is applied only on
    // the resume path, so a save file that merely exists cannot move a single RNG
    // draw of a fresh run (verified: the replay canary is byte-identical with and
    // without `saves/run.json`).
    // Main-menu-suite Phase B: three slots. The legacy single save migrates to
    // slot 1 once; `active_slot` is where the pause SAVE writes — a loaded run
    // saves back to its own slot, a fresh run claims the first empty one (else 1).
    run_save_migrate_legacy();
    RunSave saved_slots[RUN_SAVE_SLOTS];
    for (int i = 0; i < RUN_SAVE_SLOTS; ++i)
        saved_slots[i] = run_save_load(run_save_path(i + 1));
    int active_slot = 0;   // 0-based index into saved_slots
    auto newest_slot = [&]() {
        int best = -1;
        long long best_at = -1;
        for (int i = 0; i < RUN_SAVE_SLOTS; ++i)
            if (saved_slots[i].present && saved_slots[i].saved_at >= best_at) {
                best = i;
                best_at = saved_slots[i].saved_at;
            }
        return best;   // -1 = no save anywhere
    };
    auto first_free_slot = [&]() {
        for (int i = 0; i < RUN_SAVE_SLOTS; ++i)
            if (!saved_slots[i].present) return i;
        return 0;
    };
    int run_difficulty = 0;   // which difficulty the live run was started at

    // v3 Tier 3 (D209): pending hit-stop frames. Set by the kill/boss sites,
    // consumed after timer.update_blackboard each frame: while positive, the
    // published delta_time is overridden to 0 so every system integrates zero
    // motion — draw/RNG counts are unchanged (systems still run), frames still
    // advance (end_frame is untouched), so --stopframe and the canary cannot
    // hang. Declared this early because the enemy-death lambda captures it.
    int hitstop_left = 0;

    const int win_w = blackboard.get_or<int>("window_width", 980);
    const int win_h = blackboard.get_or<int>("window_height", 660);

    // v2 Upgrade Phase 5: one authority for "screen size". The renderer scales a fixed
    // win_w x win_h logical surface to whatever the window actually is (fullscreen,
    // resize, HiDPI), so CameraSystem / InputSystem / the HUD — which all read
    // window_width/height from the blackboard — stay correct with no resize plumbing.
    SDL_SetRenderLogicalPresentation(renderer.get(), win_w, win_h,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // Camera starts on the arena centre; from here on it follows the player
    // (Upgrade Phase 2 — see the follow/shake block in the main loop).
    blackboard.set<float>("camera.lookat.x", config.arena.center_x);
    blackboard.set<float>("camera.lookat.y", config.arena.center_y);
    blackboard.set<float>("camera.zoom", 1.0f);
    blackboard.set<float>("player.invuln_window", config.player.invuln_window);
    // Gameplay Phase 3: PlayerDamageSystem restarts this countdown on every hit;
    // tick_shields() spends it before refilling the shield.
    blackboard.set<float>("ship.shield_regen_delay", config.shop.shield_regen_delay);

    // v2 Phase 4: hit-feedback tunables the producer systems read (mirrors how
    // player.invuln_window is pushed to the blackboard above). Shake tunables
    // stay on `config` — only main.cpp's shake loop uses them.
    blackboard.set<float>("fb.trauma_player_hit", config.feedback.trauma_player_hit);
    blackboard.set<float>("fb.trauma_enemy_death", config.feedback.trauma_enemy_death);
    blackboard.set<float>("fb.flash_duration", config.feedback.flash_duration);
    blackboard.set<int>("fb.player_flash_r", config.feedback.player_flash_r);
    blackboard.set<int>("fb.player_flash_g", config.feedback.player_flash_g);
    blackboard.set<int>("fb.player_flash_b", config.feedback.player_flash_b);
    blackboard.set<int>("fb.enemy_flash_r", config.feedback.enemy_flash_r);
    blackboard.set<int>("fb.enemy_flash_g", config.feedback.enemy_flash_g);
    blackboard.set<int>("fb.enemy_flash_b", config.feedback.enemy_flash_b);

    // Quadtree world bounds cover the whole arena circle + margin. (Spawns ring
    // the player now, always clamped inside arena.radius, so the radius bounds
    // everything.)
    const float margin = 80.0f;
    const float world_x = config.arena.center_x - config.arena.radius - margin;
    const float world_y = config.arena.center_y - config.arena.radius - margin;
    const float world_wh = 2.0f * (config.arena.radius + margin);
    QuadtreeStrategy quadtree(6, 8, world_x, world_y, world_wh, world_wh);

    // === Systems ===
    InputSystem input_system;
    PlayerControlSystem player_control(config.player.move_speed);
    PlayerAimSystem player_aim;
    PlayerFireSystem player_fire(config.seed);
    EnemySeekSystem enemy_seek;
    enemy_seek.set_repath_interval(config.pathfinding.repath_interval);
    // v2 Phase 7: grid resolution covering the whole arena circle (world origin
    // bottom-left), rebuilt per arena swap below so A* sees that arena's walls.
    const int path_cell = std::max(1, config.pathfinding.cell_size);
    const int path_cols = static_cast<int>(std::ceil(
        (config.arena.center_x + config.arena.radius) / path_cell)) + 1;
    const int path_rows = static_cast<int>(std::ceil(
        (config.arena.center_y + config.arena.radius) / path_cell)) + 1;
    WaveSpawnerSystem wave_spawner;
    wave_spawner.set_config(&config);
    MovementSystem movement;
    CollisionSystem collision(quadtree);
    ProjectileHitSystem projectile_hit;
    projectile_hit.set_arena(&config.arena);   // D98: the ring ricochets bounce off
    PlayerDamageSystem player_damage;
    DamageApplySystem damage_apply;
    EnemyDeathSystem enemy_death;
    enemy_death.set_economy(config.economy, config.seed);
    PickupSystem pickups;
    pickups.set_economy(config.economy);
    ShopSystem shop;
    shop.set_config(&config.shop);
    LifetimeSystem lifetime;
    AnimationSystem animation;
    ParticleSystem particles;  // v2: global 2000-particle budget
    CameraSystem camera;

    auto resource_manager_ptr =
        std::make_unique<ResourceManager>(renderer.get(), project_paths::assets_dir());
    ResourceManager& resource_manager = *resource_manager_ptr;
    RenderSystem render_system(renderer.get(), resource_manager);
    HUDSystem hud_system(renderer.get(), resource_manager, win_w, win_h);
    // v3 Tier 1: render-target bloom. Constructed after the renderer at the
    // logical surface size; self-disables (begin/resolve become no-ops) when
    // the driver has no render-target support or GameData turns it off.
    BloomSystem bloom_system(renderer.get(), win_w, win_h, config.bloom);
    // v3 Tier 4 (D210): SPIR-V post-process, GPU renderer only. Self-disables
    // everywhere else (classic renderer, headless, missing .spv).
    PostFxSystem postfx(renderer.get(),
                        project_paths::assets_dir() + "/shaders/postfx.frag.spv",
                        config.postfx);

    // === UI & menu layer (Option-040 port) ===
    // ScreenStackSystem is the single writer of the stack and of every
    // UIScreen::active flag; UISystem does hit-testing and publishes confirmed
    // clicks; UIRenderSystem draws the widgets over the HUD.
    //
    // LuaManager exists only to satisfy UISystem's constructor: this game keeps
    // no Lua menu layer, and reads its own button clicks off the Blackboard via
    // UISystem::UI_CLICK_KEY instead. An empty Lua state resolves every
    // on_click_fn to nil, which UISystem treats as "no callback" — so the Lua
    // path is inert rather than broken.
    LuaManager lua_manager;
    UISystem ui_system(lua_manager, win_h);
    UIRenderSystem ui_render_system(renderer.get(), resource_manager, win_w, win_h);
    ScreenStackSystem screen_stack;
    screen_stack.initialize(blackboard, component_storage);

    std::string log_dir = create_log_directory(opts.clear_logs);
    ScreenshotSystem screenshot_system(renderer.get(), log_dir);

    GameHUDSystem game_hud;
    FlashSystem flash_system;

    // v2 Phase 4: seeded RNG for the screen-shake direction. Seeded from the run
    // seed and only advanced when there is trauma to render, so replay stays
    // deterministic (same seed + same hit sequence -> same shake).
    std::mt19937 shake_rng(config.seed);
    std::uniform_real_distribution<float> shake_angle(0.0f, 6.28318530718f);

    // Load the player sprite once (optional; falls back to a Color rectangle).
    // Lane F: a lambda rather than a one-off, because a ship can carry its own
    // sidecar — start_run reloads it after overlaying the chosen ship, so picking
    // a hull with different art does not silently keep the previous sprite.
    std::optional<sidecar_loader::LoadedSprite> player_sprite;
    auto load_player_sprite = [&]() {
        player_sprite.reset();
        if (config.player.sidecar.empty()) return;
        try {
            player_sprite = sidecar_loader::load(
                project_paths::assets_dir() + "/" + config.player.sidecar, config.player.idle_clip);
        } catch (...) { player_sprite.reset(); }
    };
    load_player_sprite();

    // v2 Phase 6: themed-arena state. arena_props holds the current arena's
    // obstacle + hazard entities so they can be swept on an arena swap;
    // active_backdrop points at the parallax layers the render step tiles.
    std::vector<Entity> arena_props;
    // v2 Phase 5c: one emitter entity that renders whatever passive item is
    // equipped. A single reconfigured emitter rather than four per-item props —
    // the items differ only in colour, radius and rate, which is data.
    Entity item_aura = 0;
    // D133/D134: the upgrade kit's overlay entities and the shield field ring.
    // Followers, not components on the player: an entity draws ONE sprite, and
    // the player's is the chassis. Created by spawn_world beside the item aura,
    // parked (zero size) until the matching upgrade is bought — the same "pool
    // it once, park it when idle" shape the minimap blips use (D58).
    Entity kit_parts[upgrade_visuals::KIT_COUNT] = {0};
    Entity shield_field = 0;
    float field_phase = 0.0f;      // free-running 0..1 loop for the hum/bloom
    // Playtest #11/#12: the dash button's FACE — a booster glyph and a circular
    // cooldown dial, parked over the authored "hud_dash_frame" rect. Sprites and
    // not widgets because UIElement has no texture path and adding one is an
    // engine change; same "pool it once, park it when idle" shape as the kit.
    Entity dash_icon = 0, dash_dial = 0;
    // Engine suite (D144): the force-field layer outlives a single hook because
    // other lanes REGISTER into it (surges, actives) and only the `forces` hook
    // consumes. Declared here with the other long-lived systems; cleared at
    // start_run so a well can never outlive its run.
    ForceFieldSystem forces;
    forces.set_capacity(config.forces.max_sources);

    // Engine suite (D146): the destructible-arena bookkeeping. Declared out here
    // because the arena-apply path seeds it and the `crumble` hook consumes it.
    CrumbleSystem crumble;

    // Engine suite (D149): arena weather. Out here because an arena shift has to
    // be able to clear it — a coolant flood must not outlive the arena it flooded.
    SurgeSystem surges;
    surges.set_config(&config);

    int active_arena = -1;
    // Hazards, tracked separately from arena_props so the glow tick has a list to
    // walk without a HazardTag component (which would mean a storage map, two
    // get_storage specialisations and six instantiations — see ENGINE.md).
    std::vector<Entity> hazard_props;
    const std::vector<BackdropLayer>* active_backdrop = &config.arena.backdrop_layers;

    // v2 Phase 5b: the arena shift is a crossfade, not an instant teleport. It
    // fires on a *cleared* wave and runs entirely inside the 2.0s wave.delay the
    // spawner already waits out, so it needs no new phase and no pause state.
    // While `outgoing_backdrop` is set, the render step tiles the old arena at
    // full opacity with the new one fading in over it.
    const std::vector<BackdropLayer>* outgoing_backdrop = nullptr;
    float shift_timer = 0.0f;      // seconds into the current shift
    int   shift_pending = -1;      // arena whose props are still to be swapped in
    // 5s, up from 1.2s. At 1.2s the crossfade read as a hard cut with a stutter in
    // it — long enough to notice, too short to look deliberate. The prompt now
    // holds the run between waves anyway, so there is room to let this breathe.
    constexpr float SHIFT_SECONDS = 5.0f;
    // Props swap at 40%, not the midpoint: the incoming backdrop's alpha ramp is
    // eased (see SHIFT_EASE below), so the busiest part of the fade sits later.
    // Swapping before that keeps the geometry change under the brightest moment.
    constexpr float SHIFT_PROP_SWAP = 2.0f;
    constexpr float TIE_DYE_CYCLE_SECONDS = 3.0f;  // one full hue rotation per enemy

    // Lane E (D77): props mid-animation. The mechanics live in arena_vfx.hpp so
    // they can be unit-tested without a window; main only owns the timing.
    std::vector<arena_vfx::AnimProp> dying_props;    // shrinking out, colliders gone
    std::vector<arena_vfx::AnimProp> growing_props;  // scaling in with the new arena
    // Fractions of the shift window. Outgoing props crumble across the whole of
    // it; incoming ones arrive over the ~3s that is left after SHIFT_PROP_SWAP.
    constexpr float DEATH_STAGGER = 0.55f;
    constexpr float BIRTH_STAGGER = 0.55f;
    constexpr float BIRTH_SECONDS = SHIFT_SECONDS - SHIFT_PROP_SWAP;

    auto clear_arena_props = [&]() {
        for (Entity e : arena_props) component_storage.add_component<DestroyRequest>(e, DestroyRequest{});
        destroy_marked_entities(entity_manager, component_storage);
        arena_props.clear();
        hazard_props.clear();
    };

    // Lane E (D77). The outgoing arena's props stop being *objects* on the frame
    // the shift starts and stop being *pixels* five seconds later. Ordering is
    // the whole point: the collider comes off first and unconditionally, so a
    // pillar that is still visibly crumbling can never block a shot or a dash.
    // Everything after that line is decoration.
    auto begin_prop_teardown = [&](const ArenaDef& outgoing) {
        auto batch = arena_vfx::teardown_props(component_storage, arena_props,
                                               outgoing.enemy_r, outgoing.enemy_g,
                                               outgoing.enemy_b, SHIFT_SECONDS);
        dying_props.insert(dying_props.end(), batch.begin(), batch.end());
        // The props are no longer the live arena's — the shift tick owns them now.
        arena_props.clear();
        hazard_props.clear();
    };

    auto spawn_arena_props = [&](const ArenaDef& def) {
        // v2 Upgrade Phase 3: visible boundary ring — segments evenly spaced on the
        // arena circle, 110 units wide on 90-unit spacing so they overlap into a
        // continuous wall (~97 segments at radius 1400).
        // ponytail: decorative only — the arena clamp in the game loop is the real wall.
        if (!def.wall_image.empty()) {
            const float seg = 110.0f;   // drawn size; spaced tighter so the ring reads solid
            const int count = std::max(24, static_cast<int>(2.0f * 3.14159265f *
                                                            config.arena.radius / 90.0f));
            for (int i = 0; i < count; ++i) {
                const float a = 2.0f * 3.14159265f * static_cast<float>(i) /
                                static_cast<float>(count);
                Entity e = entity_manager.create_entity();
                component_storage.add_component<Position>(e, Position{
                    config.arena.center_x + std::cos(a) * config.arena.radius - seg * 0.5f,
                    config.arena.center_y + std::sin(a) * config.arena.radius - seg * 0.5f});
                component_storage.add_component<Size>(e, Size{seg, seg});
                component_storage.add_component<Color>(e, Color{60, 150, 190, 255});
                component_storage.add_component<Images>(e, Images{{def.wall_image}, 0});
                // D136: wall art is directional (outer plating at the image top,
                // lit inner face at the bottom), so face each segment's top
                // outward along the ring. Cosmetic — walls have no collider.
                component_storage.add_component<Rotation>(e,
                    Rotation{a - 3.14159265f * 0.5f, 0.0f, false});
                component_storage.add_component<RenderLayer>(e, RenderLayer{2});
                arena_props.push_back(e);
            }
        }
        for (const auto& o : def.obstacles) {
            Entity e = entity_manager.create_entity();
            component_storage.add_component<Position>(e, Position{o.x, o.y});
            component_storage.add_component<Size>(e, Size{o.w, o.h});
            component_storage.add_component<Color>(e, Color{70, 96, 128, 255});
            component_storage.add_component<Collider>(e,
                Collider{o.w, o.h, layers::OBSTACLE, layers::OBSTACLE_MASK});
            // Engine suite (D146): hp > 0 makes this pillar destructible. Health
            // is what ProjectileHitSystem tests for, so `hp: 0` (the shipped
            // default) leaves the obstacle exactly as indestructible as before.
            if (o.hp > 0.0f)
                component_storage.add_component<Health>(e, Health{o.hp, o.hp});
            // Images beats Color in render priority; Color stays as the load fallback.
            if (!def.obstacle_image.empty())
                component_storage.add_component<Images>(e, Images{{def.obstacle_image}, 0});
            component_storage.add_component<RenderLayer>(e, RenderLayer{2});
            arena_props.push_back(e);
        }
        for (const auto& h : def.hazards) {
            Entity e = entity_manager.create_entity();
            component_storage.add_component<Position>(e, Position{h.x, h.y});
            component_storage.add_component<Size>(e, Size{h.w, h.h});
            component_storage.add_component<Color>(e, Color{255, 40, 30, 220});
            component_storage.add_component<Collider>(e,
                Collider{h.w, h.h, layers::HAZARD, layers::HAZARD_MASK});
            // Score/xp 0: hazards only bleed the drone, they never "die" or reward.
            component_storage.add_component<ContactDamage>(e, ContactDamage{h.damage, 0, 0});
            if (!def.hazard_image.empty())
                component_storage.add_component<Images>(e, Images{{def.hazard_image}, 0});
            // Hazards are ALWAYS red, in every arena. The vent art is drawn in the
            // arena's own palette, which made a Bio-lab hazard read as scenery; a
            // saturated additive red overrides that, so "red glow == it hurts" is
            // one rule the player learns once and never has to relearn.
            component_storage.add_component<Tint>(e, Tint{255, 60, 40, 255, true});
            // Drawn ABOVE obstacles (layer 2), not below them: a hazard hidden
            // behind a pillar is a hazard you walk into.
            component_storage.add_component<RenderLayer>(e, RenderLayer{3});

            // A slow ember plume so a hazard is visible before it is touched.
            // Budget: 9 hazards is the worst arena, at 12/s over a 1.1s lifetime
            // => ~120 live particles held permanently, against the global 2000.
            // Deliberately modest — see ENGINE.md §5 on the wave-20 cap.
            ParticleEmitter vent;
            // Line, spanning the vent's width: embers rise off the whole grate
            // rather than from one point. line_dx/dy is the segment vector from the
            // emitter position, so the offset starts it at the left-centre.
            vent.shape = EmitterShape::Line;
            vent.line_dx = h.w * 0.8f;
            vent.line_dy = 0.0f;
            vent.offset_x = h.w * 0.1f;
            vent.offset_y = h.h * 0.5f;
            vent.additive = true;
            vent.emission_rate = 12.0f;
            vent.particle_lifetime = 1.1f;
            vent.min_speed = 16.0f; vent.max_speed = 46.0f;
            vent.direction = 90.0f; vent.cone_half_angle = 38.0f;
            vent.start_size = 7.0f; vent.end_size = 0.0f;
            vent.start_r = 255; vent.start_g = 150; vent.start_b = 60; vent.start_a = 205;
            vent.end_r = 190; vent.end_g = 30; vent.end_b = 20; vent.end_a = 0;
            component_storage.add_component<ParticleEmitter>(e, vent);

            arena_props.push_back(e);
            hazard_props.push_back(e);
        }
    };

    // Everything an arena swap touches *except* the backdrop pointer, which the
    // crossfade owns separately. Props, the obstacle set the push-out reads
    // (via active_arena), the A* grid and the enemy tint all move together, so
    // there is never a frame where they disagree about which arena is live.
    auto apply_arena_props = [&](int want) {
        const ArenaDef& def = config.arenas[static_cast<size_t>(want)];
        clear_arena_props();
        spawn_arena_props(def);
        active_arena = want;
        // v2 Phase 5a: enemies spawned from here on wear this arena's colour.
        // Enemies already alive keep theirs — the tint is captured at spawn.
        wave_spawner.set_enemy_tint(def.enemy_r, def.enemy_g, def.enemy_b);
        // v2 Phase 7: rasterise this arena's obstacles into the A* grid.
        // Engine suite (D146): the grid is built from CrumbleSystem's LIVE list
        // rather than straight from the config rows, so a destroyed pillar can
        // reopen the path. set_arena() re-seeds that list from the config on every
        // apply — each arena arrives whole, so a run cannot permanently strip the
        // map, and a shift back to a wrecked theme is a fresh one.
        crumble.set_arena(def.obstacles);
        enemy_seek.set_arena(&crumble.live_obstacles(),
            enemy_path::build_obstacle_grid(path_cols, path_rows, path_cell,
                crumble.live_obstacles(), config.pathfinding.clearance));
    };

    // Lane E (D78): the shift-start was inlined in the cleared-wave branch, which
    // made it unreachable from anywhere else. It is a lambda now with no
    // dependence on `wave_cleared` or the spawner, so any caller — including the
    // wave-50 boss that shifts the arena mid-fight — can fire one by index.
    // Returns false if `want` is already live or out of range.
    auto begin_arena_shift = [&](int want) {
        if (want < 0 || want >= static_cast<int>(config.arenas.size())) return false;
        if (want == active_arena || active_arena < 0) return false;
        const ArenaDef& def = config.arenas[static_cast<size_t>(want)];
        // Strip the outgoing colliders on THIS frame, before anything else. The
        // props stay on screen for the whole crossfade, so from here on they are
        // scenery the simulation cannot see.
        begin_prop_teardown(config.arenas[static_cast<size_t>(active_arena)]);
        outgoing_backdrop = active_backdrop;
        active_backdrop = &def.backdrop_layers;
        shift_timer = 0.0f;
        shift_pending = want;
        // #13: the player named it. "arena shift" was engine vocabulary.
        blackboard.set<std::string>("hud_message", def.name + " — REACTOR SHIFT");
        blackboard.set<float>("hud_message_timer", SHIFT_SECONDS + 1.4f);
        // v3 Tier 4 (D210): the shift also ripples the whole frame.
        postfx.trigger_shock(0.5f, 0.5f);

        // Shockwave: a one-shot emitter host, the same pattern
        // EnemyDeathSystem and PickupSystem already use. Sited on
        // the drone rather than the arena centre — the camera
        // follows the drone, and at radius 1400 a centre burst is
        // usually off-screen. Measured budget: a shift only fires
        // on a *cleared* wave, where the live particle count is
        // ~13 of 2000, so this ~250-particle burst has room.
        for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
            auto pp = component_storage.get_component<Position>(p);
            auto ps = component_storage.get_component<Size>(p);
            if (!pp.has_value() || !ps.has_value()) break;
            Entity burst = entity_manager.create_entity();
            component_storage.add_component<Position>(burst, Position{
                pp->get().x + ps->get().width * 0.5f,
                pp->get().y + ps->get().height * 0.5f});
            ParticleEmitter ring;
            ring.shape = EmitterShape::Point;
            ring.additive = true;
            ring.emission_rate = 900.0f;
            ring.particle_lifetime = 0.9f;
            ring.min_speed = 280.0f; ring.max_speed = 620.0f;
            ring.cone_half_angle = 180.0f;
            ring.start_size = 7.0f; ring.end_size = 0.0f;
            ring.start_r = def.enemy_r; ring.start_g = def.enemy_g;
            ring.start_b = def.enemy_b; ring.start_a = 220;
            ring.end_r = 255; ring.end_g = 255; ring.end_b = 255;
            ring.end_a = 0;
            component_storage.add_component<ParticleEmitter>(burst, ring);
            component_storage.add_component<Lifetime>(burst, Lifetime{0.28f});
            break;
        }
        blackboard.set<float>("feedback.trauma",
            feedback::add_trauma(blackboard.get_or<float>("feedback.trauma", 0.0f),
                                 config.feedback.trauma_enemy_death));
        // v3 Tier 3 (D209): a kill freezes sim time for a beat. Saturating,
        // not additive — a multi-kill frame reads as one hit, not a slideshow.
        hitstop_left = std::max(hitstop_left, config.feedback.hitstop_frames_kill);
        return true;
    };

    // The Aux Thruster scales move speed, and a purchase must take effect on the
    // very next frame, so the multiplier is pushed in rather than cached. Shared
    // by PHASE_PLAYING and PHASE_INTERMISSION, which both fly the drone.
    // Hazard glow. A static red already says "dangerous"; a slow throb says
    // "actively dangerous", which is what stops a player treating a vent as
    // decoration. Same idiom as tick_enemy_tint: a plain loop over a list, driven
    // by delta_time, writing only Tint — nothing the simulation reads back.
    float hazard_glow_t = 0.0f;
    auto tick_hazard_glow = [&](float dt) {
        constexpr float HAZARD_PULSE_HZ = 0.9f;
        hazard_glow_t += dt * HAZARD_PULSE_HZ;
        hazard_glow_t -= std::floor(hazard_glow_t);
        // 0.62..1.0 — never dim enough to lose the shape, never flat.
        const float k = 0.81f + 0.19f * std::cos(hazard_glow_t * 6.2831853f);
        const auto ch = [&](float v) {
            return static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, v * k)));
        };
        for (Entity e : hazard_props) {
            if (auto t = component_storage.get_component<Tint>(e); t.has_value()) {
                t->get().r = ch(255.0f); t->get().g = ch(60.0f); t->get().b = ch(40.0f);
            }
        }
    };

    auto apply_move_speed = [&]() {
        for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
            if (auto st = component_storage.get_component<ShipState>(p); st.has_value())
                player_control.set_speed(config.player.move_speed * st->get().speed_mult);
            break;
        }
    };

    // Position fix-ups applied after any movement, in this order: the arena ring
    // first, then solid obstacles, so "can't pass the wall" always gets the last
    // word on where an entity ends up.
    //
    // Hoisted out of the PHASE_PLAYING block because PHASE_INTERMISSION moves the
    // drone too — the player walks around collecting loot while the prompt is up,
    // and un-clamped movement there would let them leave the arena entirely.
    auto clamp_to_arena = [&](Entity p) {
        auto pos = component_storage.get_component<Position>(p);
        auto sz = component_storage.get_component<Size>(p);
        if (!pos.has_value() || !sz.has_value()) return;
        float cx = pos->get().x + sz->get().width * 0.5f;
        float cy = pos->get().y + sz->get().height * 0.5f;
        float dx = cx - config.arena.center_x, dy = cy - config.arena.center_y;
        float d = std::sqrt(dx * dx + dy * dy);
        float limit = config.arena.radius - sz->get().width * 0.5f;
        if (d > limit && d > 0.0001f) {
            cx = config.arena.center_x + dx / d * limit;
            cy = config.arena.center_y + dy / d * limit;
            pos->get().x = cx - sz->get().width * 0.5f;
            pos->get().y = cy - sz->get().height * 0.5f;
        }
    };
    auto push_out_of_solids = [&](Entity p) {
        if (active_arena < 0) return;
        const auto& obs = config.arenas[static_cast<size_t>(active_arena)].obstacles;
        auto pos = component_storage.get_component<Position>(p);
        auto sz = component_storage.get_component<Size>(p);
        if (!pos.has_value() || !sz.has_value()) return;
        float r = sz->get().width * 0.5f;
        float cx = pos->get().x + r, cy = pos->get().y + r;
        for (const auto& o : obs) {
            Vec2 c = push_circle_out_of_aabb(cx, cy, r, o.x, o.y, o.w, o.h);
            cx = c.x; cy = c.y;
        }
        pos->get().x = cx - r;
        pos->get().y = cy - r;
    };

    // spawn_world: tear down everything and build a fresh run (player + HUD).
    auto spawn_world = [&]() {
        shop.close(component_storage);   // drop stale row ids before the teardown
        for (Entity e : entity_manager.all_entities()) {
            // The menu layer is loaded ONCE, from GameData.json, and outlives every
            // run — screens and widgets are not part of the world. Sweeping them
            // here would leave the game with no menus from the first restart on
            // (and, since spawn_world also runs at startup, from frame zero).
            if (component_storage.has_component<UIScreen>(e) ||
                component_storage.has_component<UIElement>(e)) {
                continue;
            }
            component_storage.add_component<DestroyRequest>(e, DestroyRequest{});
        }
        destroy_marked_entities(entity_manager, component_storage);

        blackboard.set("score", 0);
        blackboard.set<int>("sim.kills", 0);   // engine suite D139: kill-chain source
        blackboard.set<bool>("flight_report.reset", true);  // D143: drop last run's path
        forces.clear();                        // D144: no field outlives its run
        surges.clear(component_storage, entity_manager);   // D149: nor any weather
        surges.set_seed(config.seed);          // its own stream, seeded per run
        blackboard.set("wave", 0);
        blackboard.set<float>("player.iframes", 0.0f);
        blackboard.set<float>("hud_message_timer", 0.0f);
        blackboard.set<std::string>("hud_message", std::string());
        blackboard.set("all_waves_complete", false);
        // D3: shop upgrades are per-run. Every other purchase lands on components
        // that spawn_world rebuilds anyway; the barrel count is the one that lives
        // on the blackboard, so it is the one that needs clearing by hand.
        blackboard.set<int>("ship.extra_shots", 0);
        blackboard.set<int>("ship.bounces", 0);   // D98: same reason as the barrels
        // Phase 4's equipment publishes the same way (the ids themselves live on
        // the rebuilt ShipState, so only these four need clearing by hand).
        blackboard.set<float>("ship.item_amount", 0.0f);
        blackboard.set<float>("ship.buff_mult", 1.0f);
        blackboard.set<std::string>("ship.item_name", std::string());
        blackboard.set<std::string>("ship.consumable_name", std::string());
        // D192 #9: the battery's two rates, published once per world so
        // PlayerFireSystem stays config-blind (the ship.extra_shots pattern).
        blackboard.set<float>("battery.drain_per_s",
            config.battery.fire_time > 0.0f ? 1.0f / config.battery.fire_time : 0.0f);
        blackboard.set<float>("battery.charge_per_s",
            config.battery.recharge_time > 0.0f ? 1.0f / config.battery.recharge_time : 0.0f);
        wave_spawner.reset();

        // Player entity.
        const float psz = config.player.size;
        Entity player = entity_manager.create_entity();
        component_storage.add_component<Position>(player,
            Position{config.player.start_x - psz * 0.5f, config.player.start_y - psz * 0.5f});
        component_storage.add_component<Velocity>(player, Velocity{0.0f, 0.0f});
        component_storage.add_component<Size>(player, Size{psz, psz});
        // flip_when_left=false: the v2 drone art is symmetric and right-facing, so
        // it takes pure rotation. The default (true) mirrors the sprite past 90°,
        // which reads as a snap that no longer matches the fire direction.
        component_storage.add_component<Rotation>(player, Rotation{0.0f, 0.0f, false});
        component_storage.add_component<Input>(player, Input{});
        component_storage.add_component<Color>(player, Color{90, 200, 160, 255});
        component_storage.add_component<PlayerTag>(player, PlayerTag{});
        component_storage.add_component<Health>(player,
            Health{config.player.start_health, config.player.start_health});
        component_storage.add_component<WeaponStats>(player, WeaponStats{
            config.player.weapon.fire_rate, config.player.weapon.damage,
            config.player.weapon.projectile_speed, config.player.weapon.projectile_lifetime,
            config.player.weapon.spread, 0.0f,
            config.player.weapon.projectile_size, config.player.weapon.pierce});
        // Per-run economy/shop state (D3: no persistence — a fresh one each run).
        // The ship's stats are already baked into `config` by start_run; ship_id
        // just records which hull this run is flying.
        ShipState ship_state{};
        ship_state.ship_id = selected_ship;
        // Gameplay pack (D221): a per-ship starting shield (Gryphon). Baked into
        // config by apply_ship, so a resumed run's overlay still wins below.
        ship_state.shield_max = config.player.start_shield;
        ship_state.shield     = config.player.start_shield;
        // D192 #10: the dash stack starts at the configured size and grows by one
        // per boss killed (BossSystem). A resumed run restores dash_max from the
        // save, so bosses already beaten are not re-paid for.
        ship_state.dash_max = std::max(1, config.dash.charges);
        ship_state.dash_charges = ship_state.dash_max;
        component_storage.add_component<ShipState>(player, ship_state);
        component_storage.add_component<Collider>(player,
            Collider{psz, psz, layers::PLAYER, layers::PLAYER_MASK});
        component_storage.add_component<CircleCollider>(player, CircleCollider{psz * 0.5f, 0.0f, 0.0f});
        component_storage.add_component<RenderLayer>(player, RenderLayer{3});

        // v2: subtle additive aura/thruster emitter riding the player's centre.
        {
            ParticleEmitter thruster;
            thruster.shape = EmitterShape::Point;
            thruster.additive = true;
            thruster.emission_rate = 34.0f;
            thruster.particle_lifetime = 0.4f;
            thruster.min_speed = 5.0f; thruster.max_speed = 30.0f;
            thruster.cone_half_angle = 180.0f;
            thruster.start_r = 90; thruster.start_g = 220; thruster.start_b = 255; thruster.start_a = 170;
            thruster.end_r = 30;   thruster.end_g = 80;    thruster.end_b = 160;   thruster.end_a = 0;
            thruster.start_size = 5.0f; thruster.end_size = 0.0f;
            thruster.offset_x = psz * 0.5f; thruster.offset_y = psz * 0.5f;
            component_storage.add_component<ParticleEmitter>(player, thruster);
        }

        // v2 Phase 5c: the equipped-item aura. Starts inactive; the game loop
        // re-points and reconfigures it from ShipState.item_id each frame. Its own
        // entity because an entity holds at most one ParticleEmitter and the
        // player's is the thruster.
        {
            item_aura = entity_manager.create_entity();
            component_storage.add_component<Position>(item_aura,
                Position{config.player.start_x, config.player.start_y});
            ParticleEmitter aura;
            aura.active = false;
            aura.additive = true;
            component_storage.add_component<ParticleEmitter>(item_aura, aura);
        }

        if (player_sprite.has_value()) {
            component_storage.add_component<SpriteSheet>(player, player_sprite->sprite_sheet);
            component_storage.add_component<Animation>(player, player_sprite->animation);
        }

        // D133: one follower per kit part. Each wears its overlay as an Images,
        // authored in the chassis's own 128-space, so it is drawn at exactly the
        // player's size and rides the same rotation — the part lands in the
        // hardpoint the chassis draws empty.
        for (int i = 0; i < upgrade_visuals::KIT_COUNT; ++i) {
            Entity e = entity_manager.create_entity();
            kit_parts[i] = e;
            component_storage.add_component<Position>(e, Position{0.0f, 0.0f});
            component_storage.add_component<Size>(e, Size{0.0f, 0.0f});   // parked
            component_storage.add_component<Rotation>(e, Rotation{0.0f, 0.0f, false});
            component_storage.add_component<Images>(e,
                Images{{std::string(upgrade_visuals::KIT[i].image)}, 0});
            component_storage.add_component<RenderLayer>(e, RenderLayer{4});  // over the hull
        }

        // D134: the shield field. A SpriteSheet with NO Animation — main.cpp
        // writes current_frame from upgrade_visuals::field_frame every playing
        // frame, because the four states are a loop, a one-shot, a static and a
        // progress bar, and only one of those is a clip.
        {
            shield_field = entity_manager.create_entity();
            component_storage.add_component<Position>(shield_field, Position{0.0f, 0.0f});
            component_storage.add_component<Size>(shield_field, Size{0.0f, 0.0f});  // parked
            // Rotation carries the impact bearing on a hit; the ring itself is a
            // circle, so spinning it is free and only moves the bloom.
            component_storage.add_component<Rotation>(shield_field, Rotation{0.0f, 0.0f, false});
            SpriteSheet ss{};
            ss.atlas_filename = "v2/shield_field.png";
            ss.frame_width = 192; ss.frame_height = 192;
            ss.columns = 7; ss.total_frames = upgrade_visuals::FIELD_TOTAL;
            ss.current_frame = 0;
            component_storage.add_component<SpriteSheet>(shield_field, ss);
            component_storage.add_component<RenderLayer>(shield_field, RenderLayer{5});
        }
        field_phase = 0.0f;

        // #11: the dash button's face. Position is a placeholder — the screen
        // placement is written after camera.update in the render block, because
        // CameraSystem rewrites ScreenPosition for everything that has a Position
        // (and RenderSystem only ever reaches entities that have one).
        {
            dash_icon = entity_manager.create_entity();
            component_storage.add_component<Position>(dash_icon, Position{0.0f, 0.0f});
            component_storage.add_component<Size>(dash_icon, Size{0.0f, 0.0f});  // parked
            component_storage.add_component<Images>(dash_icon,
                Images{{std::string("v2/hud_boost.png")}, 0});
            component_storage.add_component<RenderLayer>(dash_icon, RenderLayer{60});

            dash_dial = entity_manager.create_entity();
            component_storage.add_component<Position>(dash_dial, Position{0.0f, 0.0f});
            component_storage.add_component<Size>(dash_dial, Size{0.0f, 0.0f});   // parked
            SpriteSheet ds{};
            ds.atlas_filename = "v2/hud_dash_sweep.png";
            ds.frame_width = 64; ds.frame_height = 64;
            ds.columns = 4; ds.total_frames = DASH_SWEEP_FRAMES;
            ds.current_frame = 0;
            component_storage.add_component<SpriteSheet>(dash_dial, ds);
            component_storage.add_component<RenderLayer>(dash_dial, RenderLayer{61});
        }

        game_hud.init(component_storage, entity_manager, blackboard);

        // Arena props were destroyed by the bulk teardown above; forget them and
        // force a fresh arena swap on the first playing frame.
        arena_props.clear();
        hazard_props.clear();
        active_arena = -1;
        active_backdrop = &config.arena.backdrop_layers;
        outgoing_backdrop = nullptr;
        shift_timer = 0.0f;
        shift_pending = -1;
        dying_props.clear();     // Lane E (D77): same reason — the entities are gone
        growing_props.clear();
    };

    spawn_world();
    // Task 7: unregistered + online -> the name-entry screen comes before the
    // title. Headless/scripted runs never enter it — net::enabled() is already
    // false whenever --stopframe is set, which is the whole determinism
    // guarantee this rides on (see net::set_enabled() above).
    int phase = (!meta.registered && net::enabled()) ? PHASE_NAME_ENTRY : PHASE_TITLE;
    blackboard.set("phase", phase);
    // Task 7: name-entry screen state. `name_buf` is the live typed buffer;
    // `pending_register` outlives a single frame (declared here, not inside
    // the loop) so the future can be polled across frames without its
    // destructor blocking — see net/http_client.hpp's doc comment.
    std::string name_buf;
    std::string name_entry_error;
    std::future<net::Response> pending_register;
    // Mailing list (specs/mailing-list.md): an optional second field on the same
    // screen. TAB moves `name_entry_email_focus` between the two buffers.
    // `pending_subscribe` is fired once, on a confirmed registration, and never
    // polled — signup failure must not block the player at this screen — but it
    // still lives out here so its destructor can't block the loop (same rule as
    // `pending_register`; abandoned at shutdown alongside it).
    std::string email_buf;
    bool email_focus = false;
    std::future<net::Response> pending_subscribe;
    // Feedback form (specs/feedback-reports.md). One live future, name_entry's
    // plumbing. fb_fields: 0=subject 1=body 2=tags 3=from.
    std::string fb_fields[4];
    int fb_focus = 0;
    bool fb_from_pause = false;      // routes ESC: pop-to-pause vs back-to-title
    int fb_prev_phase = PHASE_TITLE; // restored on exit when fb_from_pause
    std::string fb_msg;              // "" = show the default hint line
    std::future<net::Response> pending_feedback;
    nlohmann::json fb_ctx;           // run context captured AT OPEN, not at send
    // === HOOK: telemetry state === (specs/telemetry.md)
    // The per-run report lives in main's scope, never the ECS (Invariant 6), and
    // is write-only with respect to the sim (Invariant 4). Declared here because
    // bank_run_score below captures it by reference.
    telemetry::RunReport tm;
    const std::string session_id = generate_uuid();       // one per launch
    std::vector<std::future<net::Response>> tm_inflight;  // reaped per frame, drained at exit
    // Consumable-slot watcher for the per-frame sampler: a slot going from held
    // to empty during play is one use. Hoisted here rather than a function-local
    // static so it cannot survive across runs.
    int prev_consumable = -1;
    // items::use_consumable clears ship.consumable_name in the SAME call that
    // sets consumable_id = -1, so reading the name once the slot is already
    // empty yields "". Latch it while the consumable is still held.
    std::string prev_consumable_name;
    // === END HOOK: telemetry state ===

    // Main-menu-suite Phase C: settings — loaded once, published to the two
    // Blackboard flags the apply sites read, checkbox widgets synced at boot.
    // Loaded HERE rather than at the screen-widget block below because
    // bank_run_score's telemetry guard reads settings.analytics.
    SettingsSave settings = settings_load(settings_save_path());
    blackboard.set<bool>("settings.screen_shake", settings.screen_shake);
    blackboard.set<bool>("settings.minimap", settings.minimap);
    // Task 7 review round 1, Critical 1: the name actually POSTed for the
    // in-flight `pending_register`, captured at submit time. `name_buf` is
    // live and can change while the request is in flight (typing is not
    // blocked during a pending submit) — applying `name_buf` on success would
    // silently save whatever the player has typed by the time the response
    // lands, not what the server actually registered. Always apply this.
    std::string pending_name;
    if (phase == PHASE_NAME_ENTRY) SDL_StartTextInput(window.get());

    // Task 8: score-submission future. Same lifetime discipline as
    // `pending_register` above — declared outside the loop, polled with
    // wait_for(0s), never a discarded temporary (net/http_client.hpp). Also
    // shared across runs: abandon_future() whatever is still in flight rather
    // than letting a reassignment's implicit destructor block.
    std::future<net::Response> pending_score;

    // In-game updater. One /version GET at boot; the title button stays ghosted
    // unless the answer is BOTH newer than this build and a URL we are willing
    // to execute (update_check.hpp decides both, and fails closed on either).
    // Same single-future discipline as the futures above.
    std::future<net::Response> pending_version;
    std::future<net::Response> pending_download;
    std::string update_version, update_url, update_file;
    bool update_ready = false;    // a newer, trusted build exists
    bool update_busy = false;     // installer download in flight
    std::string update_note;      // one line of status under the button
    Entity update_w = 0;
    bool update_w_resolved = false;
    if (net::enabled())
        pending_version = net::get(std::string(net::NET_BASE) + "/version");

    // Task 9: leaderboard fetch. Same single-future discipline as
    // pending_register/pending_score above (net/http_client.hpp): declared
    // outside the loop, polled with wait_for(0s), never a discarded temporary.
    // Stale-tab guard: `fetch_leaderboard` below is the ONLY place `pending_top`
    // is assigned, and it always abandons whatever was already in flight
    // (abandon_future) and sets `lb_mode` in the SAME edge as issuing the new
    // request. So there is only ever one live future at a time, and it always
    // targets whatever `lb_mode` currently is — a late response for a mode the
    // player has since switched away from was abandoned before its replacement
    // request even went out, so it can never be applied to the new tab (the
    // exact bug shape that hit Task 7 Critical 1).
    std::future<net::Response> pending_top;
    std::string lb_mode = "high";      // "high" | "total"
    std::string lb_status = "loading"; // "loading" | "ok" | "error"
    std::string lb_rows;               // pre-formatted "1. name  1234\n" lines
    auto fetch_leaderboard = [&](const std::string& mode) {
        abandon_future(std::move(pending_top));
        lb_mode = mode;
        lb_status = "loading";
        lb_rows.clear();
        blackboard.set<std::string>("lb_mode", lb_mode);
        blackboard.set<std::string>("lb_status", lb_status);
        blackboard.set<std::string>("lb_rows", lb_rows);
        pending_top = net::get(std::string(net::NET_BASE) + "/top?mode=" + mode);
    };

    // Phase B (D50): start a run at a chosen difficulty. Re-copying base_config
    // makes the choice re-selectable across runs — scaling `config` in place
    // would compound Hard onto Hard. set_config re-seeds the spawn RNG, so two
    // runs of the same --seed and difficulty stay identical.
    // Lane F (D80): this run's score is added to the lifetime total exactly once,
    // at run end — death, victory, or quitting out of the pause menu. `banked`
    // guards the double call (game over, then quit from the same screen).
    bool run_banked = true;   // no run in progress at the title
    bool run_stats_up = false;   // tier 6 (D221): the flight report is on the stack
    auto bank_run_score = [&](const Blackboard& bb, const char* outcome) {
        if (run_banked) return;
        run_banked = true;
        const int run_score = bb.get_or<int>("score", 0);
        meta.lifetime_score += run_score;
        // Gameplay pack (D221): bank scrap on this same exactly-once edge. A
        // victory banks every wave; otherwise the current 0-based index IS the
        // number of waves fully cleared. Published for the run-stats screen.
        {
            const bool victory = std::string(outcome) == "victory";
            const int cleared = victory ? config.victory_wave
                                        : wave_spawner.current_wave_index();
            const int earned = scrap_for_run(cleared, victory, config.scrap,
                                             10, config.victory_wave);
            meta.scrap += earned;
            blackboard.set<int>("run.scrap", earned);
        }
        // Main-menu-suite Phase C: the records screen's numbers, banked in the
        // same breath as the score so they can never disagree with it.
        meta.runs_played += 1;
        meta.best_wave = std::max(meta.best_wave,
                                  wave_spawner.current_wave_index() + 1);
        meta_write(meta_save_path(), meta);
        // Task 8: submit to the global leaderboard on THIS transition edge only —
        // the `run_banked` guard above is what makes this exactly-once despite
        // five call sites (quit, to-menu, death, victory, shutdown). Headless/
        // scripted runs have net::enabled() == false (architecture.md Invariant
        // 4) so they bank locally above and make zero network calls here. An
        // unregistered player (ESC-skipped name entry) also just banks locally —
        // no status text, nothing that implies a failure.
        if (net::enabled() && meta.registered) {
            // A fast retry could still have the previous run's submit in
            // flight; reassigning `pending_score` below would otherwise run
            // its destructor synchronously (net/http_client.hpp).
            abandon_future(std::move(pending_score));
            blackboard.set<std::string>("score_submit_status",
                                        std::string("Submitting score..."));
            pending_score = net::post_json(
                std::string(net::NET_BASE) + "/score",
                nlohmann::json{{"player_id", meta.player_id},
                              {"score", run_score}}.dump(),
                net::NET_GAME_KEY);
        }

        // Telemetry: finalize and POST the per-run summary from this same
        // exactly-once edge (specs/telemetry.md). Consent is checked here and
        // nowhere else; net::enabled() is false under --stopframe, so headless
        // and scripted runs collect but never send.
        //
        // NOTE: /score is deliberately NOT re-sent here. The plan's Task 5
        // snippet adds one, but it was written before /score was wired (commit
        // 4fb7e3a) — sending it again would double-count every run. /score also
        // stays gated on meta.registered rather than settings.analytics: the
        // leaderboard is a separate opt-in (registering a pilot name), so
        // switching analytics off must not silently stop banking scores.
        tm.outcome = outcome;
        tm.wave = wave_spawner.current_wave_index() + 1;
        tm.score = run_score;
        tm.shots  = static_cast<long long>(bb.get_or<double>("tm.shots", 0.0));
        tm.hits   = static_cast<long long>(bb.get_or<double>("tm.hits", 0.0));
        tm.dashes = static_cast<long long>(bb.get_or<double>("tm.dashes", 0.0));
        tm.bombs  = static_cast<long long>(bb.get_or<double>("tm.bombs", 0.0));
        tm.item_equipped = bb.get_or<std::string>("ship.item_name", std::string());
        for (Entity p : component_storage.entities_with_component<PlayerTag>())
            if (auto s = component_storage.get_component<ShipState>(p); s.has_value())
                for (int i = 0; i < 8; ++i) tm.upg_counts[i] = s->get().upg_counts[i];
        tm.ui["minimap_on"] = settings.minimap ? 1 : 0;
        tm.ui["shake_on"] = settings.screen_shake ? 1 : 0;
        if (net::enabled() && settings.analytics) {
            tm_inflight.push_back(net::post_json(std::string(net::NET_BASE) + "/telemetry",
                                                 telemetry::serialize(tm), net::NET_GAME_KEY));
        }
    };

    // Lane K (D100): `resume` is the ONE difference between starting a run and
    // continuing one. Everything else — the pristine base_config re-copy, the
    // ship overlay, apply_difficulty, set_config, spawn_world — is shared, so a
    // resumed run cannot drift from a fresh one, and `apply_difficulty` keeps its
    // single non-idempotent application site (D50).
    // Lane K (D100): a run that ENDED cannot be continued, so its save goes.
    // Deliberately not routed through bank_run_score, which also fires on quit
    // and on closing the window — a player who just pressed SAVE and quit must
    // still find their run there.
    auto end_saved_run = [&]() {
        run_save_clear(run_save_path(active_slot + 1));
        saved_slots[active_slot] = RunSave{};
    };

    auto start_run = [&](size_t difficulty_index, const RunSave* resume = nullptr) {
        config = base_config;
        if (resume != nullptr && resume->present) {
            config.seed = resume->seed;
            if (resume->ship_id >= 0 && resume->ship_id < static_cast<int>(config.ships.size()))
                selected_ship = resume->ship_id;
        }
        // Gameplay pack (D221 call #6): shuffle the arena rotation for THIS run,
        // seeded from the run seed (a distinct stream constant, the surges
        // pattern) so replays and resumes reproduce the same order. This is the
        // ONE deliberate canary-line change of the pack — re-baselined here.
        {
            std::mt19937 arena_rng(config.seed * 2654435761u + 97u);
            shuffle_arena_order(config.arenas, arena_rng);
            std::cout << "Arena order:";
            for (const ArenaDef& a : config.arenas) std::cout << " " << a.name << ";";
            std::cout << "\n";
        }
        // Lane F (D82): the ship overlay lands here, in the one place the pristine
        // base_config is re-copied — apply_ship is no more idempotent than
        // apply_difficulty, so there must never be a second application site.
        if (selected_ship >= 0 && selected_ship < static_cast<int>(config.ships.size())) {
            const ShipDef& sd = config.ships[static_cast<size_t>(selected_ship)];
            apply_ship(config.player, sd);
            // Gameplay pack (D221): per-ship dash reach — distance is
            // speed*duration, so scaling speed here (on the fresh copy, D50
            // discipline) changes reach without touching any duration rule.
            config.dash.speed *= sd.dash_mult;
            load_player_sprite();
            // Gameplay pack (D221): the equipped weapon overlays after the ship.
            // Falls back to the ship's default when nothing valid is equipped;
            // the weapon's own colour supersedes D184's ship-complement rule.
            // Gameplay pack (D221): the ship's special attribute, published for
            // the systems that consume it (fire gate, veil, ram dash), plus the
            // per-run seeds it owns. Seeding active_cd_mult here also stops a
            // boss-repick discount leaking into the NEXT run.
            blackboard.set<std::string>("ship.special", sd.special);
            blackboard.set<float>("ship.active_cd_mult",
                                  sd.special == "equip_cd" ? 0.75f : 1.0f);
            blackboard.set<bool>("veil.armed", true);
            blackboard.set<float>("ship.no_fire", 0.0f);
            // A resumed run keeps ITS weapon, not whatever is equipped now.
            std::string wname = (resume != nullptr && resume->present && !resume->weapon.empty())
                                    ? resume->weapon
                                    : meta.equipped_weapon;
            if (wname.empty() || find_weapon(config.weapons, wname) < 0
                || (!(resume != nullptr && resume->present)
                    && !weapon_owned(meta, config.ships, wname)))
                wname = sd.default_weapon;
            const int wi = find_weapon(config.weapons, wname);
            if (wi >= 0) {
                const WeaponDef& wd = config.weapons[static_cast<size_t>(wi)];
                apply_weapon(config.player, config.battery, wd);
                blackboard.set<std::string>("weapon.name", wd.name);
                blackboard.set<std::string>("weapon.secondary", wd.secondary);
                blackboard.set<float>("weapon.secondary_cd_max", wd.secondary_cd);
                blackboard.set<int>("ship.shot_r", wd.color_r);
                blackboard.set<int>("ship.shot_g", wd.color_g);
                blackboard.set<int>("ship.shot_b", wd.color_b);
            } else {
                // No weapons catalogue authored — D184's complement rule stands.
                blackboard.set<int>("ship.shot_r", 255 - sd.color_r);
                blackboard.set<int>("ship.shot_g", 255 - sd.color_g);
                blackboard.set<int>("ship.shot_b", 255 - sd.color_b);
            }
        }
        // === HOOK: prestige === (Iteration 5, D126 — Lane O / #14)
        // The base-stat buff rides the SAME site as the ship overlay and
        // apply_difficulty: `config` is a fresh copy of the pristine base_config
        // two lines up, so this is applied exactly once per run and never
        // compounds (D50). Upgrades are stripped for free — they live on the
        // per-run ShipState that spawn_world() rebuilds below.
        apply_prestige(config.player, meta.prestige);
        blackboard.set<double>(PRESTIGE_LEVEL_KEY, static_cast<double>(meta.prestige));
        // Determinism: this is persistent state that DOES reach the sim, so a
        // replay is reproducible at a fixed level. The level is printed rather
        // than assumed, on its own line so the canary's comparison target is a
        // superset rather than an edited line.
        std::cout << "Prestige: " << meta.prestige << "\n";
        // === END HOOK: prestige ===
        run_banked = false;
        // Telemetry: a fresh report per run. The combat counters are published
        // by the systems onto the Blackboard, so they reset here too.
        tm = telemetry::RunReport{};
        tm.game_version = GAME_VERSION;
        tm.player_id = meta.player_id;
        tm.session_id = session_id;
        tm.seed = config.seed;
        tm.ship = selected_ship;
        tm.prestige = meta.prestige;
        tm.resumed = (resume != nullptr && resume->present);
        prev_consumable = -1;
        for (const char* k : {"tm.shots", "tm.hits", "tm.dashes", "tm.bombs"})
            blackboard.set<double>(k, 0.0);
        blackboard.set<std::string>("tm.last_hit_by", "");
        // Task 8: a new run starts with no submit status on screen, and any
        // still-in-flight submit from the run just left is abandoned rather
        // than awaited — see the doc comment on `pending_score`'s declaration.
        abandon_future(std::move(pending_score));
        blackboard.remove("score_submit_status");
        run_difficulty = static_cast<int>(difficulty_index);
        std::string label = "Normal";
        if (difficulty_index < config.difficulties.size()) {
            const DifficultyDef& d = config.difficulties[difficulty_index];
            label = d.name;
            apply_difficulty(config, d);
        }
        blackboard.set<std::string>("difficulty", label);
        tm.difficulty = label;
        // The one visible proof of the choice in a headless run — the menu itself
        // is the only place it shows on screen.
        // Lane F adds the ship: it is the only headless proof of which hull flew,
        // and it sits on this line rather than the summary so the replay canary's
        // comparison target is unchanged.
        std::cout << "Run start: difficulty " << label << "  ship "
                  << (selected_ship >= 0 && selected_ship < static_cast<int>(config.ships.size())
                          ? config.ships[static_cast<size_t>(selected_ship)].name
                          : std::string("Falcon"))
                  // Gameplay pack (D221): the weapon is the only other loadout
                  // choice — same line, same reason, still not the canary line.
                  << "  weapon " << blackboard.get_or<std::string>("weapon.name", "none")
                  << "\n";
        wave_spawner.set_config(&config);
        spawn_world();
        // Lane K (D100): the run's *state* is overlaid onto the world the shared
        // path just built. No entity graph is restored — the wave restarts from
        // the top of the saved wave, which is why there is nothing half-spawned
        // to reconcile.
        if (resume != nullptr && resume->present) {
            wave_spawner.resume_at_wave(resume->wave);
            run_save_apply(*resume, component_storage, blackboard);
            std::cout << "Run resumed: wave " << (resume->wave + 1)
                      << "  score " << resume->score << "\n";
        }
        // --dev --level N: start on wave N. Reuses the resume path's wave seek
        // rather than adding a second one; --level was parsed but unused before.
        if (opts.dev && opts.level > 1 && resume == nullptr) {
            wave_spawner.resume_at_wave(opts.level - 1);
            std::cout << "[dev] start wave " << opts.level << "\n";
        }
        // --dev: start fully kitted. Every stacked upgrade goes to its
        // max_stacks through ShopSystem's own purchase path, so the stats land
        // exactly as a real shopping trip would leave them. The item and
        // consumable slots are deliberately left empty — those are one-of
        // choices, and B + unlimited UNITS picks them in-game.
        if (opts.dev && resume == nullptr) {
            for (Entity p : component_storage.entities_with_component<PlayerTag>())
                if (auto s = component_storage.get_component<ShipState>(p); s.has_value())
                    shop.dev_max_upgrades(p, component_storage, blackboard, s->get());
            std::cout << "[dev] upgrades maxed\n";
        }
        phase = PHASE_PLAYING;
    };

    // Lane F (D82): the `menu_ship` widget is both the ship selector and the lock
    // readout. Resolved by name through ui.widget_id.<name>, the same path the HUD
    // gauges use — there is deliberately no second widget-lookup mechanism.
    Entity ship_widget = 0;
    bool ship_widget_resolved = false;
    auto refresh_ship_widget = [&]() {
        if (!ship_widget_resolved) {
            const double v = blackboard.get_or<double>("ui.widget_id.menu_ship", -1.0);
            ship_widget = v < 0.0 ? 0 : static_cast<Entity>(v);
            ship_widget_resolved = true;   // widget ids are load-time and survive spawn_world
        }
        if (ship_widget == 0) return;      // no main_menu authored — nothing to say
        auto el = component_storage.get_component<UIElement>(ship_widget);
        if (!el.has_value()) return;
        const int owned = owned_ship_count(config.ships, meta);
        if (owned > 1 && selected_ship < static_cast<int>(config.ships.size())) {
            el->get().label_text = "SHIP: " + config.ships[static_cast<size_t>(selected_ship)].name
                                 + "   (click to change)";
            el->get().style_id = "default_button";
            return;
        }
        // Only one hull owned: show the next purchasable ship's price instead of
        // a dead button (gameplay pack D221 — drones are bought with scrap).
        el->get().style_id = "subtitle";
        el->get().label_text.clear();
        for (const ShipDef& s : config.ships) {
            if (!s.locked && !ship_owned(meta, s)) {
                el->get().label_text = s.name + " costs " + std::to_string(s.scrap_cost)
                                     + " scrap  (you have "
                                     + std::to_string(meta.scrap) + ")";
                break;
            }
        }
    };

    auto widget_by_name = [&](const char* name, Entity& cache, bool& resolved) {
        if (!resolved) {
            const double v = blackboard.get_or<double>(std::string("ui.widget_id.") + name, -1.0);
            cache = v < 0.0 ? 0 : static_cast<Entity>(v);
            resolved = true;
        }
        return cache;
    };

    // Gameplay pack (D221) tier 6: the world drone IS the hangar preview, so a
    // ship cycle reskins the live entity immediately (start_run re-copies the
    // pristine config, so this temporary sidecar overwrite cannot leak).
    auto reskin_player = [&]() {
        if (selected_ship >= 0 && selected_ship < static_cast<int>(config.ships.size())) {
            const ShipDef& sd = config.ships[static_cast<size_t>(selected_ship)];
            if (!sd.sidecar.empty()) {
                config.player.sidecar = sd.sidecar;
                if (!sd.idle_clip.empty()) config.player.idle_clip = sd.idle_clip;
            }
        }
        load_player_sprite();
        if (!player_sprite.has_value()) return;
        for (Entity pl : component_storage.entities_with_component<PlayerTag>()) {
            component_storage.add_component<SpriteSheet>(pl, player_sprite->sprite_sheet);
            component_storage.add_component<Animation>(pl, player_sprite->animation);
            break;
        }
    };

    // The weapon the NEXT run will fly: the equipped one when owned, else the
    // selected ship's default. -1 when no catalogue is authored.
    auto current_weapon_index = [&]() -> int {
        std::string wname = meta.equipped_weapon;
        if (wname.empty() || !weapon_owned(meta, config.ships, wname)
            || find_weapon(config.weapons, wname) < 0) {
            if (selected_ship >= 0 && selected_ship < static_cast<int>(config.ships.size()))
                wname = config.ships[static_cast<size_t>(selected_ship)].default_weapon;
        }
        return find_weapon(config.weapons, wname);
    };

    // The next ship the hangar can sell: first non-locked, non-owned entry.
    auto next_purchasable_ship = [&]() -> int {
        for (size_t i = 0; i < config.ships.size(); ++i)
            if (!config.ships[i].locked && !ship_owned(meta, config.ships[i]))
                return static_cast<int>(i);
        return -1;
    };

    // Tier 6 (D221): the hangar's dynamic labels, rewritten every title frame —
    // the refresh_ship_widget idiom, batched. Pips share one x column (the
    // owner's "5 bubbles", aligned by authored geometry, not by text padding).
    auto refresh_hangar = [&]() {
        static Entity w_scrap = 0, w_weapon = 0, w_buy = 0, w_hint = 0;
        static bool r_scrap = false, r_weapon = false, r_buy = false, r_hint = false;
        static Entity w_name[8] = {0}, w_pips[8] = {0};
        static bool r_name[8] = {false}, r_pips[8] = {false};
        auto set_label = [&](Entity w, const std::string& text) {
            if (w == 0) return;
            if (auto el = component_storage.get_component<UIElement>(w); el.has_value())
                el->get().label_text = text;
        };
        set_label(widget_by_name("hangar_scrap", w_scrap, r_scrap),
                  "SCRAP: " + std::to_string(meta.scrap));
        const int wi = current_weapon_index();
        set_label(widget_by_name("menu_weapon", w_weapon, r_weapon),
                  wi >= 0 ? "WEAPON: " + config.weapons[static_cast<size_t>(wi)].name
                          : std::string("WEAPON: —"));
        if (selected_ship >= 0 && selected_ship < static_cast<int>(config.ships.size()) && wi >= 0) {
            const auto stat_rows = hangar::rows(
                config.ships[static_cast<size_t>(selected_ship)],
                config.weapons[static_cast<size_t>(wi)], config.dash);
            for (int i = 0; i < 8 && i < static_cast<int>(stat_rows.size()); ++i) {
                set_label(widget_by_name(("hs_name_" + std::to_string(i)).c_str(),
                                         w_name[i], r_name[i]), stat_rows[static_cast<size_t>(i)].name);
                set_label(widget_by_name(("hs_pips_" + std::to_string(i)).c_str(),
                                         w_pips[i], r_pips[i]),
                          hangar::pip_text(stat_rows[static_cast<size_t>(i)].pips));
            }
        }
        const int buy = next_purchasable_ship();
        set_label(widget_by_name("menu_buy", w_buy, r_buy),
                  buy < 0 ? std::string("ALL DRONES OWNED")
                          : "BUY " + config.ships[static_cast<size_t>(buy)].name + "  —  "
                            + std::to_string(config.ships[static_cast<size_t>(buy)].scrap_cost)
                            + " SCRAP");
        const int gi = buy >= 0 ? buy : -1;
        std::string hint;
        if (gi >= 0 && meta.scrap < config.ships[static_cast<size_t>(gi)].scrap_cost)
            hint = "Earn scrap by clearing waves and bosses.";
        else if (gi >= 0)
            hint = config.ships[static_cast<size_t>(gi)].name + " grants "
                 + config.ships[static_cast<size_t>(gi)].default_weapon + ".";
        set_label(widget_by_name("hangar_hint", w_hint, r_hint), hint);
    };

    // Tier 6 (D221): the flight report's lines, rewritten while it is up.
    auto refresh_run_stats = [&](bool victory) {
        static Entity w_title = 0, w_line[5] = {0};
        static bool r_title = false, r_line[5] = {false};
        auto set_label = [&](Entity w, const std::string& text) {
            if (w == 0) return;
            if (auto el = component_storage.get_component<UIElement>(w); el.has_value())
                el->get().label_text = text;
        };
        set_label(widget_by_name("rs_title", w_title, r_title),
                  victory ? "VICTORY" : "DRONE DOWN");
        const int shots = static_cast<int>(blackboard.get_or<double>("tm.shots", 0.0));
        const int hits  = static_cast<int>(blackboard.get_or<double>("tm.hits", 0.0));
        const std::string lines[5] = {
            "Wave reached: " + std::to_string(blackboard.get_or<int>("wave", 0)),
            "Score: " + std::to_string(blackboard.get_or<int>("score", 0)),
            "Scrap earned: +" + std::to_string(blackboard.get_or<int>("run.scrap", 0)),
            "Scrap total: " + std::to_string(meta.scrap),
            "Accuracy: " + (shots > 0 ? std::to_string(hits * 100 / shots) + "%" : std::string("—")),
        };
        for (int i = 0; i < 5; ++i)
            set_label(widget_by_name(("rs_line_" + std::to_string(i)).c_str(),
                                     w_line[i], r_line[i]), lines[i]);
    };

    // Lane K (D100): the two widgets this lane owns, resolved by name through
    // ui.widget_id.<name> — the same path the HUD gauges and the ship selector
    // use, deliberately not a second lookup mechanism. Widget ids are load-time
    // and survive spawn_world, so each is resolved once.
    Entity save_w = 0, continue_w = 0;
    bool save_w_resolved = false, continue_w_resolved = false;
    auto save_widget = [&]() { return widget_by_name("pause_save", save_w, save_w_resolved); };

    // Task 7: name_entry's two dynamic labels, resolved the same by-name way.
    Entity name_buf_w = 0, name_msg_w = 0, name_email_w = 0;
    bool name_buf_w_resolved = false, name_msg_w_resolved = false,
         name_email_w_resolved = false;

    // Task 9: the leaderboard's row labels + title, resolved the same way.
    // net::enabled() never changes after boot, so the hint is settled once.
    Entity lb_row_w[20] = {};
    bool lb_row_w_resolved[20] = {false};
    Entity lb_title_w = 0;
    bool lb_title_w_resolved = false;
    Entity menu_hint_w = 0;
    bool menu_hint_w_resolved = false;
    auto refresh_menu_hint = [&]() {
        const Entity w = widget_by_name("menu_hint_line", menu_hint_w, menu_hint_w_resolved);
        if (auto el = component_storage.get_component<UIElement>(w); el.has_value())
            el->get().label_text = net::enabled() ? "N renames your pilot  \xc2\xb7  L leaderboard"
                                                   : "N renames your pilot";
    };

    // CONTINUE is one widget that is either an offer or nothing at all: UIElement
    // has no visibility flag (D82 hit the same wall), so with no save it renders
    // as an empty flat label and its click is ignored below.
    auto refresh_update_widget = [&]() {
        const Entity w = widget_by_name("menu_update", update_w, update_w_resolved);
        if (w == 0) return;
        auto el = component_storage.get_component<UIElement>(w);
        if (!el.has_value()) return;
        if (!update_note.empty()) {
            el->get().style_id = "default_button";
            el->get().label_text = update_note;
        } else if (update_ready) {
            // shop_button is the one loud style on this screen (PLAY wears it):
            // an update the player never notices is the same as no update.
            el->get().style_id = "shop_button";
            el->get().label_text = "UPDATE AVAILABLE  -  v" + update_version;
        } else {
            el->get().style_id = "ghost";
            el->get().label_text.clear();
        }
    };
    auto refresh_continue_widget = [&]() {
        const Entity w = widget_by_name("menu_continue", continue_w, continue_w_resolved);
        if (w == 0) return;
        auto el = component_storage.get_component<UIElement>(w);
        if (!el.has_value()) return;
        const int ns = newest_slot();
        if (ns >= 0) {
            const RunSave& s = saved_slots[ns];
            el->get().style_id = "default_button";
            el->get().label_text = "CONTINUE  -  wave " + std::to_string(s.wave + 1)
                                 + ", " + (s.difficulty_name.empty()
                                               ? std::string("Normal")
                                               : s.difficulty_name);
        } else {
            // "ghost", not "subtitle": buttons always fill their bg, and only the
            // ghost style's bg matches the panel it sits on (see ui_styles).
            el->get().style_id = "ghost";
            el->get().label_text.clear();
        }
    };

    // (settings itself is loaded earlier, beside the telemetry state, because
    // bank_run_score's POST guard reads settings.analytics.)
    // v3 Tier 8: apply the saved display mode once, at boot. Logical
    // presentation already scales the 980x660 surface to whatever the window
    // is, so nothing else in the pipeline cares. Best-effort: a driver that
    // refuses fullscreen just stays windowed. Sited HERE rather than at the
    // load site above because that one runs before `window` exists.
    SDL_SetWindowFullscreen(window.get(), settings.fullscreen);
    Entity shake_w = 0, mini_w = 0, analytics_w = 0, fs_w = 0, rec_w[4] = {};
    bool shake_w_resolved = false, mini_w_resolved = false, analytics_w_resolved = false,
         fs_w_resolved = false, rec_w_resolved[4] = {};
    // Feedback screen widget caches: the four field value labels + status line,
    // plus the pause button whose caption is blanked when offline.
    Entity fb_w[5] = {};
    bool fb_w_resolved[5] = {};
    Entity fb_pause_btn_w = 0;
    bool fb_pause_btn_w_resolved = false;
    auto sync_settings_widgets = [&]() {
        const Entity sw = widget_by_name("settings_shake", shake_w, shake_w_resolved);
        const Entity mw = widget_by_name("settings_minimap", mini_w, mini_w_resolved);
        const Entity aw = widget_by_name("settings_analytics", analytics_w, analytics_w_resolved);
        const Entity fw = widget_by_name("settings_fullscreen", fs_w, fs_w_resolved);
        if (auto st = component_storage.get_component<UIState>(sw); st.has_value())
            st->get().value = settings.screen_shake ? 1.0f : 0.0f;
        if (auto st = component_storage.get_component<UIState>(mw); st.has_value())
            st->get().value = settings.minimap ? 1.0f : 0.0f;
        if (auto st = component_storage.get_component<UIState>(aw); st.has_value())
            st->get().value = settings.analytics ? 1.0f : 0.0f;
        if (auto st = component_storage.get_component<UIState>(fw); st.has_value())
            st->get().value = settings.fullscreen ? 1.0f : 0.0f;
    };
    auto refresh_records = [&]() {
        const std::string rows[4] = {
            "Lifetime score      " + std::to_string(meta.lifetime_score),
            "Prestige level      " + std::to_string(meta.prestige),
            "Best wave           " + std::to_string(meta.best_wave),
            "Runs flown          " + std::to_string(meta.runs_played),
        };
        for (int i = 0; i < 4; ++i) {
            const Entity w = widget_by_name(("records_" + std::to_string(i)).c_str(),
                                            rec_w[i], rec_w_resolved[i]);
            if (auto el = component_storage.get_component<UIElement>(w); el.has_value())
                el->get().label_text = rows[i];
        }
    };

    // Main-menu-suite Phase B: the save_slots rows. Labels are rewritten every
    // title frame; an empty slot's LOAD/DELETE buttons go ghost + disabled (the
    // same hidden-button spelling as menu_continue).
    Entity slot_w[RUN_SAVE_SLOTS][3] = {};        // [i] = label, load, delete
    bool slot_w_resolved[RUN_SAVE_SLOTS][3] = {};
    auto refresh_save_slots = [&]() {
        for (int i = 0; i < RUN_SAVE_SLOTS; ++i) {
            const std::string n = std::to_string(i);
            const Entity lbl = widget_by_name(("slot_label_" + n).c_str(),
                                              slot_w[i][0], slot_w_resolved[i][0]);
            const Entity ld  = widget_by_name(("slot_load_" + n).c_str(),
                                              slot_w[i][1], slot_w_resolved[i][1]);
            const Entity del = widget_by_name(("slot_del_" + n).c_str(),
                                              slot_w[i][2], slot_w_resolved[i][2]);
            const RunSave& s = saved_slots[i];
            if (auto el = component_storage.get_component<UIElement>(lbl); el.has_value()) {
                if (s.present) {
                    const std::string ship =
                        (s.ship_id >= 0 && s.ship_id < static_cast<int>(config.ships.size()))
                            ? config.ships[static_cast<size_t>(s.ship_id)].name
                            : std::string("Standard");
                    el->get().label_text = "SLOT " + std::to_string(i + 1) + "  -  wave "
                        + std::to_string(s.wave + 1) + ", "
                        + (s.difficulty_name.empty() ? std::string("Normal")
                                                     : s.difficulty_name)
                        + ", " + ship + "  -  " + std::to_string(s.score) + " pts";
                } else {
                    el->get().label_text = "SLOT " + std::to_string(i + 1) + "  -  empty";
                }
            }
            const char* names[2] = {"LOAD", "DELETE"};
            int bi = 0;
            for (Entity b : {ld, del}) {
                if (auto el = component_storage.get_component<UIElement>(b); el.has_value()) {
                    el->get().style_id = s.present ? "default_button" : "ghost";
                    // Text ignores style alpha, so a ghost button also blanks its
                    // caption (the menu_continue spelling of hidden).
                    el->get().label_text = s.present ? names[bi] : "";
                }
                if (auto st = component_storage.get_component<UIState>(b); st.has_value())
                    st->get().disabled = !s.present;
                ++bi;
            }
        }
    };

    // Main-menu-suite Phase A: the run_setup difficulty tabs use the shop-tab
    // convention — UIState.disabled on the selected tab is the selected look.
    Entity normal_w = 0, hard_w = 0;
    bool normal_w_resolved = false, hard_w_resolved = false;
    auto refresh_difficulty_tabs = [&]() {
        const Entity n = widget_by_name("menu_normal", normal_w, normal_w_resolved);
        const Entity h = widget_by_name("menu_hard", hard_w, hard_w_resolved);
        if (auto st = component_storage.get_component<UIState>(n); st.has_value())
            st->get().disabled = (setup_difficulty == 0);
        if (auto st = component_storage.get_component<UIState>(h); st.has_value())
            st->get().disabled = (setup_difficulty == 1);
    };

    // The title *is* the main menu, so it is pushed before the first frame; the
    // stack consumes the command at the top of the loop. Task 7: an unregistered
    // online player starts on name_entry instead (phase was already set above).
    blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH,
                                std::string(phase == PHASE_NAME_ENTRY ? SCREEN_NAME_ENTRY
                                                                      : SCREEN_MAIN_MENU));
    sync_settings_widgets();   // Phase C: checkboxes reflect the loaded file
    refresh_menu_hint();       // Task 9: L hint hidden in headless/offline mode

    Timer timer(opts.fps > 0 ? static_cast<double>(opts.fps) : 60.0);
    if (opts.seed.has_value()) timer.set_deterministic(true);
    timer.update_blackboard(blackboard);

    bool debug_paused = false, step_requested = false;
    bool f1_prev = false, f2_prev = false, space_prev = false, b_prev = false;
    bool f5_prev = false;   // --dev only: skip-to-next-wave edge
    bool tab_prev = false, q_prev = false;
    bool digit_prev[8] = {false};
    bool n_prev = false;   // Task 7: N renames the pilot from the title screen
    bool l_prev = false;   // Task 9: L opens the leaderboard from the title screen
    if (opts.paused) debug_paused = true;

    std::cout << "Reactor Drone v2 initialized. Arrows move, mouse aims, hold fire. ESC quits.\n";

    // v3 Tier 7 (D213): position-history trails. The history lives HERE, in a
    // render-side map keyed by entity — deliberately NOT a component. Nothing
    // in component_storage means no gameplay system can read it, so it cannot
    // feed the sim, touch the RNG, or move the replay canary. That is the
    // whole invariant the tier rests on, enforced by construction rather than
    // by a comment asking people to be careful.
    std::unordered_map<Entity, std::vector<line_mesh::P2>> trail_history;

    bool running = true;
    bool menu_paused = false;   // true while the pause screen is the top screen
    while (running) {
        timer.start_frame();
        input_system.process_events(component_storage, running, blackboard, renderer.get());
        uint64_t frame = timer.get_frame_count();

        // Engine-suite Phase 0 (D138): wipe last frame's render-FX event lists
        // (grid impulses, scar stamps) whether or not any consumer is enabled —
        // publishers append during the sim below, consumers read at render.
        fx_events::clear_frame(blackboard);

        // === HOOK: timescale === (Engine suite, D139 — Lane P / #1)
        // Owner: Temporal Overload. Rewrites the Blackboard "delta_time" for this
        // frame, BEFORE anything reads it — Timer::update_blackboard wrote the
        // real dt at the END of the previous iteration, so this is the single
        // point where the frame's seconds are decided.
        //
        // The scale is fed the REAL dt, never its own output, or the ease rate
        // would itself dilate and the effect would never recover. Only
        // PHASE_PLAYING dilates: a menu, the shop and the intermission are always
        // 1.0f, so no UI animation is ever slowed by a fight that is not running.
        {
            static TimescaleState timescale_state;
            const double real_dt = blackboard.get_or<double>("delta_time", 0.0);
            const float scale = tick_timescale(
                component_storage, blackboard, config.timescale, timescale_state,
                static_cast<float>(real_dt), phase == PHASE_PLAYING);
            // Exactly 1.0f when disabled, so the multiply is bit-for-bit a no-op.
            if (scale != 1.0f)
                blackboard.set<double>("delta_time", real_dt * static_cast<double>(scale));
            blackboard.set<float>("timescale", scale);   // HUD/audio may read it
        }
        // === END HOOK: timescale ===

        // Updater poll 1: the /version answer. Applied once; a failure
        // (offline, 404, junk body) simply leaves the button ghosted —
        // there is no retry and no message, because a player who cannot
        // reach the API cannot download an installer either.
        if (pending_version.valid() &&
            pending_version.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready) {
            const net::Response resp = pending_version.get();
            if (resp.status == 200) {
                const nlohmann::json j =
                    nlohmann::json::parse(resp.body, nullptr, false);
                if (j.is_object()) {
                    const std::string v = j.value("version", std::string());
                    const std::string u = j.value("installer_url", std::string());
                    if (update_check::is_newer(v, GAME_VERSION) &&
                        update_check::trusted_installer_url(u)) {
                        update_version = v;
                        update_url = u;
                        update_ready = true;
                        refresh_update_widget();
                    }
                }
            }
        }
        // Updater poll 2: the installer download. On success the game
        // hands off to the installer and QUITS — Windows cannot replace
        // files that are open, and the .iss relaunches the game itself
        // on a silent install (Flags: nowait; Check: WizardSilent).
        if (pending_download.valid() &&
            pending_download.wait_for(std::chrono::seconds(0)) ==
                std::future_status::ready) {
            const net::Response resp = pending_download.get();
            update_busy = false;
            if (resp.ok()) {
                const char* argv_[] = { update_file.c_str(), "/SILENT", nullptr };
                SDL_Process* proc = SDL_CreateProcess(argv_, false);
                if (proc != nullptr) {
                    // Destroying the handle does NOT stop the child
                    // (SDL_process.h) — that is the whole trick: the
                    // installer outlives us and puts the game back up.
                    SDL_DestroyProcess(proc);
                    running = false;
                } else {
                    update_note = "Update failed to start - try the website";
                }
            } else {
                update_note = "Download failed - try the website";
            }
            refresh_update_widget();
        }

        // Task 8: poll the score submission every frame, regardless of phase —
        // it can still be in flight after the player has already backed out to
        // the title. wait_for(0s) never blocks (net/http_client.hpp).
        // Reap finished telemetry POSTs. Nothing reads the responses — this only
        // stops the vector growing across a long session, and keeps each future's
        // destructor off the frame path.
        tm_inflight.erase(
            std::remove_if(tm_inflight.begin(), tm_inflight.end(),
                           [](std::future<net::Response>& f) {
                               return !f.valid() ||
                                      f.wait_for(std::chrono::seconds(0)) ==
                                          std::future_status::ready;
                           }),
            tm_inflight.end());

        if (pending_score.valid() &&
            pending_score.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const net::Response resp = pending_score.get();
            blackboard.set<std::string>("score_submit_status",
                resp.ok() ? std::string("Score submitted!")
                          : std::string("Score not submitted (offline)"));
        }

        // Task 9: poll the leaderboard fetch every frame, regardless of phase —
        // same reasoning as the score poll above. Defensive JSON parsing: the
        // public /top endpoint could return malformed JSON, an unexpected
        // shape, or garbage row data (agentProjectDocs house rule — never
        // crash, never UB on untrusted input). A row-level parse failure only
        // drops that one row; a top-level failure (bad JSON, missing "rows")
        // drops the whole response to the error state.
        if (pending_top.valid() &&
            pending_top.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const net::Response resp = pending_top.get();
            if (resp.ok()) {
                try {
                    const nlohmann::json j = nlohmann::json::parse(resp.body);
                    std::string rows_out;
                    int rank = 1;
                    for (const auto& r : j.at("rows")) {
                        try {
                            const std::string raw_name = r.at("name").get<std::string>();
                            // Fix round 1: r.at("score").get<long long>() is UB for an
                            // out-of-range JSON float (nlohmann does an unchecked
                            // static_cast<long long>(double) — from_json.hpp). Convert
                            // through double first (safe for any JSON number type) and
                            // range-check against the backend's own write-side bound
                            // (backend/src/worker.js: 0..10_000_000) before the cast
                            // that actually produces a long long. Out of range is
                            // exactly as malformed as a bad type: skip the row.
                            const double score_d = r.at("score").get<double>();
                            if (!std::isfinite(score_d) || score_d < 0.0 ||
                                score_d > 10'000'000.0)
                                throw std::out_of_range("score out of range");
                            const long long score = static_cast<long long>(score_d);
                            // Strip control bytes (incl. '\n', which would
                            // otherwise inject a fake extra row into this
                            // newline-delimited format) and clamp length — a
                            // hostile/garbage response must not break the
                            // format contract or rendering.
                            std::string clean;
                            for (char c : raw_name) {
                                if (clean.size() >= 40) break;
                                const unsigned char uc = static_cast<unsigned char>(c);
                                if (uc >= 0x20 && uc != 0x7F) clean.push_back(c);
                            }
                            rows_out += std::to_string(rank++) + ". " + clean + "  "
                                      + std::to_string(score) + "\n";
                        } catch (...) {
                            // One malformed row: skip it, keep the rest.
                        }
                    }
                    lb_rows = rows_out;
                    lb_status = "ok";
                } catch (...) {
                    lb_status = "error";
                }
            } else {
                lb_status = "error";
            }
            blackboard.set<std::string>("lb_status", lb_status);
            blackboard.set<std::string>("lb_rows", lb_rows);
        }

        // Scripted input injection (headless testing).
        for (const auto& c : opts.clicks) if (c.frame == frame) {
            blackboard.set("mouse_click_x", static_cast<float>(c.x));
            blackboard.set("mouse_click_y", static_cast<float>(c.y));
            blackboard.set("mouse.clicked", true);
            // Also drive the menu layer: UISystem hit-tests mouse.screen_x/y and
            // confirms a click only from a down AND an up inside the same widget.
            // A scripted click is instantaneous, so it supplies both edges in one
            // frame — otherwise no headless script could ever press a button.
            blackboard.set<float>("mouse.screen_x", static_cast<float>(c.x));
            blackboard.set<float>("mouse.screen_y", static_cast<float>(c.y));
            blackboard.set("mouse.down", true);
            blackboard.set("mouse.up", true);
        }
        for (const auto& hv : opts.hovers) if (hv.frame == frame) {
            blackboard.set("mouse.x", static_cast<double>(hv.x));
            blackboard.set("mouse.y", static_cast<double>(hv.y));
        }
        bool scripted_fire = false, scripted_advance = false, scripted_fire2 = false;
        int scripted_digit = 0;
        bool scripted_shop = false, scripted_tab = false, scripted_use = false;
        for (const auto& ka : opts.keys) if (ka.frame == frame) {
            // Scripted ESC mirrors the real key: it toggles the pause screen, it
            // does not quit. A script ends via --stopframe. Making this still quit
            // would mean headless runs could never exercise the pause path at all.
            if (ka.key == "ESC") blackboard.set("ui.escape_pressed", true);
            else if (ka.key == "SPACE") { scripted_fire = true; scripted_advance = true; }
            else if (ka.key == "RMB") scripted_fire2 = true;   // secondary fire (D221)
            else if (ka.key == "F1") debug_paused = !debug_paused;
            else if (ka.key == "B") scripted_shop = true;
            else if (ka.key == "TAB") scripted_tab = true;
            else if (ka.key == "Q") scripted_use = true;
            // Gameplay Phase 3: digits buy shop rows, so a headless script can
            // exercise a purchase (the shop is otherwise unreachable in tests).
            else if (ka.key.size() == 1 && ka.key[0] >= '1' && ka.key[0] <= '8')
                scripted_digit = ka.key[0] - '0';
        }

        // Debug keys (F1 pause, F2 step) + continuous-fire polling.
        const bool* keys = SDL_GetKeyboardState(nullptr);
        bool f1 = keys[SDL_SCANCODE_F1], f2 = keys[SDL_SCANCODE_F2];
        if (f1 && !f1_prev) debug_paused = !debug_paused;
        if (f2 && !f2_prev && debug_paused) step_requested = true;
        f1_prev = f1; f2_prev = f2;
        // === dev mode (--dev / --god) ===
        // Everything here is behind `opts.dev`, which is false on every normal
        // run: no key is read, no currency is written, no RNG is drawn, so the
        // replay canary is untouched.
        if (opts.dev) {
            // UNITS pinned, so the shop's own affordability checks always pass.
            for (Entity p : component_storage.entities_with_component<PlayerTag>())
                if (auto s = component_storage.get_component<ShipState>(p); s.has_value())
                    dev_top_up(true, s->get().currency);
            // F5: drop the wave on the floor and jump to the next one.
            // ponytail: enemies are destroyed outright (no loot, no score) — a
            // skip is not a clear. Skipping a boss wave reads to BossSystem as a
            // boss kill (it already treats a dead entity id as one), so the
            // reward offer still opens; pick one and carry on.
            const bool f5 = keys[SDL_SCANCODE_F5];
            if (f5 && !f5_prev && phase == PHASE_PLAYING) {
                for (Entity e : component_storage.entities_with_component<EnemyTag>())
                    component_storage.add_component<DestroyRequest>(e, DestroyRequest{});
                wave_spawner.resume_at_wave(wave_spawner.current_wave_index() + 1);
                std::cout << "[dev] wave -> " << (wave_spawner.current_wave_index() + 1) << "\n";
            }
            f5_prev = f5;
        }
        bool space = keys[SDL_SCANCODE_SPACE];
        bool space_edge = (space && !space_prev) || scripted_advance;
        space_prev = space;

        // Gameplay Phase 3 shop input: B opens/closes, 1-8 buy. Edge-detected here
        // beside the other key edges; the shop itself never touches SDL (R7).
        bool b_now = keys[SDL_SCANCODE_B];
        bool b_edge = (b_now && !b_prev) || scripted_shop;
        b_prev = b_now;
        // Gameplay Phase 4: TAB flips the shop page, Q spends the consumable.
        bool tab_now = keys[SDL_SCANCODE_TAB];
        bool tab_edge = (tab_now && !tab_prev) || scripted_tab;
        tab_prev = tab_now;
        bool q_now = keys[SDL_SCANCODE_Q];
        bool q_edge = (q_now && !q_prev) || scripted_use;
        q_prev = q_now;
        // Task 7: N at the title re-enters name_entry, pre-filled, to rename.
        bool n_now = keys[SDL_SCANCODE_N];
        bool n_edge = (n_now && !n_prev);
        n_prev = n_now;
        // Task 9: L at the title opens the leaderboard. Gated on net::enabled()
        // at the use site below, not here — determinism invariant #4: headless/
        // scripted runs never make this reachable regardless of the edge.
        bool l_now = keys[SDL_SCANCODE_L];
        bool l_edge = (l_now && !l_prev);
        l_prev = l_now;
        int digit = scripted_digit;
        for (int d = 0; d < 8; ++d) {
            bool down = keys[SDL_SCANCODE_1 + d];
            if (down && !digit_prev[d]) digit = d + 1;
            digit_prev[d] = down;
        }
        float mx, my; Uint32 mbtn = SDL_GetMouseState(&mx, &my);
        blackboard.set("mouse.held", (mbtn & SDL_BUTTON_LMASK) != 0 || scripted_fire);
        // Gameplay pack (D221): right mouse is the secondary-fire trigger.
        blackboard.set("mouse2.held", (mbtn & SDL_BUTTON_RMASK) != 0 || scripted_fire2);

        bool clicked = blackboard.get_or<bool>("mouse.clicked", false);
        bool advance = clicked || space_edge;

        // Escape toggles the pause screen. InputSystem publishes it as a one-frame
        // edge (and suppresses OS auto-repeat) precisely so the menu layer can own
        // it; the window close button and SDL_QUIT still exit directly.
        //
        // Only the base depth opens the pause screen. Escape on top of the wave
        // intermission or any future modal closes THAT, rather than stacking a
        // pause screen over it — otherwise Escape means two different things
        // depending on what happens to be open.
        //
        // Not at the title: the main menu is the only thing on screen there, and
        // popping it would leave a run that can only be started by SPACE.
        // Main-menu-suite Phase A: at the title, ESC backs out of run_setup to the
        // hub. Title screens REPLACE each other (CLEAR_TO, never PUSH): they fully
        // overlap on the canvas, and a stacked lower screen still renders — the
        // hub's pulsing PLAY would bleed through run_setup's panel.
        if (blackboard.get_or<bool>("ui.escape_pressed", false) && phase == PHASE_TITLE) {
            const std::vector<std::string> stack = ScreenStackSystem::get_stack(blackboard);
            if (!stack.empty() && (stack.back() == SCREEN_RUN_SETUP ||
                                   stack.back() == SCREEN_SAVE_SLOTS ||
                                   stack.back() == SCREEN_SETTINGS ||
                                   stack.back() == SCREEN_RECORDS ||
                                   stack.back() == SCREEN_HOW))
                blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                            std::string(SCREEN_MAIN_MENU));
        }
        // Main-menu-suite Phase C: ESC on the end screens returns to the hub
        // (click / SPACE still restarts, unchanged). Depth check keeps the
        // prestige-offer modal's ESC as a plain dismiss.
        if (blackboard.get_or<bool>("ui.escape_pressed", false) &&
            (phase == PHASE_GAMEOVER || phase == PHASE_VICTORY) &&
            ScreenStackSystem::depth(blackboard) <= 1) {
            phase = PHASE_TITLE;
            blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                        std::string(SCREEN_MAIN_MENU));
        }
        // Minor 1 (Task 7 review round 1): PHASE_NAME_ENTRY has its own ESC
        // handling below (CLEAR_TO the title) — letting this generic handler
        // also consume the escape flag first pushed/popped the screen stack a
        // frame ahead of that CLEAR_TO, producing a one-frame blank screen.
        // Task 9: PHASE_LEADERBOARD has the same shape of its own ESC handling,
        // and the feedback form (specs/feedback-reports.md) likewise.
        if (blackboard.get_or<bool>("ui.escape_pressed", false) && phase != PHASE_TITLE &&
            phase != PHASE_NAME_ENTRY && phase != PHASE_LEADERBOARD &&
            phase != PHASE_FEEDBACK && !run_stats_up) {
            if (ScreenStackSystem::depth(blackboard) <= 1) {
                blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH, std::string(SCREEN_PAUSE));
                // Lane K: the button says SAVE again each time the screen opens,
                // rather than showing the last visit's "SAVED" confirmation.
                if (Entity w = save_widget(); w != 0) {
                    if (auto el = component_storage.get_component<UIElement>(w); el.has_value())
                        el->get().label_text = "SAVE";
                }
                // Feedback needs the network; offline shows a blank button and
                // the click handler is net-gated too, so a blind click is a no-op.
                if (Entity w = widget_by_name("pause_feedback", fb_pause_btn_w,
                                              fb_pause_btn_w_resolved);
                    w != 0) {
                    if (auto el = component_storage.get_component<UIElement>(w); el.has_value())
                        el->get().label_text = net::enabled() ? "FEEDBACK" : "";
                }
            } else {
                blackboard.set<bool>(ScreenStackSystem::CMD_POP, true);
            }
        }

        // The pause screen freezes the sim. Note this is NOT is_modal(): the wave
        // intermission is also a modal screen, and it must keep the drone flying.
        //
        // ANYWHERE in the stack, not just on top: the feedback form pushes over
        // the pause screen, and a top-only test silently un-froze the three
        // `if (sim)` blocks outside the phase machine (particle ageing, trauma
        // decay, and timer.end_frame() advancing sim time) while the player
        // typed. The phase-gated main sim block was never affected — nothing
        // could move or hurt the drone — but the frozen battlefield's effects
        // drained away and the run clock kept running behind the form.
        {
            const std::vector<std::string> stack = ScreenStackSystem::get_stack(blackboard);
            menu_paused = std::find(stack.begin(), stack.end(), SCREEN_PAUSE) != stack.end();
        }
        bool sim = ((!debug_paused && !menu_paused) || step_requested);

        // === UI & menu layer ===
        // Order matters: the stack consumes last frame's push/pop commands first,
        // so UIScreen::active is settled before UISystem hit-tests against it,
        // and before the phase machine below reads a confirmed click.
        screen_stack.process_commands(blackboard, component_storage);
        ui_system.update(component_storage, entity_manager, blackboard);

        // Pause-screen buttons. Handled here rather than in the phase machine
        // because the pause screen can be raised from any phase, and because the
        // phase machine is gated on `sim` — which pausing has just turned off.
        if (menu_paused) {
            const std::string pause_click =
                blackboard.get_or<std::string>(UISystem::UI_CLICK_KEY, std::string());
            if (pause_click == "on_resume_click") {
                blackboard.remove(UISystem::UI_CLICK_KEY);
                blackboard.set<bool>(ScreenStackSystem::CMD_POP, true);
            } else if (pause_click == "on_save_run_click") {
                // Lane K (D100): the pause menu's Save. Captures the run's state —
                // wave, credits, gear, hull — and nothing about the world. Writing
                // fails silently by design: a read-only disk must not end the run.
                blackboard.remove(UISystem::UI_CLICK_KEY);
                RunSave& slot = saved_slots[active_slot];
                slot = run_save_capture(component_storage, blackboard,
                                        wave_spawner.current_wave_index(),
                                        run_difficulty,
                                        blackboard.get_or<std::string>("difficulty",
                                                                       std::string("Normal")),
                                        selected_ship, config.seed);
                // Wall-clock only ever flows INTO a save file, never out of one
                // into the sim — CONTINUE just sorts by it (D80 discipline).
                slot.saved_at = static_cast<long long>(std::time(nullptr));
                const bool ok = run_save_write(run_save_path(active_slot + 1), slot);
                if (!ok) slot.present = false;
                if (Entity w = save_widget(); w != 0) {
                    if (auto el = component_storage.get_component<UIElement>(w); el.has_value())
                        el->get().label_text = ok ? "SAVED" : "SAVE FAILED";
                }
            } else if (pause_click == "on_quit_click") {
                blackboard.remove(UISystem::UI_CLICK_KEY);
                bank_run_score(blackboard, "quit");   // Lane F: quitting still ends the run
                running = false;
            } else if (pause_click == "on_to_menu_click") {
                // Main-menu-suite Phase C: back to the hub without exiting. Banks
                // like QUIT (the run is over as far as progression is concerned;
                // the slot file survives, so CONTINUE can pick it back up). The
                // world's entities stay where they are — PHASE_TITLE runs no sim,
                // so the arena simply freezes behind the menu until the next
                // start_run's spawn_world rebuilds it.
                blackboard.remove(UISystem::UI_CLICK_KEY);
                bank_run_score(blackboard, "quit");
                phase = PHASE_TITLE;
                blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                            std::string(SCREEN_MAIN_MENU));
            } else if (pause_click == "on_feedback_click" && net::enabled()) {
                blackboard.remove(UISystem::UI_CLICK_KEY);
                fb_from_pause = true;
                fb_prev_phase = phase;
                // Run context is captured at OPEN — the numbers the player is
                // looking at — not at send, when the form may outlive the wave.
                fb_ctx = {{"in_run", true},
                          {"wave", blackboard.get_or<int>("wave", 0)},
                          {"score", blackboard.get_or<int>("score", 0)},
                          {"ship", selected_ship},
                          {"prestige", meta.prestige},
                          {"difficulty", blackboard.get_or<std::string>(
                                             "difficulty", std::string("Normal"))}};
                fb_focus = 0; fb_msg.clear();
                phase = PHASE_FEEDBACK;   // the sim block only runs in PHASE_PLAYING
                blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH,
                                            std::string(SCREEN_FEEDBACK));
                SDL_StartTextInput(window.get());
            }
        }

        // === HOOK: feedback form === (specs/feedback-reports.md)
        // Handled HERE, above the phase machine, for exactly the reason the
        // pause buttons are: the phase machine is gated on `sim`, and opening
        // this form over the pause screen turns `sim` off. Left inside that
        // chain the form went inert the moment the sim froze — no typing, no
        // submit, and no ESC either, which soft-locked the player on the form.
        if (phase == PHASE_FEEDBACK) {
            // specs/feedback-reports.md: four typed fields, TAB cycles focus,
            // ENTER submits (in BODY it inserts a newline instead), ESC backs
            // out. Same frame plumbing as PHASE_NAME_ENTRY directly above.
            constexpr size_t FB_CAPS[4] = {120, 4000, 200, 60};
            const std::string typed =
                blackboard.get_or<std::string>("ui.text_input", std::string());
            {
                std::string& cur = fb_fields[fb_focus];
                for (char c : typed) {
                    if (cur.size() >= FB_CAPS[static_cast<size_t>(fb_focus)]) break;
                    if (c >= 0x20 && c < 0x7F) cur.push_back(c);
                }
                if (blackboard.get_or<bool>("ui.backspace_pressed", false) &&
                    !cur.empty())
                    cur.pop_back();
            }
            if (blackboard.get_or<bool>("ui.tab_pressed", false))
                fb_focus = (fb_focus + 1) % 4;

            // Poll last frame's submit before reading enter/escape (the
            // name_entry ordering — a same-frame response never races input).
            if (pending_feedback.valid() &&
                pending_feedback.wait_for(std::chrono::seconds(0)) ==
                    std::future_status::ready) {
                const net::Response resp = pending_feedback.get();
                if (resp.status == 200) {
                    for (auto& f : fb_fields) f.clear();
                    fb_focus = 0;
                    fb_msg = "Thanks - received!";
                } else {
                    // Typed content is kept: a retry must not lose their words.
                    fb_msg = "Couldn't send - check your connection";
                }
            }

            const bool fb_enter = blackboard.get_or<bool>("ui.enter_pressed", false);
            if (fb_enter && fb_focus == 1) {
                if (fb_fields[1].size() < FB_CAPS[1]) fb_fields[1].push_back('\n');
            } else if (fb_enter && !pending_feedback.valid()) {
                if (fb_fields[0].find_first_not_of(' ') == std::string::npos) {
                    fb_msg = "Subject is required";
                } else if (fb_fields[1].find_first_not_of(" \n") == std::string::npos) {
                    fb_msg = "Body is required";
                } else {
                    nlohmann::json j = fb_ctx;
                    j["subject"] = fb_fields[0];
                    j["body"] = fb_fields[1];
                    j["tags"] = fb_fields[2];
                    j["from_name"] = fb_fields[3];
                    j["player_id"] = meta.player_id;
                    j["pilot"] = meta.registered ? meta.player_name : "";
                    j["version"] = GAME_VERSION;
                    j["platform"] = RD_PLATFORM;
                    j["session_id"] = session_id;
                    fb_msg = "Sending...";
                    pending_feedback = net::post_json(
                        std::string(net::NET_BASE) + "/feedback", j.dump(),
                        net::NET_GAME_KEY);
                }
            }

            if (blackboard.get_or<bool>("ui.escape_pressed", false)) {
                abandon_future(std::move(pending_feedback));
                SDL_StopTextInput(window.get());
                for (auto& f : fb_fields) f.clear();
                fb_focus = 0;
                fb_msg.clear();
                if (fb_from_pause) {
                    phase = fb_prev_phase;   // back under the pause screen
                    blackboard.set<bool>(ScreenStackSystem::CMD_POP, true);
                } else {
                    phase = PHASE_TITLE;
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_MAIN_MENU));
                }
            }

            if (phase == PHASE_FEEDBACK) {
                // Widget rewrites: focused field carries the cursor; BODY
                // shows the tail that fits its 5-line box (full text sends).
                static const char* FB_NAMES[5] = {"fb_subject", "fb_body",
                                                  "fb_tags", "fb_from", "fb_msg"};
                for (int i = 0; i < 4; ++i) {
                    Entity w = widget_by_name(FB_NAMES[i], fb_w[i], fb_w_resolved[i]);
                    if (w == 0) continue;
                    auto el = component_storage.get_component<UIElement>(w);
                    if (!el.has_value()) continue;
                    std::string text = fb_fields[i];
                    if (i == 1) {
                        // last 5 newline-separated lines of the body
                        int lines = 0;
                        size_t pos = text.size();
                        while (pos > 0 && lines < 5) {
                            pos = text.find_last_of('\n', pos - 1);
                            if (pos == std::string::npos) { pos = 0; break; }
                            ++lines;
                        }
                        if (pos > 0) text = text.substr(pos + 1);
                    }
                    el->get().label_text = (i == fb_focus) ? text + "_" : text;
                }
                if (Entity w = widget_by_name(FB_NAMES[4], fb_w[4], fb_w_resolved[4]);
                    w != 0) {
                    if (auto el = component_storage.get_component<UIElement>(w);
                        el.has_value()) {
                        el->get().label_text =
                            !fb_msg.empty()
                                ? fb_msg
                                : "TAB next field - ENTER send - ESC back";
                        el->get().style_id =
                            (fb_msg.find("required") != std::string::npos ||
                             fb_msg.find("Couldn't") != std::string::npos)
                                ? "pip_loss" : "caption";
                    }
                }
            }
        }

        // === HOOK: prestige === (Iteration 5, D128/D129 — Lane O / #14)
        // The arc-complete offer. Handled HERE, above the phase machine, for the
        // same reason the pause buttons are: the click that presses PRESTIGE RUN
        // is also a plain `advance`, which the game-over/victory branch below
        // reads as "retry". Consuming both the click and `advance` in one place
        // is the only way the two can never fire on the same frame — the same
        // trap the title screen hit with SPACE vs `advance` (D50).
        // Tier 6 (D221): the flight report — refresh while up; CONTINUE (or ESC)
        // pops it and lets the end-screen flow (retry banner / prestige) resume.
        if (run_stats_up) {
            refresh_run_stats(phase == PHASE_VICTORY);
            const std::string rs_click =
                blackboard.get_or<std::string>(UISystem::UI_CLICK_KEY, std::string());
            const bool esc = blackboard.get_or<bool>("ui.escape_pressed", false);
            if (rs_click == "on_run_stats_continue" || esc) {
                if (!rs_click.empty()) blackboard.remove(UISystem::UI_CLICK_KEY);
                blackboard.set<bool>(ScreenStackSystem::CMD_POP, true);
                run_stats_up = false;
            }
        }

        {
            static bool offer_up = false;
            static Entity prestige_line = 0;
            static bool prestige_line_resolved = false;

            const bool want_offer = (phase == PHASE_VICTORY) && !run_stats_up;
            if (want_offer != offer_up) {
                if (want_offer)
                    blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH,
                                                std::string(SCREEN_PRESTIGE));
                else
                    blackboard.set<bool>(ScreenStackSystem::CMD_POP, true);
                offer_up = want_offer;
                // ponytail: Escape at the victory screen pops the offer and it
                // does not come back until the next win. Retry (click anywhere)
                // still works, and UIElement has no visibility flag to do better
                // without an engine change (D82 hit the same wall).
            }
            if (want_offer) {
                // The level is never authored in the screen data — this rewrites
                // the line every frame, the way BossSystem rewrites its reward
                // buttons (D71).
                if (Entity w = widget_by_name("prestige_line", prestige_line,
                                              prestige_line_resolved); w != 0) {
                    if (auto el = component_storage.get_component<UIElement>(w);
                        el.has_value()) {
                        el->get().label_text =
                            meta.prestige >= PRESTIGE_MAX_LEVEL
                                ? prestige_summary(meta.prestige) + "  (MAX)"
                                : "NEXT: " + prestige_summary(meta.prestige + 1);
                    }
                }
                const std::string click =
                    blackboard.get_or<std::string>(UISystem::UI_CLICK_KEY, std::string());
                if (click == "on_prestige_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    advance = false;                 // not also a retry
                    meta.prestige = prestige_clamp(meta.prestige + 1);
                    meta_write(meta_save_path(), meta);   // banked before the run,
                    // so a crash mid-prestige-run cannot cost the level that was
                    // just earned. bank_run_score already fired on the victory.
                    blackboard.set<bool>(ScreenStackSystem::CMD_POP, true);
                    offer_up = false;
                    // The one application site does the rest: same difficulty,
                    // same ship, buffed base stats, empty upgrade sheet.
                    start_run(static_cast<size_t>(run_difficulty));
                }
            }
        }
        // === END HOOK: prestige ===

        // v2 Phase 5c: equipment visuals (thruster cone, upgrade kit,
        // shield field, item aura). They ride the aim angle player_aim
        // writes and the player's POSITION — so the call sites sit after
        // movement + clamp + push_out_of_solids, or every follower renders
        // one frame behind the hull (bug 002). Pure functions of ShipState
        // plus a cosmetic phase — no RNG, the canary cannot move.
        auto update_equipment_visuals = [&]() {
                // D134: the field's hum is a free-running loop, advanced once per
                // playing frame (not per player) so it cannot double-step.
                {
                    constexpr float FIELD_HUM_PERIOD = 0.64f;   // 8 frames at ~0.08s
                    field_phase += static_cast<float>(
                        blackboard.get_or<double>("delta_time", 0.0)) / FIELD_HUM_PERIOD;
                    field_phase -= std::floor(field_phase);
                }

                for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                    auto rot = component_storage.get_component<Rotation>(p);
                    auto pos = component_storage.get_component<Position>(p);
                    auto sz  = component_storage.get_component<Size>(p);
                    if (!rot.has_value() || !pos.has_value() || !sz.has_value()) break;
                    const float pcx = pos->get().x + sz->get().width * 0.5f;
                    const float pcy = pos->get().y + sz->get().height * 0.5f;

                    // Thruster: a cone opposite the aim, instead of the 180° aura it
                    // shipped as. ponytail: offset stays at the hull centre — emitter
                    // offsets are host-*local* and unrotated, so a rear offset would
                    // detach from the ship as it turns; the cone alone reads as exhaust.
                    if (auto em = component_storage.get_component<ParticleEmitter>(p); em.has_value()) {
                        em->get().direction = rot->get().angle + 180.0f;
                        em->get().cone_half_angle = 26.0f;
                    }

                    // D133/D134: the upgrade kit and the shield field. Both ride the
                    // aim angle written above, so they belong here rather than with
                    // the render step. Pure functions of ShipState — no RNG, nothing
                    // accumulated but a cosmetic phase — so the canary cannot move.
                    if (auto st = component_storage.get_component<ShipState>(p); st.has_value()) {
                        const ShipState& ship = st->get();
                        const float ang = rot->get().angle;
                        const float pw = sz->get().width, ph = sz->get().height;

                        for (int i = 0; i < upgrade_visuals::KIT_COUNT; ++i) {
                            const bool worn = upgrade_visuals::part_worn(ship, i);
                            auto ksz = component_storage.get_component<Size>(kit_parts[i]);
                            auto kps = component_storage.get_component<Position>(kit_parts[i]);
                            auto krt = component_storage.get_component<Rotation>(kit_parts[i]);
                            if (!ksz.has_value() || !kps.has_value() || !krt.has_value()) continue;
                            // Parked = zero size (D58's idiom): the entity stays
                            // pooled, and a zero-extent sprite draws nothing.
                            ksz->get().width  = worn ? pw : 0.0f;
                            ksz->get().height = worn ? ph : 0.0f;
                            kps->get().x = pcx - pw * 0.5f;
                            kps->get().y = pcy - ph * 0.5f;
                            krt->get().angle = ang;
                        }

                        // The field. Its sprite is 192px against the chassis's 128,
                        // which is what holds the ring clear of the hull.
                        const float delay_total =
                            blackboard.get_or<float>("ship.shield_regen_delay", 0.0f);
                        const auto fstate = upgrade_visuals::field_state(ship, delay_total);
                        const float fw = pw * upgrade_visuals::FIELD_SIZE_MULT;
                        const float fh = ph * upgrade_visuals::FIELD_SIZE_MULT;
                        const bool shown = (fstate != upgrade_visuals::FieldState::Hidden);
                        if (auto fs = component_storage.get_component<Size>(shield_field);
                            fs.has_value()) {
                            fs->get().width  = shown ? fw : 0.0f;
                            fs->get().height = shown ? fh : 0.0f;
                        }
                        if (auto fp = component_storage.get_component<Position>(shield_field);
                            fp.has_value()) {
                            fp->get().x = pcx - fw * 0.5f;
                            fp->get().y = pcy - fh * 0.5f;
                        }
                        if (auto fr = component_storage.get_component<Rotation>(shield_field);
                            fr.has_value()) {
                            // The bloom is baked pointing right, so the ring turns to
                            // put it where the hit came from. Everything else on the
                            // ring is radially symmetric, so this is invisible except
                            // during a bloom.
                            fr->get().angle =
                                (fstate == upgrade_visuals::FieldState::Hit)
                                    ? blackboard.get_or<float>("player.hit_bearing", 0.0f)
                                    : 0.0f;
                        }
                        if (auto fss = component_storage.get_component<SpriteSheet>(shield_field);
                            fss.has_value()) {
                            const float frac = ship.shield_max > 0.0f
                                                   ? ship.shield / ship.shield_max : 0.0f;
                            // The bloom decays, so it reads its progress through the
                            // hit window rather than the free-running hum phase —
                            // otherwise which bloom frame you got would depend on
                            // when in the loop the hit happened.
                            float phase = field_phase;
                            if (fstate == upgrade_visuals::FieldState::Hit &&
                                upgrade_visuals::FIELD_HIT_TIME > 0.0f) {
                                phase = std::clamp(
                                    (delay_total - ship.shield_delay) /
                                        upgrade_visuals::FIELD_HIT_TIME,
                                    0.0f, 0.999f);
                            }
                            fss->get().current_frame =
                                upgrade_visuals::field_frame(fstate, frac, phase);
                        }
                    }

                    // Item aura: one emitter, reconfigured from the equipped item.
                    if (auto aura = component_storage.get_component<ParticleEmitter>(item_aura);
                        aura.has_value()) {
                        ParticleEmitter& a = aura->get();
                        if (auto ap = component_storage.get_component<Position>(item_aura);
                            ap.has_value()) { ap->get().x = pcx; ap->get().y = pcy; }

                        int item = -1;
                        if (auto s = component_storage.get_component<ShipState>(p); s.has_value())
                            item = s->get().item_id;

                        a.active = (item >= 0);
                        a.shape = EmitterShape::Circle;
                        a.cone_half_angle = 180.0f;
                        a.particle_lifetime = 0.6f;
                        a.min_speed = 0.0f; a.max_speed = 22.0f;
                        a.start_size = 6.0f; a.end_size = 0.0f;
                        a.end_a = 0;
                        // Emission scales with the radius being described: at the
                        // Magnet Core's 220-unit reach, 26/s is a dozen specks lost in
                        // the backdrop. These rates were raised until the field
                        // actually reads on screen. Worst case ~90 live particles of
                        // the 2000 budget — see ENGINE.md §5 before raising further.
                        switch (item) {
                            case item_ids::MAGNET_CORE:      // amber, the loot colour, at pull range
                                a.radius = config.economy.pickup_magnet_radius;
                                a.emission_rate = 150.0f;
                                a.start_r = 255; a.start_g = 210; a.start_b = 90;  a.start_a = 200;
                                a.end_r   = 180; a.end_g   = 120; a.end_b   = 30;
                                break;
                            case item_ids::REPULSOR_FIELD:   // cold shove field, at push range
                                a.radius = config.shop.repulsor_radius;
                                a.emission_rate = 120.0f;
                                a.start_r = 120; a.start_g = 220; a.start_b = 255; a.start_a = 200;
                                a.end_r   = 30;  a.end_g   = 90;  a.end_b   = 180;
                                break;
                            case item_ids::REACTIVE_PLATING: // hot shell hugging the hull
                                a.radius = sz->get().width * 0.8f;
                                a.emission_rate = 50.0f;
                                a.start_r = 255; a.start_g = 130; a.start_b = 60;  a.start_a = 220;
                                a.end_r   = 160; a.end_g   = 40;  a.end_b   = 20;
                                break;
                            case item_ids::SALVAGER:         // sparse gold flecks
                                a.radius = sz->get().width * 1.2f;
                                a.emission_rate = 26.0f;
                                a.start_r = 255; a.start_g = 235; a.start_b = 150; a.start_a = 220;
                                a.end_r   = 200; a.end_g   = 160; a.end_b   = 60;
                                break;
                            default: break;                  // nothing equipped: inactive
                        }
                    }
                    break;
                }
        };

        // === State machine + gameplay ===
        if (phase == PHASE_PLAYING && sim) {
            apply_move_speed();
            player_control.update(component_storage);

            // === HOOK: dash === (Iteration 3, D51 — Lane B / #5)
            // Owner: the thruster-dash phase. Runs after player_control has written
            // the frame's velocity and before player_fire, so the burst overrides
            // ordinary movement for its window. Nothing else may edit this block.
            {
                // Lane N (D123): the drone wears its shop upgrades as its engine
                // plume. Here because this is the one per-frame block this lane
                // owns; it is idempotent, so a resumed run and a restart both get
                // the right look with no second application site. Before
                // tick_dash, so the burst captures this tier's emission rate as
                // the one to restore.
                for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                    if (auto s = component_storage.get_component<ShipState>(p); s.has_value())
                        upgrade_visuals::apply_to_player(component_storage, p, s->get());
                    break;
                }

                // Lane N (D120): the dash is SPACE now, on a 10 s cooldown.
                //
                // SPACE is also the title screen's start key, and the two can
                // never collide: this block only runs in PHASE_PLAYING && sim,
                // and the title/game-over branches only run when it does not. The
                // remaining case is one physical press spanning both — press at
                // the title, still held on the run's first frame — and that is why
                // the trigger is `space_edge`, the same one-frame edge the title
                // consumed, rather than the held key it used to be. The press that
                // started the run cannot also spend the dash, and a dash cannot
                // restart a run.
                //
                // LSHIFT stays as the scripted/headless alias: a scripted SPACE
                // also means "start the run" (see the key parser above), so a
                // headless script has no other way to ask for a dash on its own.
                bool dash_key = space_edge;
                for (const auto& ka : opts.keys)
                    if (ka.frame == frame && ka.key == "LSHIFT") dash_key = true;
                static DashState dash_state;   // one dash's scratch; see dash_system.hpp
                tick_dash(component_storage, entity_manager, blackboard, config.dash,
                          dash_state, dash_key,
                          static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));

                // Engine suite (D140): a dash drags the lattice behind it. One
                // publish per dashing frame, at the drone's centre — render-only
                // (fx_events is one-way), so this cannot reach the sim.
                for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                    auto ss = component_storage.get_component<ShipState>(p);
                    if (!ss.has_value() || ss->get().dash_timer <= 0.0f) break;
                    auto pp = component_storage.get_component<Position>(p);
                    auto psz = component_storage.get_component<Size>(p);
                    if (pp.has_value() && psz.has_value()) {
                        fx_events::push_impulse(
                            blackboard,
                            pp->get().x + psz->get().width * 0.5f,
                            pp->get().y + psz->get().height * 0.5f,
                            0.45f);
                    }
                    break;
                }
            }
            // === END HOOK: dash ===

            // === HOOK: telemetry === (specs/telemetry.md)
            // Pure observation: reads player state, writes only the report. Zero
            // RNG draws and nothing the sim reads, so the replay canary cannot
            // move (Invariant 4).
            {
                float hull = 0.0f, shield = 0.0f, px = 0.0f, py = 0.0f;
                long long currency = 0;
                for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                    if (auto h = component_storage.get_component<Health>(p); h.has_value())
                        hull = h->get().current;
                    if (auto s = component_storage.get_component<ShipState>(p); s.has_value()) {
                        shield = s->get().shield;
                        currency = s->get().currency;
                        const int cur = s->get().consumable_id;
                        if (prev_consumable >= 0 && cur < 0 && !prev_consumable_name.empty())
                            tm.consumables_used[prev_consumable_name]++;
                        if (cur >= 0)
                            prev_consumable_name =
                                blackboard.get_or<std::string>("ship.consumable_name",
                                                               std::string());
                        prev_consumable = cur;
                    }
                    if (auto pos = component_storage.get_component<Position>(p); pos.has_value()) {
                        px = pos->get().x; py = pos->get().y;
                        if (auto sz = component_storage.get_component<Size>(p); sz.has_value()) {
                            px += sz->get().width * 0.5f; py += sz->get().height * 0.5f;
                        }
                    }
                }
                const int cur_wave = blackboard.get_or<int>("wave", 0);
                telemetry::frame_sample(
                    tm, blackboard.get_or<double>("delta_time", 0.0), cur_wave, hull, shield,
                    currency, px, py,
                    active_arena_index(config.arenas, std::max(cur_wave, 1)),
                    config.arena.center_x, config.arena.center_y, config.arena.radius);
            }
            // === END HOOK: telemetry ===

            player_aim.update(component_storage, blackboard);

            // === HOOK: director === (Engine suite, D142 — Lane Q / #8)
            // Owner: the Adaptive Director. Integrates a stress scalar from sim
            // state and hands WaveSpawnerSystem ONE spacing multiplier — never
            // counts, never skips the table. Publishes "director.stress".
            //
            // Pushed every frame, immediately before the spawner reads it, the
            // same discipline as player_control.set_speed: nothing caches it.
            {
                static DirectorState director_state;
                wave_spawner.set_spacing_mult(tick_director(
                    component_storage, blackboard, config.director, director_state,
                    static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)),
                    /*active=*/true));
            }
            // === END HOOK: director ===

            wave_spawner.update(blackboard, entity_manager, component_storage);

            // === HOOK: boss === (Iteration 3, D51 — Lane D / #4)
            // Owner: the boss phase. Runs straight after the spawner so a wave
            // flagged `boss` can spawn its boss and hold the clear condition in
            // the same frame the spawner would otherwise have finished the wave.
            {
                static BossSystem boss_system;
                boss_system.set_config(&config);
                boss_system.update(component_storage, entity_manager, blackboard,
                                   wave_spawner);

                // D72 — the wave-50 finale transforms the map MID-FIGHT.
                //
                // Wired to Lane E's begin_arena_shift(int) (D76-D79) at
                // integration, replacing the stub this lane carried because its
                // worktree predated the Lane E merge. begin_arena_shift is the
                // ONE transition: it strips outgoing colliders on the shift
                // frame, then plays the destruction/entry animation across the
                // 5s crossfade. Lane E verified it is callable mid-wave with
                // enemies alive; it has no dependency on wave_cleared.
                // v3 Tier 3 (D209): boss deaths hit harder than kills.
                // Tier 4 (D210): and fire a screen-space shockwave from centre.
                if (blackboard.get_or<bool>("boss.just_died", false)) {
                    blackboard.set<bool>("boss.just_died", false);
                    hitstop_left = std::max(hitstop_left,
                                            config.feedback.hitstop_frames_boss);
                    postfx.trigger_shock(0.5f, 0.5f);
                }
                if (boss_system.wants_arena_shift()) {
                    boss_system.clear_arena_shift_request();
                    // By name, not a magic index: the void is the 9th entry today
                    // and must not become a number in two places.
                    int idx = -1;
                    for (size_t i = 0; i < config.arenas.size(); ++i)
                        if (config.arenas[i].name == "Singularity") idx = static_cast<int>(i);
                    if (idx >= 0) begin_arena_shift(idx);
                }
            }
            // === END HOOK: boss ===

            // D15: wave_just_cleared() is a plain getter reset at the top of the
            // *next* update(), so it has to be read here, in the same frame. The
            // phase switch itself happens after the frame simulates, below.
            const bool wave_cleared = wave_spawner.wave_just_cleared();
            const bool shop_due = wave_cleared &&
                                  !wave_spawner.all_complete() &&
                                  (wave_spawner.current_wave_index() % 5 == 0);

            // v2 Phase 6 / 5b: swap the themed arena (backdrop + obstacle/hazard
            // props) when the wave crosses a configured activation. Keyed on the
            // integer wave, so it is deterministic under a fixed seed.
            //
            // Phase 5b moved this off the every-frame wave check, which landed the
            // swap mid-combat, onto the cleared-wave edge — the arena is empty and
            // the spawner is waiting out wave.delay, so there is room for a fade.
            if (!config.arenas.empty()) {
                if (active_arena < 0) {
                    // First playing frame of a run: snap, no crossfade.
                    int want = active_arena_index(config.arenas,
                                                  blackboard.get_or<int>("wave", 1));
                    apply_arena_props(want);
                    active_backdrop = &config.arenas[static_cast<size_t>(want)].backdrop_layers;
                } else if (wave_cleared && shift_pending < 0) {
                    // D15 again: the spawner publishes "wave" *before* it may
                    // increment, so on the clear frame the blackboard is stale by
                    // one. The wave about to start is current_wave_index() + 1.
                    int want = active_arena_index(config.arenas,
                                                  wave_spawner.current_wave_index() + 1);
                    begin_arena_shift(want);
                }

                if (outgoing_backdrop != nullptr) {
                    shift_timer += static_cast<float>(
                        blackboard.get_or<double>("delta_time", 0.0));
                    if (shift_pending >= 0 && shift_timer >= SHIFT_PROP_SWAP) {
                        apply_arena_props(shift_pending);
                        // Lane E (D77): the incoming props keep their colliders
                        // from frame one, but animate() scales those too, so a
                        // half-grown pillar is never an invisible wall.
                        growing_props = arena_vfx::capture_props(component_storage, arena_props);
                        shift_pending = -1;
                    }
                    if (shift_timer >= SHIFT_SECONDS) outgoing_backdrop = nullptr;
                }

                // === HOOK: arena-vfx === (Iteration 3, D51 — Lane E / #2)
                // Owner: the arena-transition VFX phase. Inside the shift tick, so
                // the outgoing props' destruction and the incoming props' arrival
                // animate against the same shift_timer the crossfade already runs on.
                //
                // D77. Two staggered passes over the same shift_timer the backdrop
                // crossfade rides, using the same smoothstep (arena_vfx.hpp). No
                // RNG here — the debris draws all happen inside ParticleSystem,
                // which is seeded, so determinism is untouched.
                if (!dying_props.empty() || !growing_props.empty()) {
                    const float t_out = shift_timer / SHIFT_SECONDS;
                    arena_vfx::animate(component_storage, dying_props, t_out,
                                       DEATH_STAGGER, /*shrink=*/true);
                    // t_out >= 1 means the window closed: nothing of the old
                    // arena may survive it, animated or not.
                    if (t_out >= 1.0f)
                        arena_vfx::destroy_all(entity_manager, component_storage, dying_props);

                    const float t_in = (shift_timer - SHIFT_PROP_SWAP) / BIRTH_SECONDS;
                    arena_vfx::animate(component_storage, growing_props, t_in,
                                       BIRTH_STAGGER, /*shrink=*/false);
                    if (t_in >= 1.0f) growing_props.clear();  // left at authored size
                }
                // === END HOOK: arena-vfx ===
            }

            // === HOOK: surge === (Engine suite, D149 — Lane X / #7)
            // Owner: Reactor Surge Events. After the arena-shift tick so a surge
            // schedules against the settled arena. Sim-side: the scheduler takes
            // three RNG draws on EVERY wave change, before any conditional, so an
            // arena with no surge table draws exactly as many values as one with
            // a full table (R2 / Invariant 4).
            {
                static int surge_prev_wave = -1;
                const int surge_wave = blackboard.get_or<int>("wave", 0);
                const bool surge_wave_changed = surge_wave != surge_prev_wave;
                surge_prev_wave = surge_wave;
                surges.update(component_storage, entity_manager, blackboard, forces,
                              active_arena, surge_wave, surge_wave_changed,
                              static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));
            }
            // === END HOOK: surge ===

            enemy_seek.update(component_storage, blackboard);

            // === HOOK: enemy-fire === (Iteration 3, D51 — Lane D / #3)
            // Owner: the enemy-projectile phase. After the seek step so a shot is
            // aimed from where the enemy actually is this frame, and before
            // movement so a new shot moves on the frame it is born.
            {
                // Function-local static: the system owns no per-run state (its
                // timers all live on EnemyBehavior), and this keeps every line
                // Lane D adds to main.cpp inside a hook block. D66.
                static EnemyFireSystem enemy_fire;
                enemy_fire.set_config(&config);
                enemy_fire.update(component_storage, entity_manager, blackboard);
            }
            // === END HOOK: enemy-fire ===

            // === HOOK: pattern === (Engine suite, D148 — Lane Y / #2)
            // Owner: the bullet-pattern interpreter. Beside enemy-fire because it
            // is the same moment — "what does this enemy shoot" — dispatched when
            // an EnemyType names a pattern. Spawns through the existing EnemyShot
            // path; no RNG in the interpreter, variation is authored.
            bullet_pattern::tick(component_storage, entity_manager, blackboard, config,
                                 static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));
            // === END HOOK: pattern ===

            // === HOOK: specialty === (Iteration 3, D51 — Lane D / #9)
            // Owner: the per-arena specialty-unit phase (spitter trails, mines,
            // bulwark facing, splitter). Immediately after enemy-fire because both
            // are "what this enemy does beyond seeking", driven by EnemyBehavior.
            {
                static SpecialtySystem specialty;
                specialty.set_config(&config);
                specialty.update(component_storage, entity_manager, blackboard);
            }
            // === END HOOK: specialty ===

            // === HOOK: forces === (Engine suite, D144 — Lane T / #3)
            // Owner: the force-field layer. One accumulation pass over the
            // registered sources writing velocity deltas, BEFORE movement so a
            // field acts on the frame it exists — and the arena clamp + obstacle
            // push-out below stay the last word on position.
            //
            // Inert by shape: with nothing registered the pass iterates nothing,
            // which is why there is no `enabled` flag to check here. The system
            // is exposed to the rest of the frame through `forces` (declared at
            // outer scope) so Lane X's surges can register a gravity storm.
            forces.set_capacity(config.forces.max_sources);
            forces.update(component_storage,
                          static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));
            // === END HOOK: forces ===

            movement.update(component_storage, blackboard);

            // === HOOK: arena mechanics === (roguelite phase 5, design §4)
            // The Drift's current, applied to the drone, the enemies and the
            // loot alike. HERE, after movement and before the clamp: it
            // displaces Position because Velocity is overwritten every frame by
            // the seek and control systems, and running before the clamp is
            // what keeps the boundary wall the final authority on where
            // anything ends up.
            if (active_arena >= 0) {
                const ArenaDef& ad = config.arenas[static_cast<size_t>(active_arena)];
                arena_mechanics::tick_drift(
                    component_storage, ad.drift_x, ad.drift_y,
                    static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));
            }
            // === END HOOK: arena mechanics ===

            // Clamp the player inside the arena circle. Gameplay Phase 1: enemies
            // too — waves now advance only on a cleared arena, so an enemy drifting
            // outside the ring would stall the run (R3).
            //
            // v2 Upgrade Phase 4: enemies get the obstacle push-out as well, so
            // walls are solid whether A* or the line-of-sight fast path was
            // steering. Both helpers are defined once at outer scope.
            for (Entity p : component_storage.entities_with_component<PlayerTag>()) clamp_to_arena(p);
            for (Entity e : component_storage.entities_with_component<EnemyTag>()) clamp_to_arena(e);
            for (Entity p : component_storage.entities_with_component<PlayerTag>()) push_out_of_solids(p);
            for (Entity e : component_storage.entities_with_component<EnemyTag>()) push_out_of_solids(e);
            // Gameplay pack (D221) spec #5: the drone no longer phases through
            // enemies. A dash is the exception (tick_dash owns that contact and
            // the burst must plow through); the Owl's veil phases through by
            // design. Resolution is positional — push the drone out along the
            // radial with a small extra shove so ending a dash on top of an
            // enemy reads as a bounce, not a pin. Contact damage is unchanged
            // (CollidedWith still fires).
            for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                auto ship = component_storage.get_component<ShipState>(p);
                if (ship.has_value() && ship->get().dash_timer > 0.0f) break;
                if (blackboard.get_or<std::string>("ship.special", std::string()) == "phoenix_veil"
                    && blackboard.get_or<float>("ship.no_fire", 0.0f) > 0.0f) break;
                auto ppos = component_storage.get_component<Position>(p);
                auto psz  = component_storage.get_component<Size>(p);
                if (!ppos.has_value() || !psz.has_value()) break;
                const float pr = psz->get().width * 0.5f;
                for (Entity e : component_storage.entities_with_component<EnemyTag>()) {
                    if (component_storage.has_component<DestroyRequest>(e)) continue;
                    auto epos = component_storage.get_component<Position>(e);
                    auto esz  = component_storage.get_component<Size>(e);
                    if (!epos.has_value() || !esz.has_value()) continue;
                    const float er = esz->get().width * 0.5f;
                    const float pcx = ppos->get().x + pr, pcy = ppos->get().y + psz->get().height * 0.5f;
                    const float ecx = epos->get().x + er, ecy = epos->get().y + esz->get().height * 0.5f;
                    float dx = pcx - ecx, dy = pcy - ecy;
                    const float dist = std::sqrt(dx * dx + dy * dy);
                    const float min_d = pr + er;
                    if (dist >= min_d) continue;
                    if (dist < 0.001f) { dx = 1.0f; dy = 0.0f; }
                    else { dx /= dist; dy /= dist; }
                    // ponytail: overlap + 2px shove = the "small bounce"; spring it if playtests want more
                    const float push = (min_d - dist) + 2.0f;
                    ppos->get().x += dx * push;
                    ppos->get().y += dy * push;
                }
                break;   // one player
            }
            update_equipment_visuals();

            // Gameplay Phase 4: the Repulsor Field runs after the arena clamp and
            // the obstacle push-out, so "solid wall" still gets the last word on
            // where an enemy ends up; a shoved enemy is re-clamped next frame.
            items::repulse_enemies(component_storage, blackboard,
                                   config.shop.repulsor_radius,
                                   static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));

            // Q spends the held consumable. Nothing to do if the slot is empty.
            if (q_edge) items::use_consumable(component_storage, entity_manager,
                                              blackboard, config.shop);

            // === HOOK: actives === (Iteration 3, D51 — Lane D / boss rewards)
            // Owner: the boss-reward active items (missiles / laser sweep /
            // repulsion field). Beside use_consumable because it is the same kind
            // of moment — a player-triggered one-shot — and the repulsion field
            // reuses items::repulse_enemies, which has already run this frame.
            {
                // E fires the aimed actives; the repulsion device is not on a key
                // at all (it auto-triggers below 20% hull). The edge is a
                // function-local static rather than another `*_prev` beside the
                // others at the top of the loop, which are outside this hook.
                // ponytail: no `--keys E` alias — the scripted-key parser is not
                // this lane's to edit, so the actives are covered by unit tests
                // rather than by a headless script. See the report.
                static bool e_prev = false;
                const bool e_now = keys[SDL_SCANCODE_E];
                const bool e_edge = e_now && !e_prev;
                e_prev = e_now;
                actives::tick(component_storage, entity_manager, blackboard,
                              config, e_edge);
            }
            // === END HOOK: actives ===

            player_fire.update(component_storage, entity_manager, blackboard);
            collision.update(component_storage, blackboard);
            projectile_hit.update(entity_manager, component_storage, blackboard);

            // === HOOK: crumble === (Engine suite, D146 — Lane U / #9)
            // Owner: the destructible arena. The damage itself already landed:
            // ProjectileHitSystem emits a DamageEvent against a solid that carries
            // Health, and DamageApplySystem resolves it below. This resolves what
            // that leaves behind — cracked shading, debris, collider removal, and
            // the A* rebuild.
            //
            // The rebuild is HERE rather than inside the system so there is
            // exactly one place in the codebase that knows how the grid is built
            // (the arena-apply path above is the other caller of the same pair).
            if (crumble.update(component_storage, entity_manager, blackboard)) {
                enemy_seek.set_arena(&crumble.live_obstacles(),
                    enemy_path::build_obstacle_grid(path_cols, path_rows, path_cell,
                        crumble.live_obstacles(), config.pathfinding.clearance));
            }
            // === END HOOK: crumble ===

            // Gameplay pack (D221): ship specials (Owl's phoenix veil + the
            // shared no-fire countdown) tick just before shields/damage so a
            // sub-10% frame grants its i-frames before the hit resolves.
            tick_ship_specials(component_storage, blackboard,
                static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));
            // Gameplay pack (D221) tier 3: the right-mouse secondary and its
            // status effects, on the same pre-damage edge.
            tick_secondary_fire(component_storage, entity_manager, blackboard,
                static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));
            tick_burns_and_chills(component_storage, entity_manager,
                static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));
            // Shields regen *before* damage resolves, so a hit this frame restarts
            // the quiet timer with the last word (Phase 3).
            tick_shields(component_storage,
                         static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));
            // Same shape, same place (D29): one countdown on ShipState.
            tick_buff(component_storage,
                      static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));
            player_damage.update(entity_manager, component_storage, blackboard);
            damage_apply.update(entity_manager, component_storage);
            enemy_death.update(component_storage, entity_manager, blackboard);
            pickups.update(component_storage, entity_manager, blackboard);

            // === HOOK: sustain-spawn === (Iteration 3, D51 — Lane B / #10)
            // Owner: the periodic health/shield pickup phase. After pickups.update
            // so a placement made this frame cannot be collected on the same frame
            // it appears, which would make the interval read as random.
            sustain_spawn(component_storage, entity_manager, blackboard,
                          config.sustain, config.arena, config.economy);
            // === END HOOK: sustain-spawn ===

            lifetime.update(component_storage, blackboard);
            animation.update(component_storage, blackboard);
            // v2 Phase 5b: the tie-dye arena's enemies hue-cycle. Runs *before*
            // the flash tick so a hit flash written this frame still wins for its
            // 0.12s and then fades back to the hue just cycled to.
            if (active_arena >= 0 && config.arenas[static_cast<size_t>(active_arena)].tie_dye) {
                tick_enemy_tint(component_storage,
                                static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)),
                                TIE_DYE_CYCLE_SECONDS);
            }
            // The Shroud's darkness, beside the tie-dye cycle and for the same
            // reason: it writes Tint, and a hit flash written this frame must
            // still win. tick_shroud skips a flashing enemy outright — being lit
            // up when you hit it is the one thing darkness must not eat.
            if (active_arena >= 0) {
                arena_mechanics::tick_shroud(
                    component_storage,
                    config.arenas[static_cast<size_t>(active_arena)].light_radius);
            }
            flash_system.update(component_storage, blackboard);  // v2: tick hit flashes -> Tint
            destroy_marked_entities(entity_manager, component_storage);

            // Win/lose detection.
            bool player_alive = false;
            for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                auto h = component_storage.get_component<Health>(p);
                if (h.has_value() && h->get().current > 0.0f) player_alive = true;
            }
            // D2: the shop opens on a cleared multiple-of-4 wave, or on demand when
            // the player spends a key. Loss and victory outrank both.
            bool key_entry = false;
            if (b_edge && player_alive) {
                for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                    if (auto s = component_storage.get_component<ShipState>(p);
                        s.has_value() && s->get().keys > 0) {
                        s->get().keys -= 1;
                        key_entry = true;
                    }
                    break;
                }
            }
            // --dev: B raises the same prompt with no key spent. Same transition,
            // not a second way in.
            if (opts.dev && b_edge && player_alive) key_entry = true;

            if (!player_alive) {
                phase = PHASE_GAMEOVER;
                // Telemetry: where and to what the drone died, captured before the
                // bank so the entity is still around to read a position off.
                tm.died = true;
                tm.death_wave = blackboard.get_or<int>("wave", 0);
                tm.killed_by = blackboard.get_or<std::string>("tm.last_hit_by", std::string());
                for (Entity p : component_storage.entities_with_component<PlayerTag>())
                    if (auto pos = component_storage.get_component<Position>(p); pos.has_value()) {
                        tm.death_x = pos->get().x; tm.death_y = pos->get().y;
                        // Same centre-of-sprite convention the heat sampler uses.
                        // Position is the top-left corner, so without this the
                        // death bin lands a cell or two off the occupancy grid it
                        // is meant to be read against.
                        if (auto sz = component_storage.get_component<Size>(p); sz.has_value()) {
                            tm.death_x += sz->get().width * 0.5f;
                            tm.death_y += sz->get().height * 0.5f;
                        }
                    }
                tm.death_bin = telemetry::heat_bin(tm.death_x, tm.death_y,
                                                   config.arena.center_x,
                                                   config.arena.center_y,
                                                   config.arena.radius);
                bank_run_score(blackboard, "death");   // Lane F: the run ended, so it counts
                end_saved_run();              // Lane K: and a dead run is not resumable
                // Tier 6 (D221): the flight report, over the end banner.
                blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH,
                                            std::string(SCREEN_RUN_STATS));
                run_stats_up = true;
            } else if (wave_spawner.all_complete() &&
                       component_storage.entities_with_component<EnemyTag>().empty()) {
                phase = PHASE_VICTORY;
                bank_run_score(blackboard, "victory");
                end_saved_run();
                // Tier 6 (D221): the flight report first; the prestige offer
                // waits for it (the want_offer gate below).
                blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH,
                                            std::string(SCREEN_RUN_STATS));
                run_stats_up = true;
            } else if (shop_due || key_entry) {
                // The shop no longer opens itself. The same trigger now raises the
                // between-waves prompt, and the player picks: shop, or push on.
                blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH,
                                            std::string(SCREEN_INTERMISSION));
                phase = PHASE_INTERMISSION;
            }
        } else if (phase == PHASE_INTERMISSION && sim) {
            // The wave is cleared and the spawner is held off until the player
            // chooses — but the drone still FLIES. Freezing it here (which is what
            // the shop does, and what this block used to copy) stranded every
            // credit the last kill dropped: the loot is on the floor, the player
            // can see it, and nothing can reach it before the run moves on.
            //
            // So this runs the movement half of a normal frame and none of the
            // combat half: no spawner, no enemy seek, no firing, no damage. There
            // are no enemies alive on a cleared wave, so there is nothing to fight
            // anyway — this is a victory lap with a menu open over it.
            apply_move_speed();
            tick_hazard_glow(static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));
            player_control.update(component_storage);
            player_aim.update(component_storage, blackboard);
            movement.update(component_storage, blackboard);
            for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                clamp_to_arena(p);
                push_out_of_solids(p);
            }
            update_equipment_visuals();
            pickups.update(component_storage, entity_manager, blackboard);
            // Lifetimes tick here too, so loot still expires on its normal 12s
            // timer rather than waiting politely forever, and so the one-shot
            // particle hosts retire instead of emitting into the 2000 budget for
            // as long as the prompt is up (see ENGINE.md §5).
            lifetime.update(component_storage, blackboard);
            flash_system.update(component_storage, blackboard);
            //
            // Both buttons are read from the Blackboard rather than through Lua:
            // UISystem publishes a confirmed click's on_click_fn under
            // UI_CLICK_KEY, and this game carries no Lua menu layer.
            const std::string click =
                blackboard.get_or<std::string>(UISystem::UI_CLICK_KEY, std::string());
            // B is the same key that used to open and close the shop, kept as the
            // keyboard path so headless --keys scripts can still drive the choice.
            const bool want_shop     = (click == "on_shop_open_click") || b_edge;
            const bool want_continue = (click == "on_continue_click");
            if (want_shop || want_continue) {
                // Consume the click so it cannot re-fire on the next frame or
                // leak into the shop's own input handling.
                blackboard.remove(UISystem::UI_CLICK_KEY);
                blackboard.set<bool>(ScreenStackSystem::CMD_POP, true);
                if (want_shop) {
                    shop.open(component_storage, entity_manager, blackboard);
                    phase = PHASE_SHOP;
                } else {
                    phase = PHASE_PLAYING;
                }
            }
            animation.update(component_storage, blackboard);
            destroy_marked_entities(entity_manager, component_storage);
        } else if (phase == PHASE_SHOP && sim) {
            // The arena is frozen while the shop is open — no spawner, no movement,
            // no damage. Purchases and rendering are ShopSystem's (R7).
            // Only B closes the shop: SPACE is the fire key, and a player holding it
            // when the wave cleared would never see the shop at all.
            if (shop.update(component_storage, blackboard, digit, b_edge, tab_edge)) {
                shop.close(component_storage);
                phase = PHASE_PLAYING;
            }

            // === HOOK: shop-menu === (Iteration 3, D51 — Lane C / #1, #11)
            // Owner: the clickable-shop + gear-upgrade phase. The widget shop reads
            // UISystem::UI_CLICK_KEY, which was published earlier this frame by
            // ui_system.update, and must consume the key so it cannot re-fire.
            // The 1-8 keyboard path above stays as the headless fallback.
            if (shop.menu_tick(component_storage, entity_manager, blackboard)) {
                shop.close(component_storage);
                phase = PHASE_PLAYING;
            }
            // === END HOOK: shop-menu ===

            animation.update(component_storage, blackboard);
            destroy_marked_entities(entity_manager, component_storage);
        } else if (sim) {
            // Title / game-over / victory: keep effects animating, nothing else.
            animation.update(component_storage, blackboard);
            destroy_marked_entities(entity_manager, component_storage);

            if (phase == PHASE_TITLE) {
                // Main-menu-suite Phase A: hub (PLAY / CONTINUE / QUIT) plus the
                // run_setup screen (difficulty tabs + ship + LAUNCH). SPACE — not
                // `advance` — remains the quick-start fallback, because `advance`
                // also fires on the very mouse click that pressed a button. SPACE
                // always means "Normal run, now", from either screen: it is the
                // replay canary's documented entry path and keeps its meaning.
                const std::string menu_click =
                    blackboard.get_or<std::string>(UISystem::UI_CLICK_KEY, std::string());
                bool launch = false;      // fresh start via LAUNCH or SPACE
                size_t launch_difficulty = 0;
                bool resumed = false;
                // Task 7: N re-enters name_entry pre-filled with the current name
                // (blank if never registered) — the server upserts by player_id,
                // so a rename rides the exact same submit path as first-launch.
                if (l_edge && net::enabled()) {
                    // Task 9: unreachable in headless/scripted runs — net::enabled()
                    // is false whenever --stopframe is set (architecture.md
                    // Invariant #4), so this whole branch, and everything it
                    // fetches, is never entered by a replay.
                    fetch_leaderboard("high");
                    phase = PHASE_LEADERBOARD;
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_LEADERBOARD));
                } else if (n_edge) {
                    name_buf = meta.player_name;
                    name_entry_error.clear();
                    // Critical 1: a stale future from an abandoned previous
                    // attempt (e.g. ESC'd out mid-submit earlier) must never
                    // block this new submit's `!pending_register.valid()`
                    // guard, and its eventual result must never be applied
                    // here. Hand it to the graveyard so its destructor never
                    // blocks this thread either (Critical 2).
                    abandon_future(std::move(pending_register));
                    pending_name.clear();
                    email_buf.clear();
                    email_focus = false;
                    phase = PHASE_NAME_ENTRY;
                    SDL_StartTextInput(window.get());
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_NAME_ENTRY));
                } else if (menu_click == "on_play_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_RUN_SETUP));
                } else if (menu_click == "on_back_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_MAIN_MENU));
                } else if (menu_click == "on_menu_quit_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    running = false;      // nothing to bank at the title
                } else if (menu_click == "on_feedback_click" && net::enabled()) {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    fb_from_pause = false;
                    fb_ctx = {{"in_run", false}};
                    fb_focus = 0; fb_msg.clear();
                    phase = PHASE_FEEDBACK;
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_FEEDBACK));
                    SDL_StartTextInput(window.get());
                } else if (menu_click == "on_pick_normal") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    setup_difficulty = 0;
                } else if (menu_click == "on_pick_hard") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    setup_difficulty = 1;
                } else if (menu_click == "on_launch_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    launch = true;
                    launch_difficulty = static_cast<size_t>(setup_difficulty);
                } else if (menu_click == "on_ship_cycle") {
                    // Lane F (D82) / gameplay pack (D221): cycle to the next
                    // *owned* ship — an unowned or locked one is never landed on.
                    // The choice persists as the equipped loadout.
                    selected_ship = next_owned_ship(config.ships, selected_ship, meta);
                    if (selected_ship >= 0 && selected_ship < static_cast<int>(config.ships.size())) {
                        meta.equipped_ship = config.ships[static_cast<size_t>(selected_ship)].name;
                        meta.equipped_weapon.clear();  // ship default until a weapon is picked
                        meta_write(meta_save_path(), meta);
                    }
                    reskin_player();   // tier 6: the world drone is the preview
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                } else if (menu_click == "on_weapon_cycle") {
                    // Tier 6 (D221): cycle through OWNED weapons only.
                    const int cur = current_weapon_index();
                    const int n = static_cast<int>(config.weapons.size());
                    for (int step = 1; step <= n && n > 0; ++step) {
                        const int i = (cur + step) % n;
                        if (!weapon_owned(meta, config.ships, config.weapons[static_cast<size_t>(i)].name))
                            continue;
                        meta.equipped_weapon = config.weapons[static_cast<size_t>(i)].name;
                        meta_write(meta_save_path(), meta);
                        break;
                    }
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                } else if (menu_click == "on_buy_ship") {
                    // Tier 6 (D221): drones are bought with scrap, here and only
                    // here. Equips on purchase; colors/weapon derive (D222).
                    const int buy = next_purchasable_ship();
                    if (buy >= 0 &&
                        meta.scrap >= config.ships[static_cast<size_t>(buy)].scrap_cost) {
                        meta.scrap -= config.ships[static_cast<size_t>(buy)].scrap_cost;
                        meta.owned_ships.push_back(config.ships[static_cast<size_t>(buy)].name);
                        meta.equipped_ship = config.ships[static_cast<size_t>(buy)].name;
                        meta.equipped_weapon.clear();
                        selected_ship = buy;
                        meta_write(meta_save_path(), meta);
                        reskin_player();
                    }
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                } else if (menu_click == "on_continue_run_click" && newest_slot() >= 0) {
                    // Lane K (D100): CONTINUE resumes the newest saved run. Handled
                    // apart from the fresh-start branch so the two can never both fire.
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    active_slot = newest_slot();
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string());
                    start_run(static_cast<size_t>(saved_slots[active_slot].difficulty),
                              &saved_slots[active_slot]);
                    resumed = true;
                } else if (menu_click == "on_continue_run_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);   // no save: an inert widget
                } else if (menu_click == "on_slots_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_SAVE_SLOTS));
                } else if (menu_click == "on_settings_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    sync_settings_widgets();
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_SETTINGS));
                } else if (menu_click == "on_records_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    refresh_records();
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_RECORDS));
                } else if (menu_click == "on_update_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    if (update_ready && !update_busy) {
#ifdef _WIN32
                        // Windows is the only platform that can genuinely
                        // self-update: the installer replaces {app} while the
                        // game is closed, and user data lives in prefpath, so
                        // saves are untouched by design (project_paths, D194).
                        update_file = project_paths::user_data_dir() +
                                      "/ReactorDrone-Setup-" + update_version + ".exe";
                        pending_download = net::download(update_url, update_file);
                        update_busy = true;
                        update_note = "Downloading update...";
#else
                        // A running ELF/.app cannot safely overwrite itself, and
                        // itch already updates those channels — so off Windows
                        // the button just points the player at the download.
                        // The WEBSITE, not the raw GitHub release: the site is
                        // the front door people are meant to see. (GitHub is
                        // still where the bytes live — the site hosts no files —
                        // so the Windows installer fetch below pins the release
                        // asset host, not this page.)
                        SDL_OpenURL("https://thebrainstormlabs.com/reactor-drone/");
                        update_note = "Opened the download page";
#endif
                        refresh_update_widget();
                    }
                } else if (menu_click == "on_how_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_HOW));
                } else if (menu_click == "on_toggle_shake" ||
                           menu_click == "on_toggle_minimap" ||
                           menu_click == "on_toggle_analytics" ||
                           menu_click == "on_toggle_fullscreen") {
                    // UISystem already flipped the checkbox's UIState.value — read
                    // it back as the truth, persist, and publish to the apply sites.
                    // MERGE 2026-08-15: four rows now, from both branches. They do
                    // not share a shape — screen_shake and minimap publish a
                    // Blackboard key, analytics has no apply site (the telemetry
                    // POST guard reads the settings struct directly), and
                    // fullscreen applies straight to the window — so each row
                    // names what it needs and the common tail does the rest.
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    Entity w = 0;
                    bool* target = nullptr;
                    const char* bb_key = nullptr;
                    bool apply_fullscreen = false;
                    if (menu_click == "on_toggle_shake") {
                        w = widget_by_name("settings_shake", shake_w, shake_w_resolved);
                        target = &settings.screen_shake; bb_key = "settings.screen_shake";
                    } else if (menu_click == "on_toggle_minimap") {
                        w = widget_by_name("settings_minimap", mini_w, mini_w_resolved);
                        target = &settings.minimap; bb_key = "settings.minimap";
                    } else if (menu_click == "on_toggle_analytics") {
                        w = widget_by_name("settings_analytics", analytics_w,
                                           analytics_w_resolved);
                        target = &settings.analytics;
                    } else {
                        w = widget_by_name("settings_fullscreen", fs_w, fs_w_resolved);
                        target = &settings.fullscreen; apply_fullscreen = true;
                    }
                    if (auto st = component_storage.get_component<UIState>(w);
                        st.has_value()) {
                        const bool on = st->get().value >= 0.5f;
                        *target = on;
                        if (bb_key != nullptr) blackboard.set<bool>(bb_key, on);
                        if (apply_fullscreen) SDL_SetWindowFullscreen(window.get(), on);
                        settings_write(settings_save_path(), settings);
                    }
                } else if (menu_click.rfind("on_slot_load_", 0) == 0) {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    const int i = menu_click.back() - '0';
                    if (i >= 0 && i < RUN_SAVE_SLOTS && saved_slots[i].present) {
                        active_slot = i;
                        blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                    std::string());
                        start_run(static_cast<size_t>(saved_slots[i].difficulty),
                                  &saved_slots[i]);
                        resumed = true;
                    }
                } else if (menu_click.rfind("on_slot_del_", 0) == 0) {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    const int i = menu_click.back() - '0';
                    if (i >= 0 && i < RUN_SAVE_SLOTS) {
                        run_save_clear(run_save_path(i + 1));
                        saved_slots[i] = RunSave{};
                    }
                }
                refresh_ship_widget();
                refresh_hangar();
                refresh_continue_widget();
                refresh_difficulty_tabs();
                refresh_save_slots();
                if (space_edge && !launch && !resumed) {
                    launch = true;        // SPACE: quick-start Normal from anywhere
                    launch_difficulty = 0;
                }
                if (launch && !resumed) {
                    // CLEAR_TO(""), not POP: title screens replace each other, and
                    // SPACE can fire from any of them — all must land on the bare
                    // gameplay base. A fresh run claims the first empty slot so a
                    // pause-SAVE never silently overwrites an older run.
                    active_slot = first_free_slot();
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string());
                    start_run(launch_difficulty);
                }
            } else if (phase == PHASE_NAME_ENTRY) {
                // Task 7: first-launch / rename pilot-name registration. Typed
                // text and BACKSPACE come off the Blackboard, published this
                // frame by InputSystem between SDL_StartTextInput/StopTextInput.
                const std::string typed =
                    blackboard.get_or<std::string>("ui.text_input", std::string());
                // TAB moves the caret between pilot name and email; both share
                // one text-input stream, so the focus flag alone decides where
                // this frame's characters land.
                if (tab_edge) email_focus = !email_focus;
                std::string& field = email_focus ? email_buf : name_buf;
                const size_t field_max = email_focus ? 64u : 24u;
                for (char c : typed) {
                    if (field.size() >= field_max) break;
                    // Printable ASCII only (space..~) — filters control bytes and
                    // any non-ASCII UTF-8 lead/continuation bytes from an IME.
                    if (c >= 0x20 && c < 0x7F) field.push_back(c);
                }
                if (blackboard.get_or<bool>("ui.backspace_pressed", false) && !field.empty())
                    field.pop_back();

                // Poll last frame's submit before reading escape/enter this frame,
                // so a response that lands the same frame as ENTER never races it.
                if (pending_register.valid() &&
                    pending_register.wait_for(std::chrono::seconds(0)) ==
                        std::future_status::ready) {
                    const net::Response resp = pending_register.get();
                    if (resp.status == 200) {
                        // Critical 1: apply the name that was actually POSTed
                        // (`pending_name`), never the live `name_buf` — the
                        // player may have edited it while this was in flight.
                        meta.player_name = pending_name;
                        meta.registered = true;
                        meta_write(meta_save_path(), meta);
                        // Mailing list: signup rides the confirmed registration,
                        // so a player who ESCs out never gets subscribed. The
                        // reply is deliberately never read — the server dedupes
                        // by address, and a failed signup must not hold up the
                        // title screen. Only the '@' is checked here; real
                        // validation is the server's job.
                        if (email_buf.find('@') != std::string::npos) {
                            abandon_future(std::move(pending_subscribe));
                            pending_subscribe = net::post_json(
                                std::string(net::NET_BASE) + "/subscribe",
                                nlohmann::json{{"email", email_buf},
                                               {"source", "game"}}.dump());
                            email_buf.clear();
                        }
                        SDL_StopTextInput(window.get());
                        phase = PHASE_TITLE;
                        blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                    std::string(SCREEN_MAIN_MENU));
                    } else if (resp.status == 409) {
                        name_entry_error = "Name taken - try another";
                    } else {
                        name_entry_error = "Offline - ESC to skip";
                    }
                }
                if (phase == PHASE_NAME_ENTRY &&
                    blackboard.get_or<bool>("ui.escape_pressed", false)) {
                    // Skip: registered stays false, so this retries next launch.
                    // Critical 1/2: abandon (don't block-wait) any submit still
                    // in flight for this attempt — its result must never be
                    // applied once the player has walked away, and its future
                    // must never stall this thread waiting on it.
                    abandon_future(std::move(pending_register));
                    pending_name.clear();
                    SDL_StopTextInput(window.get());
                    phase = PHASE_TITLE;
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_MAIN_MENU));
                } else if (phase == PHASE_NAME_ENTRY &&
                           blackboard.get_or<bool>("ui.enter_pressed", false) &&
                           !pending_register.valid() && !name_buf.empty()) {
                    // Minor 3: trim before submit — an all-whitespace name is
                    // printable ASCII and would otherwise pass this guard,
                    // relying entirely on the server to reject it.
                    const size_t first = name_buf.find_first_not_of(' ');
                    if (first != std::string::npos) {
                        const size_t last = name_buf.find_last_not_of(' ');
                        const std::string trimmed = name_buf.substr(first, last - first + 1);
                        name_entry_error.clear();
                        pending_name = trimmed;
                        pending_register = net::post_json(
                            std::string(net::NET_BASE) + "/register",
                            nlohmann::json{{"player_id", meta.player_id},
                                          {"name", trimmed}}.dump());
                    } else {
                        name_entry_error = "Name can't be blank";
                    }
                }

                if (phase == PHASE_NAME_ENTRY) {
                    if (Entity w = widget_by_name("name_entry_buffer", name_buf_w,
                                                  name_buf_w_resolved);
                        w != 0) {
                        if (auto el = component_storage.get_component<UIElement>(w);
                            el.has_value())
                            el->get().label_text = name_buf + (email_focus ? "" : "_");
                    }
                    if (Entity w = widget_by_name("name_entry_email", name_email_w,
                                                  name_email_w_resolved);
                        w != 0) {
                        if (auto el = component_storage.get_component<UIElement>(w);
                            el.has_value())
                            el->get().label_text = email_buf + (email_focus ? "_" : "");
                    }
                    if (Entity w = widget_by_name("name_entry_msg", name_msg_w,
                                                  name_msg_w_resolved);
                        w != 0) {
                        if (auto el = component_storage.get_component<UIElement>(w);
                            el.has_value()) {
                            el->get().label_text =
                                !name_entry_error.empty() ? name_entry_error
                                : pending_register.valid()
                                    ? "Submitting..."
                                    : "TAB next field  \xc2\xb7  ENTER confirm  \xc2\xb7  ESC to skip";
                            // Minor 2: an error reuses the existing red-toned
                            // `pip_loss` style (shop stat-loss preview) rather
                            // than a new literal colour — house rule against
                            // inventing new colours. Hint/status text stays
                            // `caption`, same dim grey as every other screen.
                            el->get().style_id =
                                !name_entry_error.empty() ? "pip_loss" : "caption";
                        }
                    }
                }
            } else if (phase == PHASE_LEADERBOARD) {
                // Task 9: Highest/Cumulative global leaderboard. TAB toggles mode,
                // ESC returns to the title — handled here directly rather than by
                // the generic ESC handler above, same reasoning as PHASE_NAME_ENTRY
                // (Minor 1, Task 7 review round 1): letting both fire the same frame
                // would push/pop the stack a frame ahead of this CLEAR_TO.
                if (blackboard.get_or<bool>("ui.escape_pressed", false)) {
                    abandon_future(std::move(pending_top));
                    phase = PHASE_TITLE;
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_MAIN_MENU));
                } else if (tab_edge) {
                    fetch_leaderboard(lb_mode == "high" ? "total" : "high");
                }
                // menu_click is only ever set by an actual confirmed click, and
                // only the phase==PHASE_TITLE branch above consumes it — this
                // phase must consume its own BACK button here or the click would
                // sit stale on the Blackboard and fire again once back at title.
                const std::string menu_click =
                    blackboard.get_or<std::string>(UISystem::UI_CLICK_KEY, std::string());
                if (menu_click == "on_back_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    if (phase == PHASE_LEADERBOARD) {
                        abandon_future(std::move(pending_top));
                        phase = PHASE_TITLE;
                        blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                    std::string(SCREEN_MAIN_MENU));
                    }
                }

                if (phase == PHASE_LEADERBOARD) {
                    if (Entity w = widget_by_name("lb_title", lb_title_w, lb_title_w_resolved);
                        w != 0) {
                        if (auto el = component_storage.get_component<UIElement>(w);
                            el.has_value())
                            el->get().label_text = lb_mode == "total"
                                ? "LEADERBOARD \xe2\x80\x94 CUMULATIVE"
                                : "LEADERBOARD \xe2\x80\x94 HIGHEST";
                    }
                    // Split lb_rows into its "N. name  score\n" lines, one per row
                    // widget. loading/error/empty each collapse to a single status
                    // line in row 0 with the rest blanked — a fresh/empty
                    // leaderboard must show something sensible, not a blank panel.
                    std::vector<std::string> lines;
                    if (lb_status == "loading") {
                        lines.push_back("Loading...");
                    } else if (lb_status == "error") {
                        lines.push_back("Could not reach the leaderboard.");
                    } else {
                        size_t start = 0;
                        while (start < lb_rows.size()) {
                            size_t nl = lb_rows.find('\n', start);
                            if (nl == std::string::npos) nl = lb_rows.size();
                            if (nl > start) lines.push_back(lb_rows.substr(start, nl - start));
                            start = nl + 1;
                        }
                        if (lines.empty()) lines.push_back("No scores yet.");
                    }
                    for (int i = 0; i < 20; ++i) {
                        const Entity w = widget_by_name(("lb_row_" + std::to_string(i)).c_str(),
                                                        lb_row_w[i], lb_row_w_resolved[i]);
                        if (w == 0) continue;
                        auto el = component_storage.get_component<UIElement>(w);
                        if (!el.has_value()) continue;
                        if (static_cast<size_t>(i) < lines.size()) {
                            const std::string& line = lines[static_cast<size_t>(i)];
                            el->get().label_text = line;
                            // Design note: the player's own row stands out. A row
                            // reads "N. name  score" — pull the name back out and
                            // compare; registered names are unique (server-
                            // enforced by /register), so an exact match is
                            // unambiguous.
                            const size_t dot = line.find(". ");
                            const size_t sep = (dot == std::string::npos) ? std::string::npos
                                                                          : line.rfind("  ");
                            const bool is_me = !meta.player_name.empty() &&
                                dot != std::string::npos && sep != std::string::npos &&
                                sep > dot + 2 &&
                                line.substr(dot + 2, sep - (dot + 2)) == meta.player_name;
                            el->get().style_id = is_me ? "title" : "subtitle";
                        } else {
                            el->get().label_text.clear();
                            el->get().style_id = "subtitle";
                        }
                    }
                }
            } else if (advance && !run_stats_up &&
                       (phase == PHASE_GAMEOVER || phase == PHASE_VICTORY)) {
                // Restart keeps the difficulty the run was started at — `config`
                // is already scaled, so only the world is rebuilt.
                run_banked = false;   // Lane F: a new run to bank at its end
                // Telemetry: same fresh-report reset as start_run's copy. The
                // difficulty is not re-chosen here, so it is read back rather
                // than recomputed from the picker.
                tm = telemetry::RunReport{};
                tm.game_version = GAME_VERSION;
                tm.player_id = meta.player_id;
                tm.session_id = session_id;
                tm.seed = config.seed;
                tm.ship = selected_ship;
                tm.prestige = meta.prestige;
                tm.difficulty = blackboard.get_or<std::string>("difficulty", "Normal");
                prev_consumable = -1;
                for (const char* k : {"tm.shots", "tm.hits", "tm.dashes", "tm.bombs"})
                    blackboard.set<double>(k, 0.0);
                blackboard.set<std::string>("tm.last_hit_by", "");
                // Task 8: same reasoning as start_run's copy of this pair — clear
                // the on-screen status and abandon (never await) a submit still
                // in flight from the run that just ended.
                abandon_future(std::move(pending_score));
                blackboard.remove("score_submit_status");
                spawn_world();
                phase = PHASE_PLAYING;
            }
        }
        if (step_requested) apply_step_complete(step_requested, debug_paused);
        blackboard.set("phase", phase);

        // v2: advance the particle simulation every frame in ALL phases (so trails
        // and death bursts keep animating on the title / game-over / victory
        // screens), then sweep the particles that expired this frame.
        //
        // Emission happens in the two phases that fly the drone — PHASE_PLAYING and
        // PHASE_INTERMISSION — so the thruster plume keeps up while the player
        // collects loot under the prompt. Both of those phases also run
        // lifetime.update; outside them the one-shot FX hosts (death bursts,
        // pickup pops, the arena shockwave) never expire — left emitting, they
        // saturated the 2000-particle budget for the entire duration of a shop
        // visit, which silently starved every other effect. Ageing without
        // emitting is exactly what "keep animating" was meant to mean.
        if (sim) {
            float pdt = static_cast<float>(blackboard.get_or<double>("delta_time", 0.0));
            particles.update(component_storage, entity_manager, pdt,
                             /*emit=*/phase == PHASE_PLAYING ||
                                      phase == PHASE_INTERMISSION);
            destroy_marked_entities(entity_manager, component_storage);
        }

        // Tick the transient HUD-message timer.
        float dt = static_cast<float>(blackboard.get_or<double>("delta_time", 0.0));
        float mt = blackboard.get_or<float>("hud_message_timer", 0.0f);
        if (mt > 0.0f) blackboard.set<float>("hud_message_timer", std::max(0.0f, mt - dt));

        game_hud.update(component_storage, blackboard);

        // === HOOK: telemetry === (Engine suite, D143 — Lane S / #10)
        // Owner: the Flight Report recorder. Every phase, outside the `sim` gate,
        // so the terminal screens keep drawing the report while the sim is
        // stopped; recording itself is gated on PHASE_PLAYING inside the system.
        // Passive — reads sim state into its own ring buffers, nothing reads back.
        {
            static FlightReport flight_report;   // ring buffers + widget pool, once
            flight_report.set_config(config.flight_report, config.arena);
            if (blackboard.get_or<bool>("flight_report.reset", false)) {
                flight_report.reset();
                blackboard.set<bool>("flight_report.reset", false);
            }
            flight_report.update(component_storage, entity_manager, blackboard);
        }
        // === END HOOK: telemetry ===

        // === HOOK: audio ===
        // EMPTY — deliberately. Chip-synth audio (#4, D150) is SHELVED, not
        // deleted (D151): engine/ecs/systems/chip_synth_system.* still builds and
        // is still tested, so it cannot bitrot, but nothing constructs it.
        // Re-wiring is this block plus one include; see D150 for what it does.
        // === END HOOK: audio ===

        // === HOOK: minimap === (Iteration 3, D51 — Lane B / #7)
        // Owner: the minimap phase. Beside game_hud.update because it is the same
        // kind of work — a per-frame refresh of screen-space furniture — and it
        // must run in every phase, not just PHASE_PLAYING, so the map does not
        // blank out during the intermission.
        // Note for the owner: blips must carry ScreenPosition and NOT Position, or
        // CameraSystem will overwrite them with a world-to-screen transform.
        //
        // Lane B correction (D58): the note above describes an entity that NOTHING
        // draws — RenderSystem::render iterates entities_with_component<Position>(),
        // so a ScreenPosition-only entity is never reached, and adding a Position
        // hands it straight back to CameraSystem. The blips are therefore pooled UI
        // widgets on this screen, the same mechanism the hull/shield gauges use.
        {
            static MinimapSystem minimap;   // pool is allocated once, per process
            minimap.set_config(config.minimap, config.arena);
            minimap.update(component_storage, entity_manager, blackboard);
        }
        // === END HOOK: minimap ===

        // === HOOK: ship-readout === (Iteration 5 — Lane M / #5, #13)
        // The pause screen's character sheet and the HUD's active-item slot, both
        // driven from one query of the player. Here, beside game_hud and minimap,
        // because it is the same kind of work — a per-frame refresh of screen-space
        // furniture — and it must run while the sim is frozen, which everything
        // inside the `sim` block above does not.
        {
            static PauseStatsSystem ship_readout;   // label pool allocated once
            ship_readout.update(component_storage, entity_manager, blackboard, config);
        }
        // === END HOOK: ship-readout ===

        // v2 Phase 4 + Upgrade Phase 2: follow camera & screen shake. Decay
        // trauma, then set look-at to the player's centre offset by
        // shake_amplitude(trauma) in a seeded-random direction. Recomputing from
        // the base each frame means no separate "restore" step. With no player
        // (title / game-over) the base falls back to the arena centre. Runs in
        // every simulated phase so a lingering shake keeps decaying into the
        // game-over/victory transition.
        if (sim) {
            postfx.update(static_cast<float>(
                blackboard.get_or<double>("delta_time", 0.0)));
            float trauma = feedback::decay_trauma(
                blackboard.get_or<float>("feedback.trauma", 0.0f), dt,
                config.feedback.trauma_decay_per_sec);
            blackboard.set<float>("feedback.trauma", trauma);

            // v3 Tier 3 (D209): trauma also punches the camera in. Same
            // trauma^2 curve as the shake; nothing else writes camera.zoom
            // (CameraControlSystem is not instantiated in this game), so a
            // plain per-frame recompute needs no restore step. Presentation
            // only: zoom feeds ScreenPosition/draw scale, never the sim.
            // Gated with the shake setting — it is the same "camera moves on
            // impact" preference.
            if (blackboard.get_or<bool>("settings.screen_shake", true)) {
                blackboard.set<float>("camera.zoom",
                    1.0f + config.feedback.zoom_punch * trauma * trauma);
            } else {
                blackboard.set<float>("camera.zoom", 1.0f);
            }

            float amp = feedback::shake_amplitude(trauma, config.feedback.max_shake_px);
            float ox = 0.0f, oy = 0.0f;
            if (amp > 0.0f) {
                // Main-menu-suite Phase C: the rng draw happens whether or not the
                // shake setting is on, so toggling it mid-run cannot shift a
                // single later draw — only the APPLIED offset is gated. (The
                // offset does still steer real-mouse aim through the camera, the
                // same way a window resize does; scripted --hover aim is world-
                // space and never sees it, so the canary cannot either.)
                float ang = shake_angle(shake_rng);
                if (blackboard.get_or<bool>("settings.screen_shake", true)) {
                    ox = std::cos(ang) * amp;
                    oy = std::sin(ang) * amp;
                }
            }
            float base_x = config.arena.center_x, base_y = config.arena.center_y;
            for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                auto pos = component_storage.get_component<Position>(p);
                auto sz = component_storage.get_component<Size>(p);
                if (!pos.has_value() || !sz.has_value()) continue;
                base_x = pos->get().x + sz->get().width * 0.5f;
                base_y = pos->get().y + sz->get().height * 0.5f;
                break;
            }
            blackboard.set<float>("camera.lookat.x", base_x + ox);
            blackboard.set<float>("camera.lookat.y", base_y + oy);
        }

        // === Render ===
        // === HOOK: palette === (Engine suite, D147 — Lane W / #5) — part 1 of 2
        // The palette engine needs TWO sites, for the same reason the prestige
        // hook does: the capture has to be armed before the first draw call and
        // resolved after the last one. Nothing else lives between them, so the
        // whole feature is still one contiguous idea and one revert.
        //
        // Off (the shipped default) begin_capture returns false and every draw
        // below goes straight to the backbuffer, on exactly the code path that
        // existed before the feature.
        static PaletteSystem palette;
        const bool palette_on = config.palettes.enabled &&
            palette.begin_capture(renderer.get(), win_w, win_h);
        // === END HOOK: palette ===

        camera.update(component_storage, blackboard);

        // #11: the dash button's face, parked over the authored "hud_dash_frame"
        // rect. HERE, after camera.update, because that is what owns
        // ScreenPosition — anything written earlier in the frame is overwritten
        // by the world-to-screen transform (the trap the minimap note calls out).
        // Overwriting ScreenPosition afterwards is enough: RenderSystem prefers
        // it over Position, so the Position these two carry only exists to get
        // them iterated at all.
        //
        // The dial is a whole-box clock wipe, not a bar: it greys the button out
        // on use and sweeps back to clear, so "can I dash?" is answered by the
        // button's own state instead of by a second strip of furniture inside it.
        {
            const double dial_id =
                blackboard.get_or<double>("ui.widget_id.hud_dash_frame", -1.0);
            const Entity frame_w = dial_id < 0.0 ? 0 : static_cast<Entity>(dial_id);
            // The widget's LIVE rect: GameHUDSystem collapses it to zero width in
            // every phase that hides the HUD, so the phase gate is already done.
            UIRect design{};
            if (frame_w != 0) {
                if (auto fel = component_storage.get_component<UIElement>(frame_w);
                    fel.has_value())
                    design = fel->get().rect;
            }
            const UIRect box = ui_apply_transform(
                ui_canvas_transform(static_cast<float>(win_w), static_cast<float>(win_h)),
                design);

            float dash_frac = 1.0f;
            for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                if (auto s = component_storage.get_component<ShipState>(p); s.has_value()) {
                    if (s->get().dash_charges <= 0 && config.dash.cooldown > 0.0f)
                        dash_frac = 1.0f - s->get().dash_cd / config.dash.cooldown;
                }
                break;
            }
            const bool row_on = box.w > 0.0f && box.h > 0.0f;
            auto park = [&](Entity e, bool on) {
                if (auto sz = component_storage.get_component<Size>(e); sz.has_value()) {
                    sz->get().width  = on ? box.w : 0.0f;
                    sz->get().height = on ? box.h : 0.0f;
                }
                if (on) component_storage.add_component<ScreenPosition>(
                            e, ScreenPosition{box.x, box.y});
            };
            park(dash_icon, row_on);
            // A charge in hand means READY: no dial at all, rather than a full one.
            park(dash_dial, row_on && dash_frac < 1.0f);
            if (auto ss = component_storage.get_component<SpriteSheet>(dash_dial);
                ss.has_value())
                ss->get().current_frame = dash_sweep_frame(dash_frac);
        }

        // v2 Phase 5: parallax backdrops. Camera displacement is camera.lookat
        // minus the arena centre — since Upgrade Phase 2 that is mostly the
        // follow camera tracking the drone, plus the seeded screen shake;
        // each layer is offset by parallax_offset(displacement, factor),
        // so far layers barely move and near layers shake the most. Render-only —
        // no RNG, no sim state — so deterministic replay is untouched.
        //
        // v2 Phase 5b: during an arena shift the outgoing set is pushed first at
        // full opacity and the incoming set over it at a rising alpha. Fading only
        // the *incoming* set (rather than crossfading both) means the screen never
        // dips toward black, since each set's first layer is opaque.
        std::vector<RenderSystem::TiledLayer> bg_layers;
        {
            float cam_dx = blackboard.get_or<float>("camera.lookat.x", config.arena.center_x)
                         - config.arena.center_x;
            float cam_dy = blackboard.get_or<float>("camera.lookat.y", config.arena.center_y)
                         - config.arena.center_y;
            auto push_backdrop = [&](const std::vector<BackdropLayer>* set, float alpha) {
                if (set == nullptr) return;
                for (const auto& bl : *set) {
                    bg_layers.push_back({bl.image,
                        parallax::parallax_offset(cam_dx, bl.scroll_factor),
                        parallax::parallax_offset(cam_dy, bl.scroll_factor),
                        alpha});
                }
            };
            push_backdrop(outgoing_backdrop, 1.0f);
            // Smoothstep rather than a linear ramp: a linear crossfade spends most
            // of its time in the muddy half-and-half middle, which is what made the
            // short version look like a glitch. This lingers at each end instead.
            // D76: the curve moved to arena_vfx.hpp so the props animate on
            // exactly the same easing as the backdrop, not a copy of it.
            push_backdrop(active_backdrop,
                          outgoing_backdrop != nullptr
                              ? arena_vfx::smoothstep(shift_timer / SHIFT_SECONDS)
                              : 1.0f);
        }
        // v3 Tier 1: everything between begin() and resolve() renders into the
        // bloom scene target; resolve() composites scene + blur chain onto the
        // backbuffer. UI is inside the pass on purpose — menus glow too. When
        // bloom is disabled (config or target-less driver) both are no-ops and
        // this block is byte-for-byte the old pipeline.
        // v3 Tier 5 (D211): the frame's neon lines, world-space, immediate
        // mode — rebuilt every frame from live state, so there is nothing to
        // invalidate on an arena shift. Drawn into the scene after the world
        // (below) and again into the emissive target so they bloom.
        std::vector<RenderSystem::GlowLine> glow_lines;
        if (phase == PHASE_PLAYING || phase == PHASE_INTERMISSION ||
            phase == PHASE_SHOP) {
            // Arena boundary ring — the rink line. Soft blue-white, like the
            // Laser Hockey rink; the clamp circle finally has a visible edge.
            RenderSystem::GlowLine ring;
            ring.points = line_mesh::circle_points(
                config.arena.center_x, config.arena.center_y,
                config.arena.radius, 96);
            ring.width = 7.0f;
            ring.color = Color{150, 210, 255, 190};
            glow_lines.push_back(std::move(ring));
            // Obstacle outlines in the live arena's enemy tint, dimmed.
            if (active_arena >= 0 &&
                active_arena < static_cast<int>(config.arenas.size())) {
                const ArenaDef& adef =
                    config.arenas[static_cast<size_t>(active_arena)];
                for (const auto& ob : adef.obstacles) {
                    RenderSystem::GlowLine box;
                    box.points = {{ob.x, ob.y}, {ob.x + ob.w, ob.y},
                                  {ob.x + ob.w, ob.y + ob.h}, {ob.x, ob.y + ob.h},
                                  {ob.x, ob.y}};
                    box.width = 4.0f;
                    box.color = Color{adef.enemy_r, adef.enemy_g, adef.enemy_b, 90};
                    box.core = false;   // outline only; the prop art is the body
                    glow_lines.push_back(std::move(box));
                }
            }
            // Laser beams: a hot ribbon over each recycled beam quad.
            for (Entity b : component_storage.entities_with_component<BeamTag>()) {
                auto bp = component_storage.get_component<Position>(b);
                auto bs = component_storage.get_component<Size>(b);
                auto br = component_storage.get_component<Rotation>(b);
                if (!bp.has_value() || !bs.has_value() || !br.has_value()) continue;
                const float cx = bp->get().x + bs->get().width * 0.5f;
                const float cy = bp->get().y + bs->get().height * 0.5f;
                const float half = bs->get().width * 0.5f;
                const float ca = std::cos(br->get().angle);
                const float sa = std::sin(br->get().angle);
                RenderSystem::GlowLine beam;
                beam.points = {{cx - ca * half, cy - sa * half},
                               {cx + ca * half, cy + sa * half}};
                beam.width = 10.0f;
                beam.color = Color{160, 230, 255, 230};
                glow_lines.push_back(std::move(beam));
            }

            // v3 Tier 7 (D213): position-history trails. Sampled here, in the
            // render pass, from live Positions — a pure observer. A trail IS a
            // glow line whose points are where the entity has been, so this
            // reuses Tier 5's ribbon end to end and adds no draw path.
            if (config.trails.enabled) {
                const std::size_t max_pts =
                    static_cast<std::size_t>(std::max(0, config.trails.max_points));
                std::unordered_set<Entity> alive;

                auto center_of = [&](Entity e, float* cx, float* cy) {
                    auto pp = component_storage.get_component<Position>(e);
                    auto ps = component_storage.get_component<Size>(e);
                    if (!pp.has_value() || !ps.has_value()) return false;
                    *cx = pp->get().x + ps->get().width * 0.5f;
                    *cy = pp->get().y + ps->get().height * 0.5f;
                    return true;
                };
                auto sample = [&](Entity e) {
                    float cx, cy;
                    if (!center_of(e, &cx, &cy)) return;
                    alive.insert(e);
                    trail::push_sample(trail_history[e], {cx, cy},
                                       config.trails.min_spacing, max_pts);
                };

                for (Entity e : component_storage.entities_with_component<PlayerTag>())
                    sample(e);
                for (Entity e : component_storage.entities_with_component<ProjectileTag>())
                    sample(e);
                for (Entity e : component_storage.entities_with_component<EnemyShot>())
                    sample(e);

                // Drop history for anything that died this frame, or the map
                // grows for the whole run.
                for (auto it = trail_history.begin(); it != trail_history.end();)
                    it = (alive.count(it->first) == 0) ? trail_history.erase(it)
                                                       : std::next(it);

                // Emit within the per-frame vertex budget, most important
                // first: the hull, then your shots, then incoming fire. What
                // runs out of budget is enemy trails in a heavy wave — the
                // least load-bearing of the three.
                std::size_t verts_left =
                    static_cast<std::size_t>(std::max(0, config.trails.vertex_budget));
                const int shot_r = blackboard.get_or<int>("ship.shot_r", 120);
                const int shot_g = blackboard.get_or<int>("ship.shot_g", 240);
                const int shot_b = blackboard.get_or<int>("ship.shot_b", 255);

                // Shots carry NO Color component, so the ribbon is their ONLY
                // visual and must never fail to draw. When history is too
                // short (a shot fired this frame) or the budget is spent, they
                // fall back to a minimum-length dash along the velocity, which
                // is what makes a fresh shot read as a laser rather than pop in.
                auto emit = [&](Entity e, float head_w, Color col, bool is_shot) {
                    std::vector<line_mesh::P2> pts;
                    auto it = trail_history.find(e);
                    const std::size_t have = it == trail_history.end() ? 0 : it->second.size();
                    const std::size_t keep =
                        trail::points_within_budget(have, verts_left);
                    if (keep >= 2) {
                        // Budget trims from the TAIL: the head is the projectile.
                        pts.assign(it->second.end() - static_cast<long>(keep),
                                   it->second.end());
                    } else if (is_shot) {
                        float cx, cy;
                        if (!center_of(e, &cx, &cy)) return;
                        auto v = component_storage.get_component<Velocity>(e);
                        float dx = v.has_value() ? v->get().dx : 0.0f;
                        float dy = v.has_value() ? v->get().dy : 1.0f;
                        const float len = std::sqrt(dx * dx + dy * dy);
                        if (len < 1e-3f) { dx = 0.0f; dy = 1.0f; }
                        else { dx /= len; dy /= len; }
                        const float dash = head_w * 2.0f;   // a short bolt, not a dot
                        pts = {{cx - dx * dash, cy - dy * dash}, {cx, cy}};
                    } else {
                        return;
                    }
                    RenderSystem::GlowLine t;
                    const std::size_t n = pts.size();
                    t.points = std::move(pts);
                    // v3 Tier 10: shots taper head-heavy (exponent 2) so the
                    // streak reads as a tracer with a bright nose; hull and
                    // dash trails keep the straight taper they were tuned on.
                    t.widths = trail::taper_widths(n, head_w, 0.0f,
                                                   is_shot ? 2.0f : 1.0f);
                    t.width = head_w;
                    t.color = col;
                    t.fade_tail = true;
                    // Shots keep the hot white core — that is what reads as a
                    // laser rather than a smear. Hull/dash trails do not.
                    t.core = is_shot;
                    if (is_shot) t.core_scale = 0.26f;   // v3 Tier 10: hotter nose
                    verts_left -= n * 2;
                    glow_lines.push_back(std::move(t));
                };

                // v3 Tier 11 (D216): the layered explosion. The effect entity
                // enemy_death_system already spawns IS the clock — its sprite
                // clip gives progress, so the ring and the shards need no
                // component, no event and no state of their own. Emitted
                // before the trails so a heavy wave spends its vertex budget on
                // blasts rather than on the tail of every enemy shot.
                for (Entity e : component_storage.entities_with_component<SpriteSheet>()) {
                    auto ss = component_storage.get_component<SpriteSheet>(e);
                    if (!ss.has_value()) continue;
                    if (ss->get().atlas_filename.find("effect_explosion") ==
                        std::string::npos) continue;
                    float cx, cy;
                    if (!center_of(e, &cx, &cy)) continue;
                    const int last = std::max(1, ss->get().total_frames - 1);
                    const float t = std::min(1.0f,
                        static_cast<float>(ss->get().current_frame) /
                        static_cast<float>(last));
                    const float a = explosion_fx::ring_alpha(t);
                    if (a <= 0.01f) continue;

                    // v3 Tier 12: the blast is sized off the DEAD UNIT, not a
                    // constant. `unit` is its edge (enemies 62-82, boss 260), so
                    // a spark pops and a boss detonates without a second code
                    // path for the common case.
                    auto sz = component_storage.get_component<Size>(e);
                    const float unit = sz.has_value()
                        ? std::max(sz->get().width, sz->get().height) : 64.0f;
                    // ponytail: size IS the boss test. Every enemy is <= 82 and
                    // the boss is 260, so no marker component, no death-side
                    // flag — if something in between ever spawns, the blast
                    // scales through the gap smoothly anyway.
                    const bool boss = unit >= 150.0f;
                    const float scale = unit / 70.0f;        // width scale
                    const float reach = unit * (boss ? 0.95f : 0.62f);

                    // Layer 2: the shockwave ring.
                    const float rad = explosion_fx::ring_radius(t, unit * 0.10f,
                                                                reach);
                    RenderSystem::GlowLine ring;
                    ring.points = explosion_fx::ring_points(cx, cy, rad, 24);
                    if (ring.points.size() >= 2 && verts_left >= ring.points.size() * 2) {
                        ring.width = std::max(2.0f, 8.0f * scale * (0.45f + 0.55f * a));
                        ring.color = Color{255, 170, 80,
                                           static_cast<Uint8>(255.0f * std::min(1.0f, a * 1.6f))};
                        // No white core on the blast layers: a white-lifted
                        // ring is what the bloom chain smears into the flat
                        // grey ball that filled the blast. Warm ring, warm
                        // bloom.
                        ring.core = false;
                        verts_left -= ring.points.size() * 2;
                        glow_lines.push_back(std::move(ring));
                    }

                    // Boss only: a second ring running behind the first, and a
                    // white-hot rim on the leading one. Two rings read as a
                    // detonation with depth rather than one bigger pop.
                    if (boss) {
                        const float t2 = std::max(0.0f, t - 0.22f) / 0.78f;
                        const float a2 = explosion_fx::ring_alpha(t2) * 0.8f;
                        const float rad2 = explosion_fx::ring_radius(
                            t2, unit * 0.08f, reach * 0.66f);
                        RenderSystem::GlowLine r2;
                        r2.points = explosion_fx::ring_points(cx, cy, rad2, 28);
                        if (a2 > 0.02f && r2.points.size() >= 2 &&
                            verts_left >= r2.points.size() * 2) {
                            r2.width = std::max(2.0f, 10.0f * scale * a2);
                            r2.color = Color{255, 235, 190,
                                             static_cast<Uint8>(255.0f * a2)};
                            r2.core = false;
                            verts_left -= r2.points.size() * 2;
                            glow_lines.push_back(std::move(r2));
                        }
                    }

                    // Layer 3: debris shards, seeded off the entity id so a
                    // replay throws the same debris.
                    const auto span = explosion_fx::shard_span(t, reach * 1.15f);
                    if (span.outer > span.inner) {
                        const std::size_t SHARDS = boss ? 14u : 6u;
                        for (std::size_t k = 0; k < SHARDS; ++k) {
                            if (verts_left < 4) break;
                            const float ang = explosion_fx::shard_angle(
                                k, SHARDS, static_cast<std::uint32_t>(e));
                            const float dx = std::cos(ang), dy = std::sin(ang);
                            RenderSystem::GlowLine sh;
                            sh.points = {{cx + dx * span.inner, cy + dy * span.inner},
                                         {cx + dx * span.outer, cy + dy * span.outer}};
                            sh.width = std::max(1.5f, 5.5f * scale * (0.4f + 0.6f * a));
                            sh.color = Color{255, 120, 45,
                                             static_cast<Uint8>(255.0f * std::min(1.0f, a * 1.5f))};
                            sh.core = false;
                            sh.fade_tail = true;
                            verts_left -= 4;
                            glow_lines.push_back(std::move(sh));
                        }
                    }
                }

                // A shot's colour rides on its own tag: enemy types carry
                // per-spec colours, and the player's hue already bakes in
                // D184's complement at spawn.

                for (Entity e : component_storage.entities_with_component<PlayerTag>()) {
                    auto pd = component_storage.get_component<ShipState>(e);
                    const bool dashing = pd.has_value() && pd->get().dash_timer > 0.0f;
                    emit(e, dashing ? config.trails.dash_width
                                    : config.trails.drone_width,
                         dashing ? Color{200, 245, 255, 235}
                                 : Color{150, 210, 255, 170},
                         false);
                }
                for (Entity e : component_storage.entities_with_component<ProjectileTag>()) {
                    auto pt = component_storage.get_component<ProjectileTag>(e);
                    const Color c = pt.has_value()
                        ? Color{pt->get().r, pt->get().g, pt->get().b, 235}
                        : Color{static_cast<Uint8>(shot_r), static_cast<Uint8>(shot_g),
                                static_cast<Uint8>(shot_b), 235};
                    emit(e, config.trails.shot_width, c, true);
                }
                for (Entity e : component_storage.entities_with_component<EnemyShot>()) {
                    auto es = component_storage.get_component<EnemyShot>(e);
                    const Color c = es.has_value()
                        ? Color{es->get().r, es->get().g, es->get().b, 235}
                        : Color{255, 80, 80, 235};   // D185: enemy fire is red
                    emit(e, config.trails.shot_width, c, true);
                }
            }
        }

        // v3 Tier 4: when post-fx is live the whole composite lands in its
        // frame target (bloom captures it at begin and resolves back to it),
        // then apply() draws that through the SPIR-V shader to the backbuffer.
        SDL_SetRenderTarget(renderer.get(), postfx.frame_target());
        bloom_system.begin();
        render_system.render_layers(bg_layers);

        // === HOOK: grid-render === (Engine suite, D140 — Lane R / RG)
        // Owner: the resonance grid. Between render_layers and render, so it is
        // over the parallax backdrops and under every entity.
        //
        // Render-only, and structurally so: it consumes fx.grid_impulses (published
        // sim-side at deaths, dashes and blasts) and writes pixels. Nothing reads
        // grid state back, it owns no RNG, and it is stepped by the REAL frame dt
        // rather than the dilated one — the ripple keeps its own clock so bullet
        // time does not turn the lattice to treacle.
        if (config.resonance.enabled) {
            static ResonanceGridSystem grid;
            // D151: sized FROM THE ARENA, not from two numbers in a config block.
            // The first version was a fixed 40x28 at 40 px — 1600 px across an
            // arena 2800 px wide, so it stopped a third of the way to the wall.
            // configure_for_arena is cheap and only rebuilds on a size change.
            grid.configure_for_arena(config.arena.center_x, config.arena.center_y,
                                     config.arena.radius, config.resonance.spacing);
            grid.set_tuning(config.resonance.stiffness, config.resonance.damping,
                            config.resonance.impulse_scale, config.resonance.max_offset,
                            config.resonance.r, config.resonance.g,
                            config.resonance.b, config.resonance.a);
            grid.update(static_cast<float>(timer.get_delta_time()),
                        blackboard.get_or<std::vector<fx_events::Impulse>>(
                            fx_events::GRID_IMPULSES, {}));
            grid.render(renderer.get(), blackboard);
        }
        // === END HOOK: grid-render ===

        // === HOOK: scars-render ===
        // EMPTY. The battle-scar layer (#6, D145) was cut after the first playtest
        // (D151): the arena reads as floating in space, so scorch marks
        // accumulating on "the floor" never made sense. The hook stays — it is
        // pinned by test_scaffolding, and it is a useful slot: under the entities,
        // over the grid.
        // === END HOOK: scars-render ===

        render_system.render(component_storage, blackboard);
        render_system.render_glow_lines(glow_lines, blackboard);   // v3 Tier 5
        render_system.render_particles(component_storage, blackboard);  // v3 Tier 9
        hud_system.render(component_storage, blackboard);
        // Menus composite last, on top of the world and the gameplay HUD.
        ui_render_system.render(component_storage, blackboard);
        // v3 Tier 2: the glow-only pass. Draws each entity's `_glow` sibling
        // (plus additive-tinted visuals) into the emissive target; the bloom
        // chain reads that target, so hulls/backdrop/HUD no longer bleed.
        // Guarded: when bloom is inactive these draws would land on the
        // backbuffer and double every glow sprite.
        if (bloom_system.active()) {
            bloom_system.begin_emissive();
            render_system.render_emissive(component_storage, blackboard);
            render_system.render_glow_lines(glow_lines, blackboard);   // lines bloom too
            render_system.render_particles(component_storage, blackboard);  // discs bloom too
        }
        bloom_system.resolve();
        postfx.apply();

        // === HOOK: palette === (Engine suite, D147 — Lane W / #5) — part 2 of 2
        // Resolve the captured frame through the live palette. BEFORE
        // screenshot_system, not after: a screenshot taken while the capture
        // target was still bound recorded the UNRESOLVED frame, which made the
        // palette invisible to every headless check (it looked like the feature
        // was inert when it was in fact working on screen). The palette is
        // picked from SIM STATE — which arena is live, and whether the hull is
        // critical — and nothing reads back, so this stays render-only.
        if (palette_on) {
            PaletteSystem::Palette pal;
            if (!config.palettes.palettes.empty()) {
                const size_t idx = static_cast<size_t>(std::max(0, active_arena)) %
                                   config.palettes.palettes.size();
                const PaletteDef& def = config.palettes.palettes[idx];
                if (def.colors.size() >= 2) {
                    const uint32_t s0 = def.colors[0], s1 = def.colors[1];
                    pal.shadow_r = static_cast<uint8_t>((s0 >> 16) & 0xFF);
                    pal.shadow_g = static_cast<uint8_t>((s0 >> 8) & 0xFF);
                    pal.shadow_b = static_cast<uint8_t>(s0 & 0xFF);
                    pal.light_r = static_cast<uint8_t>((s1 >> 16) & 0xFF);
                    pal.light_g = static_cast<uint8_t>((s1 >> 8) & 0xFF);
                    pal.light_b = static_cast<uint8_t>(s1 & 0xFF);
                }
                pal.mix = 0.55f;
            }
            // Hull-critical washes the WORLD, not a vignette — the whole point of
            // owning the frame's colour. Pushed toward the palette's shadow so it
            // reads as the reactor browning out rather than as a red overlay.
            for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                auto h = component_storage.get_component<Health>(p);
                if (h.has_value() && h->get().max_hp > 0.0f &&
                    h->get().current > 0.0f &&
                    h->get().current / h->get().max_hp < 0.2f)
                    pal.mix = std::min(1.0f, pal.mix + 0.3f);
                break;
            }
            palette.resolve(renderer.get(), pal);
        }
        // === END HOOK: palette ===

        if (!opts.screenshot_frames.empty()) {
            for (uint64_t sf : opts.screenshot_frames) if (sf == frame) blackboard.set("screenshot_frame", frame);
        }
        screenshot_system.update(blackboard);


        render_system.present();

        if (sim) timer.end_frame(); else timer.end_frame_no_advance();
        timer.update_blackboard(blackboard);
        // v3 Tier 3 (D209): apply pending hit-stop to the NEXT frame's dt.
        // After update_blackboard so the override is the last writer; only
        // while simulating, so pause cannot eat the budget invisibly.
        if (hitstop_left > 0 && sim) {
            blackboard.set("delta_time", 0.0);
            --hitstop_left;
        }

        if (opts.stop_frame.has_value() && timer.get_frame_count() >= opts.stop_frame.value()) {
            running = false;
        }
    }

    // Lane F: closing the window mid-run is also a run end. bank_run_score is
    // idempotent, so a run that already died or won is not counted twice.
    bank_run_score(blackboard, "close");

    // Credits are on the ship, not the blackboard — but a headless run needs them
    // in the summary line to be a usable balance/determinism canary (R2, R6).
    // Drain in-flight telemetry POSTs so the vector's destructor cannot stall
    // shutdown invisibly. ponytail: bounded 2 s grace each, after which the
    // future destructors can still block up to CURLOPT_TIMEOUT (8 s) worst case;
    // a detach-capable client is the upgrade path if exit latency ever matters.
    for (auto& f : tm_inflight)
        if (f.valid()) f.wait_for(std::chrono::seconds(2));

    int final_credits = 0;
    for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
        if (auto s = component_storage.get_component<ShipState>(p); s.has_value()) {
            final_credits = s->get().currency;
        }
        break;
    }
    // Phase 3 adds phase+wave to the line: with the shop able to freeze the run,
    // "score stopped rising" is ambiguous between shopping, dying and stalling.
    std::cout << "Shutting down. Frames: " << timer.get_frame_count()
              << "  Final score: " << blackboard.get_or<int>("score", 0)
              << "  Units: " << final_credits
              << "  Wave: " << blackboard.get_or<int>("wave", 0)
              << "  Phase: " << phase << std::endl;

    // Critical 2: if a /register request is still in flight at shutdown,
    // letting `pending_register` fall out of scope here would block up to
    // the 8s CURLOPT_TIMEOUT in its destructor (std::async(std::launch::async)
    // always blocks there, regardless of wait_for polling). Hand it to the
    // graveyard instead — quit is no longer bounded by network latency at all.
    abandon_future(std::move(pending_register));
    abandon_future(std::move(pending_subscribe));
    // Task 8: same reasoning — a score submit from bank_run_score just above
    // may still be in flight at shutdown; never block the process exit on it.
    abandon_future(std::move(pending_score));
    // Task 9: same reasoning — a leaderboard fetch may still be in flight if
    // the window was closed while the screen was open.
    abandon_future(std::move(pending_top));

    resource_manager_ptr.reset();
    renderer.reset();
    window.reset();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
