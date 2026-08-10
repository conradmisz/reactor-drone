/**
 * Class-110 Final Project — "Reactor Drone" arena survival shooter.
 *
 * Forked from Class-090 (tower defense). You pilot a maintenance drone in a
 * circular reactor core; enemies spawn from the ring and seek you in escalating
 * waves. Move with the arrow keys, aim with the mouse, hold the mouse button or
 * space to fire. Kills give score and drop currency pickups you walk over, which
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

#include "cli_parser.hpp"
#include "script_loader.hpp"
#include "debug_state.hpp"
#include "arena_config.hpp"
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
#include "minimap_system.hpp"
#include "wave_spawner_system.hpp"
#include "shop_system.hpp"

// UI & menu layer (Option-040 port).
#include "engine/lua_manager.hpp"
#include "engine/ecs/systems/screen_stack_system.hpp"
#include "engine/ecs/systems/ui_render_system.hpp"
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

struct SDL_WindowDeleter { void operator()(SDL_Window* w) const { if (w) SDL_DestroyWindow(w); } };
struct SDL_RendererDeleter { void operator()(SDL_Renderer* r) const { if (r) SDL_DestroyRenderer(r); } };
using SDL_WindowPtr = std::unique_ptr<SDL_Window, SDL_WindowDeleter>;
using SDL_RendererPtr = std::unique_ptr<SDL_Renderer, SDL_RendererDeleter>;

// Game phases (Blackboard "phase").
enum Phase { PHASE_TITLE = 0, PHASE_PLAYING = 1, PHASE_GAMEOVER = 2, PHASE_VICTORY = 3,
             PHASE_SHOP = 4, PHASE_INTERMISSION = 5 };

// Screen name of the between-waves prompt, authored in GameData.json's "screens".
constexpr const char* SCREEN_INTERMISSION = "wave_intermission";
constexpr const char* SCREEN_PAUSE        = "pause";
constexpr const char* SCREEN_MAIN_MENU    = "main_menu";

int main(int argc, char* argv[]) {
    auto opts = parse_command_line(argc, argv);
    if (opts.help_requested) return 0;
    if (opts.parse_error) return 1;
    if (!opts.script_file.empty()) {
        std::string script_path = opts.script_file;
        opts = load_script(script_path);
        opts.script_file = script_path;
    }

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
    int selected_ship = 0;

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

    auto clear_arena_props = [&]() {
        for (Entity e : arena_props) component_storage.add_component<DestroyRequest>(e, DestroyRequest{});
        destroy_marked_entities(entity_manager, component_storage);
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
        // Phase 4's equipment publishes the same way (the ids themselves live on
        // the rebuilt ShipState, so only these four need clearing by hand).
        blackboard.set<float>("ship.item_amount", 0.0f);
        blackboard.set<float>("ship.buff_mult", 1.0f);
        blackboard.set<std::string>("ship.item_name", std::string());
        blackboard.set<std::string>("ship.consumable_name", std::string());
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
    };

    spawn_world();
    int phase = PHASE_TITLE;
    blackboard.set("phase", phase);

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
        meta.lifetime_score += bb.get_or<int>("score", 0);
        meta_write(meta_save_path(), meta);
    };

    auto start_run = [&](size_t difficulty_index) {
        config = base_config;
        // Lane F (D82): the ship overlay lands here, in the one place the pristine
        // base_config is re-copied — apply_ship is no more idempotent than
        // apply_difficulty, so there must never be a second application site.
        if (selected_ship >= 0 && selected_ship < static_cast<int>(config.ships.size())) {
            apply_ship(config.player, config.ships[static_cast<size_t>(selected_ship)]);
            load_player_sprite();
        }
        run_banked = false;
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

    // The title *is* the main menu, so it is pushed before the first frame; the
    // stack consumes the command at the top of the loop.
    blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH, std::string(SCREEN_MAIN_MENU));

    Timer timer(opts.fps > 0 ? static_cast<double>(opts.fps) : 60.0);
    if (opts.seed.has_value()) timer.set_deterministic(true);
    timer.update_blackboard(blackboard);

    bool debug_paused = false, step_requested = false;
    bool f1_prev = false, f2_prev = false, space_prev = false, b_prev = false;
    bool tab_prev = false, q_prev = false;
    bool digit_prev[8] = {false};
    if (opts.paused) debug_paused = true;

    std::cout << "Reactor Drone v2 initialized. Arrows move, mouse aims, hold fire. ESC quits.\n";

    bool running = true;
    bool menu_paused = false;   // true while the pause screen is the top screen
    while (running) {
        timer.start_frame();
        input_system.process_events(component_storage, running, blackboard, renderer.get());
        uint64_t frame = timer.get_frame_count();

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
        if (blackboard.get_or<bool>("ui.escape_pressed", false) && phase != PHASE_TITLE) {
            if (ScreenStackSystem::depth(blackboard) <= 1) {
                blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH, std::string(SCREEN_PAUSE));
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
            } else if (pause_click == "on_quit_click") {
                blackboard.remove(UISystem::UI_CLICK_KEY);
                bank_run_score(blackboard);   // Lane F: quitting still ends the run
                running = false;
            }
        }

        // === State machine + gameplay ===
        if (phase == PHASE_PLAYING && sim) {
            apply_move_speed();
            player_control.update(component_storage);

            // === HOOK: dash === (Iteration 3, D51 — Lane B / #5)
            // Owner: the thruster-dash phase. Runs after player_control has written
            // the frame's velocity and before player_fire, so the burst overrides
            // ordinary movement for its window. Nothing else may edit this block.
            {
                // Everything the dash needs is here rather than in the shared
                // key-edge block above, so this hook stays a self-contained diff
                // (D57). Held, not edge-triggered: dash_cd is the gate, so holding
                // LSHIFT simply dashes again the moment it comes off cooldown.
                bool dash_key = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
                for (const auto& ka : opts.keys)
                    if (ka.frame == frame && ka.key == "LSHIFT") dash_key = true;
                static DashState dash_state;   // one dash's scratch; see dash_system.hpp
                tick_dash(component_storage, entity_manager, blackboard, config.dash,
                          dash_state, dash_key,
                          static_cast<float>(blackboard.get_or<double>("delta_time", 0.0)));
            }
            // === END HOOK: dash ===

            player_aim.update(component_storage, blackboard);

            // v2 Phase 5c: equipment visuals. Both ride the aim angle the line
            // above just wrote, so they run here rather than with the render step.
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
            wave_spawner.update(blackboard, entity_manager, component_storage);

            // === HOOK: boss === (Iteration 3, D51 — Lane D / #4)
            // Owner: the boss phase. Runs straight after the spawner so a wave
            // flagged `boss` can spawn its boss and hold the clear condition in
            // the same frame the spawner would otherwise have finished the wave.
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
                    if (want != active_arena) {
                        const ArenaDef& def = config.arenas[static_cast<size_t>(want)];
                        outgoing_backdrop = active_backdrop;
                        active_backdrop = &def.backdrop_layers;
                        shift_timer = 0.0f;
                        shift_pending = want;
                        blackboard.set<std::string>("hud_message", def.name + " — arena shift");
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
                    }
                }

                if (outgoing_backdrop != nullptr) {
                    shift_timer += static_cast<float>(
                        blackboard.get_or<double>("delta_time", 0.0));
                    if (shift_pending >= 0 && shift_timer >= SHIFT_PROP_SWAP) {
                        apply_arena_props(shift_pending);
                        shift_pending = -1;
                    }
                    if (shift_timer >= SHIFT_SECONDS) outgoing_backdrop = nullptr;
                }

                // === HOOK: arena-vfx === (Iteration 3, D51 — Lane E / #2)
                // Owner: the arena-transition VFX phase. Inside the shift tick, so
                // the outgoing props' destruction and the incoming props' arrival
                // animate against the same shift_timer the crossfade already runs on.
                // === END HOOK: arena-vfx ===
            }

            enemy_seek.update(component_storage, blackboard);

            // === HOOK: enemy-fire === (Iteration 3, D51 — Lane D / #3)
            // Owner: the enemy-projectile phase. After the seek step so a shot is
            // aimed from where the enemy actually is this frame, and before
            // movement so a new shot moves on the frame it is born.
            // === END HOOK: enemy-fire ===

            // === HOOK: specialty === (Iteration 3, D51 — Lane D / #9)
            // Owner: the per-arena specialty-unit phase (spitter trails, mines,
            // bulwark facing, splitter). Immediately after enemy-fire because both
            // are "what this enemy does beyond seeking", driven by EnemyBehavior.
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

            if (!player_alive) {
                phase = PHASE_GAMEOVER;
                bank_run_score(blackboard);   // Lane F: the run ended, so it counts
            } else if (wave_spawner.all_complete() &&
                       component_storage.entities_with_component<EnemyTag>().empty()) {
                phase = PHASE_VICTORY;
                bank_run_score(blackboard);
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
            // === END HOOK: shop-menu ===

            animation.update(component_storage, blackboard);
            destroy_marked_entities(entity_manager, component_storage);
        } else if (sim) {
            // Title / game-over / victory: keep effects animating, nothing else.
            animation.update(component_storage, blackboard);
            destroy_marked_entities(entity_manager, component_storage);

            if (phase == PHASE_TITLE) {
                // Phase B (D50): the main menu picks the difficulty. SPACE — not
                // `advance` — is the fallback, because `advance` also fires on the
                // very mouse click that pressed HARD, which would start a Normal
                // run in the same frame.
                const std::string menu_click =
                    blackboard.get_or<std::string>(UISystem::UI_CLICK_KEY, std::string());
                int chosen = -1;
                if (menu_click == "on_start_normal_click") chosen = 0;
                else if (menu_click == "on_start_hard_click") chosen = 1;
                else if (menu_click == "on_ship_cycle") {
                    // Lane F (D82): cycle to the next *unlocked* ship. A locked one
                    // is never landed on, so there is no "you can't fly that" case
                    // to report — with only one ship unlocked this is a no-op and
                    // the widget is a lock readout rather than a selector.
                    selected_ship = next_unlocked_ship(config.ships, selected_ship,
                                                       meta.lifetime_score);
                    blackboard.remove(UISystem::UI_CLICK_KEY);
                }
                refresh_ship_widget();
                if (chosen >= 0) blackboard.remove(UISystem::UI_CLICK_KEY);
                else if (space_edge) chosen = 0;
                if (chosen >= 0) {
                    blackboard.set<bool>(ScreenStackSystem::CMD_POP, true);
                    start_run(static_cast<size_t>(chosen));
                }
            } else if (advance && (phase == PHASE_GAMEOVER || phase == PHASE_VICTORY)) {
                // Restart keeps the difficulty the run was started at — `config`
                // is already scaled, so only the world is rebuilt.
                run_banked = false;   // Lane F: a new run to bank at its end
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
                float ang = shake_angle(shake_rng);
                ox = std::cos(ang) * amp;
                oy = std::sin(ang) * amp;
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
            auto smoothstep = [](float t) { return t * t * (3.0f - 2.0f * t); };
            push_backdrop(active_backdrop,
                          outgoing_backdrop != nullptr
                              ? smoothstep(std::min(1.0f, shift_timer / SHIFT_SECONDS))
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
              << "  Credits: " << final_credits
              << "  Wave: " << blackboard.get_or<int>("wave", 0)
              << "  Phase: " << phase << std::endl;

    resource_manager_ptr.reset();
    renderer.reset();
    window.reset();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
