/**
 * Class-110 Final Project — "Reactor Drone" arena survival shooter.
 *
 * Forked from Class-090 (tower defense). You pilot a maintenance drone in a
 * circular reactor core; enemies spawn from the ring and seek you in escalating
 * waves. Move with the arrow keys, aim with the mouse, hold the mouse button or
 * space to fire. Kills give score + XP; each level auto-applies a weighted-random
 * weapon upgrade. Title -> play -> game-over/victory, click to (re)start.
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
#include "experience_system.hpp"
#include "upgrade_system.hpp"
#include "wave_spawner_system.hpp"
#include "game_hud_system.hpp"

struct SDL_WindowDeleter { void operator()(SDL_Window* w) const { if (w) SDL_DestroyWindow(w); } };
struct SDL_RendererDeleter { void operator()(SDL_Renderer* r) const { if (r) SDL_DestroyRenderer(r); } };
using SDL_WindowPtr = std::unique_ptr<SDL_Window, SDL_WindowDeleter>;
using SDL_RendererPtr = std::unique_ptr<SDL_Renderer, SDL_RendererDeleter>;

// Game phases (Blackboard "phase").
enum Phase { PHASE_TITLE = 0, PHASE_PLAYING = 1, PHASE_GAMEOVER = 2, PHASE_VICTORY = 3 };

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

    const int win_w = blackboard.get_or<int>("window_width", 980);
    const int win_h = blackboard.get_or<int>("window_height", 660);

    // Fixed camera centered on the arena.
    blackboard.set<float>("camera.lookat.x", config.arena.center_x);
    blackboard.set<float>("camera.lookat.y", config.arena.center_y);
    blackboard.set<float>("camera.zoom", 1.0f);
    blackboard.set<float>("player.invuln_window", config.player.invuln_window);

    // Quadtree world bounds cover the arena + spawn ring + margin.
    const float margin = 80.0f;
    const float world_x = config.arena.center_x - config.arena.spawn_radius - margin;
    const float world_y = config.arena.center_y - config.arena.spawn_radius - margin;
    const float world_wh = 2.0f * (config.arena.spawn_radius + margin);
    QuadtreeStrategy quadtree(6, 8, world_x, world_y, world_wh, world_wh);

    // === Systems ===
    InputSystem input_system;
    PlayerControlSystem player_control(config.player.move_speed);
    PlayerAimSystem player_aim;
    PlayerFireSystem player_fire(config.seed);
    EnemySeekSystem enemy_seek;
    WaveSpawnerSystem wave_spawner;
    wave_spawner.set_config(&config);
    MovementSystem movement;
    CollisionSystem collision(quadtree);
    ProjectileHitSystem projectile_hit;
    PlayerDamageSystem player_damage;
    DamageApplySystem damage_apply;
    EnemyDeathSystem enemy_death;
    ExperienceSystem experience;
    UpgradeSystem upgrade;
    upgrade.set_pool(config.upgrades, config.seed);
    LifetimeSystem lifetime;
    AnimationSystem animation;
    CameraSystem camera;

    auto resource_manager_ptr =
        std::make_unique<ResourceManager>(renderer.get(), project_paths::assets_dir());
    ResourceManager& resource_manager = *resource_manager_ptr;
    RenderSystem render_system(renderer.get(), resource_manager);
    HUDSystem hud_system(renderer.get(), resource_manager, win_w, win_h);

    std::string log_dir = create_log_directory(opts.clear_logs);
    ScreenshotSystem screenshot_system(renderer.get(), log_dir);

    GameHUDSystem game_hud;

    // Load the player sprite once (optional; falls back to a Color rectangle).
    std::optional<sidecar_loader::LoadedSprite> player_sprite;
    if (!config.player.sidecar.empty()) {
        try {
            player_sprite = sidecar_loader::load(
                project_paths::assets_dir() + "/" + config.player.sidecar, config.player.idle_clip);
        } catch (...) { player_sprite.reset(); }
    }

    // spawn_world: tear down everything and build a fresh run (player + HUD).
    auto spawn_world = [&]() {
        for (Entity e : entity_manager.all_entities()) {
            component_storage.add_component<DestroyRequest>(e, DestroyRequest{});
        }
        destroy_marked_entities(entity_manager, component_storage);

        blackboard.set("score", 0);
        blackboard.set("level", 1);
        blackboard.set("wave", 0);
        blackboard.set("pending_xp", 0);
        blackboard.set("pending_upgrades", 0);
        blackboard.set<float>("player.iframes", 0.0f);
        blackboard.set<float>("upgrade_message_timer", 0.0f);
        blackboard.set<std::string>("upgrade_message", std::string());
        blackboard.set("all_waves_complete", false);
        wave_spawner.reset();

        // Player entity.
        const float psz = config.player.size;
        Entity player = entity_manager.create_entity();
        component_storage.add_component<Position>(player,
            Position{config.player.start_x - psz * 0.5f, config.player.start_y - psz * 0.5f});
        component_storage.add_component<Velocity>(player, Velocity{0.0f, 0.0f});
        component_storage.add_component<Size>(player, Size{psz, psz});
        component_storage.add_component<Rotation>(player, Rotation{0.0f, 0.0f});
        component_storage.add_component<Input>(player, Input{});
        component_storage.add_component<Color>(player, Color{90, 200, 160, 255});
        component_storage.add_component<PlayerTag>(player, PlayerTag{});
        component_storage.add_component<Health>(player,
            Health{config.player.start_health, config.player.start_health});
        component_storage.add_component<WeaponStats>(player, WeaponStats{
            config.player.weapon.fire_rate, config.player.weapon.damage,
            config.player.weapon.projectile_speed, config.player.weapon.projectile_lifetime,
            config.player.weapon.spread, 0.0f});
        component_storage.add_component<Experience>(player,
            Experience{0.0f, 1, config.xp_level2, config.xp_multiplier});
        component_storage.add_component<Collider>(player,
            Collider{psz, psz, layers::PLAYER, layers::PLAYER_MASK});
        component_storage.add_component<CircleCollider>(player, CircleCollider{psz * 0.5f, 0.0f, 0.0f});
        component_storage.add_component<RenderLayer>(player, RenderLayer{3});
        if (player_sprite.has_value()) {
            component_storage.add_component<SpriteSheet>(player, player_sprite->sprite_sheet);
            component_storage.add_component<Animation>(player, player_sprite->animation);
        }

        game_hud.init(component_storage, entity_manager, blackboard);
    };

    spawn_world();
    int phase = PHASE_TITLE;
    blackboard.set("phase", phase);

    Timer timer(opts.fps > 0 ? static_cast<double>(opts.fps) : 60.0);
    if (opts.seed.has_value()) timer.set_deterministic(true);
    timer.update_blackboard(blackboard);

    bool debug_paused = false, step_requested = false;
    bool f1_prev = false, f2_prev = false, space_prev = false;
    if (opts.paused) debug_paused = true;

    std::cout << "Reactor Drone v2 initialized. Arrows move, mouse aims, hold fire. ESC quits.\n";

    bool running = true;
    while (running) {
        timer.start_frame();
        input_system.process_events(component_storage, running, blackboard);
        uint64_t frame = timer.get_frame_count();

        // Scripted input injection (headless testing).
        for (const auto& c : opts.clicks) if (c.frame == frame) {
            blackboard.set("mouse_click_x", static_cast<float>(c.x));
            blackboard.set("mouse_click_y", static_cast<float>(c.y));
            blackboard.set("mouse.clicked", true);
        }
        for (const auto& hv : opts.hovers) if (hv.frame == frame) {
            blackboard.set("mouse.x", static_cast<double>(hv.x));
            blackboard.set("mouse.y", static_cast<double>(hv.y));
        }
        bool scripted_fire = false, scripted_advance = false;
        for (const auto& ka : opts.keys) if (ka.frame == frame) {
            if (ka.key == "ESC") running = false;
            else if (ka.key == "SPACE") { scripted_fire = true; scripted_advance = true; }
            else if (ka.key == "F1") debug_paused = !debug_paused;
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
        float mx, my; Uint32 mbtn = SDL_GetMouseState(&mx, &my);
        blackboard.set("mouse.held", (mbtn & SDL_BUTTON_LMASK) != 0 || scripted_fire);

        bool clicked = blackboard.get_or<bool>("mouse.clicked", false);
        bool advance = clicked || space_edge;

        bool sim = (!debug_paused || step_requested);

        // === State machine + gameplay ===
        if (phase == PHASE_PLAYING && sim) {
            player_control.update(component_storage);
            player_aim.update(component_storage, blackboard);
            wave_spawner.update(blackboard, entity_manager, component_storage);
            enemy_seek.update(component_storage);
            movement.update(component_storage, blackboard);

            // Clamp the player inside the arena circle.
            for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                auto pos = component_storage.get_component<Position>(p);
                auto sz = component_storage.get_component<Size>(p);
                if (!pos.has_value() || !sz.has_value()) continue;
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
            }

            player_fire.update(component_storage, entity_manager, blackboard);
            collision.update(component_storage, blackboard);
            projectile_hit.update(entity_manager, component_storage);
            player_damage.update(entity_manager, component_storage, blackboard);
            damage_apply.update(entity_manager, component_storage);
            enemy_death.update(component_storage, entity_manager, blackboard);
            experience.update(component_storage, blackboard);
            upgrade.update(component_storage, blackboard);
            lifetime.update(component_storage, blackboard);
            animation.update(component_storage, blackboard);
            destroy_marked_entities(entity_manager, component_storage);

            // Win/lose detection.
            bool player_alive = false;
            for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
                auto h = component_storage.get_component<Health>(p);
                if (h.has_value() && h->get().current > 0.0f) player_alive = true;
            }
            if (!player_alive) {
                phase = PHASE_GAMEOVER;
            } else if (wave_spawner.all_complete() &&
                       component_storage.entities_with_component<EnemyTag>().empty()) {
                phase = PHASE_VICTORY;
            }
        } else if (sim) {
            // Title / game-over / victory: keep effects animating, nothing else.
            animation.update(component_storage, blackboard);
            destroy_marked_entities(entity_manager, component_storage);

            if (advance) {
                if (phase == PHASE_TITLE) {
                    phase = PHASE_PLAYING;
                } else if (phase == PHASE_GAMEOVER || phase == PHASE_VICTORY) {
                    spawn_world();
                    phase = PHASE_PLAYING;
                }
            }
        }
        if (step_requested) apply_step_complete(step_requested, debug_paused);
        blackboard.set("phase", phase);

        // Tick the level-up message timer.
        float dt = static_cast<float>(blackboard.get_or<double>("delta_time", 0.0));
        float mt = blackboard.get_or<float>("upgrade_message_timer", 0.0f);
        if (mt > 0.0f) blackboard.set<float>("upgrade_message_timer", std::max(0.0f, mt - dt));

        game_hud.update(component_storage, blackboard);

        // === Render ===
        camera.update(component_storage, blackboard);
        render_system.clear_background();
        render_system.render(component_storage, blackboard);
        hud_system.render(component_storage, blackboard);

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

    std::cout << "Shutting down. Frames: " << timer.get_frame_count()
              << "  Final score: " << blackboard.get_or<int>("score", 0) << std::endl;

    resource_manager_ptr.reset();
    renderer.reset();
    window.reset();
    TTF_Quit();
    SDL_Quit();
    return 0;
}
