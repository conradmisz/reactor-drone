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
#include <vector>

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/destruction.hpp"
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

#include <nlohmann/json.hpp>

#include "net/http_client.hpp"
#include "net/net_config.hpp"
#include "cli_parser.hpp"
#include "script_loader.hpp"
#include "debug_state.hpp"
#include "arena_config.hpp"
#include "arena_vfx.hpp"
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

struct SDL_WindowDeleter { void operator()(SDL_Window* w) const { if (w) SDL_DestroyWindow(w); } };
struct SDL_RendererDeleter { void operator()(SDL_Renderer* r) const { if (r) SDL_DestroyRenderer(r); } };
using SDL_WindowPtr = std::unique_ptr<SDL_Window, SDL_WindowDeleter>;
using SDL_RendererPtr = std::unique_ptr<SDL_Renderer, SDL_RendererDeleter>;

// Game phases (Blackboard "phase").
enum Phase { PHASE_TITLE = 0, PHASE_PLAYING = 1, PHASE_GAMEOVER = 2, PHASE_VICTORY = 3,
             PHASE_SHOP = 4, PHASE_INTERMISSION = 5, PHASE_NAME_ENTRY = 6,
             PHASE_LEADERBOARD = 7 };

// Screen name of the between-waves prompt, authored in GameData.json's "screens".
constexpr const char* SCREEN_INTERMISSION = "wave_intermission";
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
    SDL_RendererPtr renderer(SDL_CreateRenderer(window.get(), nullptr));
    if (!renderer) { std::cerr << "Renderer: " << SDL_GetError() << std::endl; SDL_Quit(); return 1; }

    // === ECS + config ===
    EntityManager entity_manager;
    ComponentStorage component_storage;
    Blackboard blackboard;

    const std::string gamedata_path = project_paths::assets_dir() + "/GameData.json";
    // Engine loader sets window/camera (and ignores our arena blocks).
    load_game_data(gamedata_path, entity_manager, component_storage, blackboard);
    GameConfig config = load_arena_config(gamedata_path);
    if (opts.seed.has_value()) config.seed = static_cast<unsigned int>(opts.seed.value());
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
        enemy_seek.set_arena(&def.obstacles,
            enemy_path::build_obstacle_grid(path_cols, path_rows, path_cell,
                def.obstacles, config.pathfinding.clearance));
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
            config.player.weapon.spread, 0.0f});
        // Per-run economy/shop state (D3: no persistence — a fresh one each run).
        // The ship's stats are already baked into `config` by start_run; ship_id
        // just records which hull this run is flying.
        ShipState ship_state{};
        ship_state.ship_id = selected_ship;
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
    auto bank_run_score = [&](const Blackboard& bb) {
        if (run_banked) return;
        run_banked = true;
        const int run_score = bb.get_or<int>("score", 0);
        meta.lifetime_score += run_score;
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
        // Lane F (D82): the ship overlay lands here, in the one place the pristine
        // base_config is re-copied — apply_ship is no more idempotent than
        // apply_difficulty, so there must never be a second application site.
        if (selected_ship >= 0 && selected_ship < static_cast<int>(config.ships.size())) {
            const ShipDef& sd = config.ships[static_cast<size_t>(selected_ship)];
            apply_ship(config.player, sd);
            load_player_sprite();
            // D184: shots fire in the complement of the hull's hue, published
            // here so PlayerFireSystem stays catalogue-blind like the barrel
            // count. get_or defaults in the reader keep old saves harmless.
            blackboard.set<int>("ship.shot_r", 255 - sd.color_r);
            blackboard.set<int>("ship.shot_g", 255 - sd.color_g);
            blackboard.set<int>("ship.shot_b", 255 - sd.color_b);
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
        // The one visible proof of the choice in a headless run — the menu itself
        // is the only place it shows on screen.
        // Lane F adds the ship: it is the only headless proof of which hull flew,
        // and it sits on this line rather than the summary so the replay canary's
        // comparison target is unchanged.
        std::cout << "Run start: difficulty " << label << "  ship "
                  << (selected_ship >= 0 && selected_ship < static_cast<int>(config.ships.size())
                          ? config.ships[static_cast<size_t>(selected_ship)].name
                          : std::string("Standard"))
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
        const int unlocked = unlocked_ship_count(config.ships, meta.lifetime_score);
        if (unlocked > 1 && selected_ship < static_cast<int>(config.ships.size())) {
            el->get().label_text = "SHIP: " + config.ships[static_cast<size_t>(selected_ship)].name
                                 + "   (click to change)";
            el->get().style_id = "default_button";
            return;
        }
        // Only one hull available: show the next threshold instead of a dead button.
        el->get().style_id = "subtitle";
        el->get().label_text.clear();
        for (const ShipDef& s : config.ships) {
            if (!ship_unlocked(s, meta.lifetime_score)) {
                el->get().label_text = s.name + " unlocks at " + std::to_string(s.unlock_score)
                                     + " pts  (lifetime "
                                     + std::to_string(meta.lifetime_score) + ")";
                break;
            }
        }
    };

    // Lane K (D100): the two widgets this lane owns, resolved by name through
    // ui.widget_id.<name> — the same path the HUD gauges and the ship selector
    // use, deliberately not a second lookup mechanism. Widget ids are load-time
    // and survive spawn_world, so each is resolved once.
    auto widget_by_name = [&](const char* name, Entity& cache, bool& resolved) {
        if (!resolved) {
            const double v = blackboard.get_or<double>(std::string("ui.widget_id.") + name, -1.0);
            cache = v < 0.0 ? 0 : static_cast<Entity>(v);
            resolved = true;
        }
        return cache;
    };
    Entity save_w = 0, continue_w = 0;
    bool save_w_resolved = false, continue_w_resolved = false;
    auto save_widget = [&]() { return widget_by_name("pause_save", save_w, save_w_resolved); };

    // Task 7: name_entry's two dynamic labels, resolved the same by-name way.
    Entity name_buf_w = 0, name_msg_w = 0;
    bool name_buf_w_resolved = false, name_msg_w_resolved = false;

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

    // Main-menu-suite Phase C: settings — loaded once, published to the two
    // Blackboard flags the apply sites read, checkbox widgets synced at boot.
    SettingsSave settings = settings_load(settings_save_path());
    blackboard.set<bool>("settings.screen_shake", settings.screen_shake);
    blackboard.set<bool>("settings.minimap", settings.minimap);
    Entity shake_w = 0, mini_w = 0, rec_w[4] = {};
    bool shake_w_resolved = false, mini_w_resolved = false, rec_w_resolved[4] = {};
    auto sync_settings_widgets = [&]() {
        const Entity sw = widget_by_name("settings_shake", shake_w, shake_w_resolved);
        const Entity mw = widget_by_name("settings_minimap", mini_w, mini_w_resolved);
        if (auto st = component_storage.get_component<UIState>(sw); st.has_value())
            st->get().value = settings.screen_shake ? 1.0f : 0.0f;
        if (auto st = component_storage.get_component<UIState>(mw); st.has_value())
            st->get().value = settings.minimap ? 1.0f : 0.0f;
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

    bool running = true;
    bool menu_paused = false;   // true while the pause screen is the top screen
    while (running) {
        timer.start_frame();
        input_system.process_events(component_storage, running, blackboard, renderer.get());
        uint64_t frame = timer.get_frame_count();

        // Task 8: poll the score submission every frame, regardless of phase —
        // it can still be in flight after the player has already backed out to
        // the title. wait_for(0s) never blocks (net/http_client.hpp).
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
                            const long long score = r.at("score").get<long long>();
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
        bool scripted_fire = false, scripted_advance = false;
        int scripted_digit = 0;
        bool scripted_shop = false, scripted_tab = false, scripted_use = false;
        for (const auto& ka : opts.keys) if (ka.frame == frame) {
            // Scripted ESC mirrors the real key: it toggles the pause screen, it
            // does not quit. A script ends via --stopframe. Making this still quit
            // would mean headless runs could never exercise the pause path at all.
            if (ka.key == "ESC") blackboard.set("ui.escape_pressed", true);
            else if (ka.key == "SPACE") { scripted_fire = true; scripted_advance = true; }
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
        // Task 9: PHASE_LEADERBOARD has the same shape of its own ESC handling.
        if (blackboard.get_or<bool>("ui.escape_pressed", false) && phase != PHASE_TITLE &&
            phase != PHASE_NAME_ENTRY && phase != PHASE_LEADERBOARD) {
            if (ScreenStackSystem::depth(blackboard) <= 1) {
                blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH, std::string(SCREEN_PAUSE));
                // Lane K: the button says SAVE again each time the screen opens,
                // rather than showing the last visit's "SAVED" confirmation.
                if (Entity w = save_widget(); w != 0) {
                    if (auto el = component_storage.get_component<UIElement>(w); el.has_value())
                        el->get().label_text = "SAVE";
                }
            } else {
                blackboard.set<bool>(ScreenStackSystem::CMD_POP, true);
            }
        }

        // The pause screen freezes the sim. Note this is NOT is_modal(): the wave
        // intermission is also a modal screen, and it must keep the drone flying.
        {
            const std::vector<std::string> stack = ScreenStackSystem::get_stack(blackboard);
            menu_paused = !stack.empty() && stack.back() == SCREEN_PAUSE;
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
                bank_run_score(blackboard);   // Lane F: quitting still ends the run
                running = false;
            } else if (pause_click == "on_to_menu_click") {
                // Main-menu-suite Phase C: back to the hub without exiting. Banks
                // like QUIT (the run is over as far as progression is concerned;
                // the slot file survives, so CONTINUE can pick it back up). The
                // world's entities stay where they are — PHASE_TITLE runs no sim,
                // so the arena simply freezes behind the menu until the next
                // start_run's spawn_world rebuilds it.
                blackboard.remove(UISystem::UI_CLICK_KEY);
                bank_run_score(blackboard);
                phase = PHASE_TITLE;
                blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                            std::string(SCREEN_MAIN_MENU));
            }
        }

        // === HOOK: prestige === (Iteration 5, D128/D129 — Lane O / #14)
        // The arc-complete offer. Handled HERE, above the phase machine, for the
        // same reason the pause buttons are: the click that presses PRESTIGE RUN
        // is also a plain `advance`, which the game-over/victory branch below
        // reads as "retry". Consuming both the click and `advance` in one place
        // is the only way the two can never fire on the same frame — the same
        // trap the title screen hit with SPACE vs `advance` (D50).
        {
            static bool offer_up = false;
            static Entity prestige_line = 0;
            static bool prestige_line_resolved = false;

            const bool want_offer = (phase == PHASE_VICTORY);
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
            }
            // === END HOOK: dash ===

            player_aim.update(component_storage, blackboard);

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

            movement.update(component_storage, blackboard);

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
                bank_run_score(blackboard);   // Lane F: the run ended, so it counts
                end_saved_run();              // Lane K: and a dead run is not resumable
            } else if (wave_spawner.all_complete() &&
                       component_storage.entities_with_component<EnemyTag>().empty()) {
                phase = PHASE_VICTORY;
                bank_run_score(blackboard);
                end_saved_run();
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
                    // Lane F (D82): cycle to the next *unlocked* ship. A locked one
                    // is never landed on, so there is no "you can't fly that" case
                    // to report — with only one ship unlocked this is a no-op and
                    // the widget is a lock readout rather than a selector.
                    selected_ship = next_unlocked_ship(config.ships, selected_ship,
                                                       meta.lifetime_score);
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
                } else if (menu_click == "on_how_click") {
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    blackboard.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO,
                                                std::string(SCREEN_HOW));
                } else if (menu_click == "on_toggle_shake" ||
                           menu_click == "on_toggle_minimap") {
                    // UISystem already flipped the checkbox's UIState.value — read
                    // it back as the truth, persist, and publish to the apply sites.
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                    const bool shake_toggle = (menu_click == "on_toggle_shake");
                    const Entity w = shake_toggle
                        ? widget_by_name("settings_shake", shake_w, shake_w_resolved)
                        : widget_by_name("settings_minimap", mini_w, mini_w_resolved);
                    if (auto st = component_storage.get_component<UIState>(w);
                        st.has_value()) {
                        const bool on = st->get().value >= 0.5f;
                        (shake_toggle ? settings.screen_shake : settings.minimap) = on;
                        blackboard.set<bool>(shake_toggle ? "settings.screen_shake"
                                                          : "settings.minimap", on);
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
                for (char c : typed) {
                    if (name_buf.size() >= 24) break;
                    // Printable ASCII only (space..~) — filters control bytes and
                    // any non-ASCII UTF-8 lead/continuation bytes from an IME.
                    if (c >= 0x20 && c < 0x7F) name_buf.push_back(c);
                }
                if (blackboard.get_or<bool>("ui.backspace_pressed", false) && !name_buf.empty())
                    name_buf.pop_back();

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
                            el->get().label_text = name_buf + "_";
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
                                    : "Type a name, then press ENTER  (ESC to skip)";
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
            } else if (advance && (phase == PHASE_GAMEOVER || phase == PHASE_VICTORY)) {
                // Restart keeps the difficulty the run was started at — `config`
                // is already scaled, so only the world is rebuilt.
                run_banked = false;   // Lane F: a new run to bank at its end
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
            float trauma = feedback::decay_trauma(
                blackboard.get_or<float>("feedback.trauma", 0.0f), dt,
                config.feedback.trauma_decay_per_sec);
            blackboard.set<float>("feedback.trauma", trauma);

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
        render_system.render_layers(bg_layers);
        render_system.render(component_storage, blackboard);
        hud_system.render(component_storage, blackboard);
        // Menus composite last, on top of the world and the gameplay HUD.
        ui_render_system.render(component_storage, blackboard);

        if (!opts.screenshot_frames.empty()) {
            for (uint64_t sf : opts.screenshot_frames) if (sf == frame) blackboard.set("screenshot_frame", frame);
        }
        screenshot_system.update(blackboard);
        render_system.present();

        if (sim) timer.end_frame(); else timer.end_frame_no_advance();
        timer.update_blackboard(blackboard);

        if (opts.stop_frame.has_value() && timer.get_frame_count() >= opts.stop_frame.value()) {
            running = false;
        }
    }

    // Lane F: closing the window mid-run is also a run end. bank_run_score is
    // idempotent, so a run that already died or won is not counted twice.
    bank_run_score(blackboard);

    // Credits are on the ship, not the blackboard — but a headless run needs them
    // in the summary line to be a usable balance/determinism canary (R2, R6).
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
