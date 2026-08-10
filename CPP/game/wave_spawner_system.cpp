#include "wave_spawner_system.hpp"
#include "enemy_fire_system.hpp"   // behavior_kind_for
#include "enemy_components.hpp"    // EnemyTag, Health, PathFollower (seek speed)
#include "player_components.hpp"   // ContactDamage
#include "collision_layers.hpp"
#include "engine/project_paths.hpp"
#include <cmath>
#include <algorithm>

void WaveSpawnerSystem::set_config(const GameConfig* cfg) {
    cfg_ = cfg;
    if (cfg_) rng_.seed(cfg_->seed);
    reset();
}

int WaveSpawnerSystem::total_waves() const {
    if (!cfg_) return 0;
    int n = static_cast<int>(cfg_->waves.size());
    if (cfg_->victory_wave > 0 && cfg_->victory_wave < n) return cfg_->victory_wave;
    return n;
}

const sidecar_loader::LoadedSprite* WaveSpawnerSystem::resolve_sprite(
    const std::string& sidecar, const std::string& clip) {
    if (sidecar.empty()) return nullptr;
    std::string key = sidecar + "|" + clip;
    auto it = sprite_cache_.find(key);
    if (it != sprite_cache_.end()) return &it->second;
    try {
        std::string abs_path = project_paths::assets_dir() + "/" + sidecar;
        auto [inserted, _] = sprite_cache_.emplace(key, sidecar_loader::load(abs_path, clip));
        (void)_;
        return &inserted->second;
    } catch (...) {
        return nullptr;  // fall back to the Color rectangle
    }
}

void WaveSpawnerSystem::spawn_enemy(const WaveDef& wave,
                                    EntityManager& entity_manager,
                                    ComponentStorage& component_storage) {
    // Choose enemy type: cycle the wave's type list, or all types if none listed.
    int type_index;
    if (!wave.types.empty()) {
        type_index = wave.types[static_cast<size_t>(enemies_spawned_) % wave.types.size()];
    } else {
        type_index = enemies_spawned_ % static_cast<int>(cfg_->enemy_types.size());
    }

    // Iteration 3 (D67): the arena's specialty unit and the unlocked moon
    // shooters are injected on a spawn cadence rather than being written into 50
    // wave rosters. The live arena is derived from the wave number here rather
    // than pushed in from main.cpp: it is the same pure function main uses, and
    // deriving it keeps the spawner off the arena-shift timing entirely.
    const int wave_number = current_wave_ + 1;
    int specialty_tier = 1;
    if (cfg_->specialty.every_n_spawns > 0 || cfg_->specialty.moon_every_n_spawns > 0) {
        int specialty_unit = -1;
        const int ai = active_arena_index(cfg_->arenas, wave_number);
        if (ai >= 0) {
            specialty_unit = cfg_->arenas[static_cast<size_t>(ai)].specialty_unit;
            specialty_tier = cfg_->arenas[static_cast<size_t>(ai)].specialty_tier;
        }
        const int inj = injected_type(enemies_spawned_, cfg_->specialty, specialty_unit,
                                      unlocked_injections(cfg_->enemy_types, wave_number));
        if (inj >= 0) type_index = inj;
        if (inj != specialty_unit) specialty_tier = 1;   // a moon is not tiered by arena
    }

    type_index = std::max(0, std::min(type_index, static_cast<int>(cfg_->enemy_types.size()) - 1));
    const EnemyType& type = cfg_->enemy_types[static_cast<size_t>(type_index)];

    // Ring spawn: random angle at spawn_radius around the *player* (falling back
    // to the arena centre when there is none), clamped inside the arena circle.
    // Draw the angle first, unconditionally, so replays stay deterministic.
    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530717958647692f);
    float angle = angle_dist(rng_);
    // R2: the tie-dye hue offset is drawn here, unconditionally, on every spawn in
    // every arena — never inside an `if (tie_dye)`. A conditional draw would make
    // the RNG sequence depend on which arena is live and break replay determinism.
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    float tint_phase = unit(rng_);
    float px = cfg_->arena.center_x, py = cfg_->arena.center_y;
    for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
        auto pos = component_storage.get_component<Position>(p);
        auto sz = component_storage.get_component<Size>(p);
        if (!pos.has_value() || !sz.has_value()) continue;
        px = pos->get().x + sz->get().width * 0.5f;
        py = pos->get().y + sz->get().height * 0.5f;
        break;
    }
    Vec2 ring = ring_spawn_point(px, py, angle, cfg_->arena.spawn_radius,
                                 cfg_->arena.center_x, cfg_->arena.center_y,
                                 cfg_->arena.radius);
    float ring_x = ring.x, ring_y = ring.y;
    float half = type.size * 0.5f;

    Entity e = entity_manager.create_entity();
    component_storage.add_component<Position>(e, Position{ring_x - half, ring_y - half});
    component_storage.add_component<Size>(e, Size{type.size, type.size});
    // v2 Phase 5a: enemy sprites are pure luminance, so the arena's enemy tint is
    // what gives them a colour. Color doubles as the no-sprite fallback rect *and*
    // as the record of the entity's resting tint, which is what FlashSystem
    // restores when a hit flash expires (feedback::flash_tint's `base`).
    // ponytail: on the fallback path Color and Tint multiply, so a sprite-load
    // failure renders a darker rect than the sprite would. Failure path only.
    component_storage.add_component<Color>(e, Color{enemy_r_, enemy_g_, enemy_b_, 255});
    component_storage.add_component<Tint>(e, Tint{enemy_r_, enemy_g_, enemy_b_, 255, false});
    component_storage.add_component<Velocity>(e, Velocity{0.0f, 0.0f});
    const sidecar_loader::LoadedSprite* sprite = resolve_sprite(type.sidecar, type.clip);
    if (sprite != nullptr) {
        component_storage.add_component<SpriteSheet>(e, sprite->sprite_sheet);
        component_storage.add_component<Animation>(e, sprite->animation);
    }
    // PathFollower reused purely as the enemy's seek speed (EnemySeekSystem).
    // Per-wave multipliers scale the shared enemy_types (D10 — no new types).
    // Iteration 3 (D68): a specialty unit on the second pass over the four themes
    // (waves 26-50) is the SAME unit, harder — two multipliers, not a new row.
    const int beh_kind = enemy_fire::behavior_kind_for(type.behavior);
    const bool tier2 = specialty_tier >= 2 && beh_kind != behavior_kinds::SEEKER &&
                       beh_kind != behavior_kinds::SHOOTER;
    const float sp_hp    = tier2 ? cfg_->specialty.tier2_hp_mult : 1.0f;
    const float sp_speed = tier2 ? cfg_->specialty.tier2_speed_mult : 1.0f;

    component_storage.add_component<PathFollower>(e,
        PathFollower{1, 0.0f, type.speed * wave.speed_mult * sp_speed,
                     0.0f, 0.0f, 0.0f, tint_phase});
    float hp = type.health * wave.hp_mult * sp_hp;
    component_storage.add_component<Health>(e, Health{hp, hp});
    // Iteration 3 (D66): whatever this type does beyond seeking. The timer starts
    // at a full cooldown, so nothing fires on the frame it spawns.
    if (beh_kind != behavior_kinds::SEEKER) {
        const float interval = type.fire_interval > 0.0f ? type.fire_interval : 2.0f;
        const int tier = tier2 ? 2 : type.behavior_tier;
        component_storage.add_component<EnemyBehavior>(e,
            EnemyBehavior{beh_kind, tier, interval, interval, 0.0f});
    }
    component_storage.add_component<EnemyTag>(e, EnemyTag{});
    // Enemies used to fall to layer 0 — behind the walls. At 64-78 px that reads
    // as a bug, so they share the obstacle layer and still sit under the player(3).
    component_storage.add_component<RenderLayer>(e, RenderLayer{2});
    component_storage.add_component<ContactDamage>(e,
        ContactDamage{type.contact_damage, type.score, type.currency, type.drop_chance});
    component_storage.add_component<Collider>(e,
        Collider{type.size, type.size, layers::ENEMY, layers::ENEMY_MASK});
    component_storage.add_component<CircleCollider>(e, CircleCollider{half, 0.0f, 0.0f});

    enemies_spawned_++;
}

void WaveSpawnerSystem::update(Blackboard& blackboard,
                               EntityManager& entity_manager,
                               ComponentStorage& component_storage) {
    wave_just_cleared_ = false;
    if (!cfg_ || cfg_->waves.empty() || cfg_->enemy_types.empty()) return;
    if (!blackboard.has("delta_time")) return;

    const int total = total_waves();
    blackboard.set("total_waves", total);

    if (all_waves_complete_ || current_wave_ >= total) {
        if (!all_waves_complete_) {
            all_waves_complete_ = true;
            blackboard.set("all_waves_complete", true);
        }
        blackboard.set("wave", total);
        return;
    }

    blackboard.set("wave", current_wave_ + 1);  // 1-based for HUD

    float dt = static_cast<float>(blackboard.get<double>("delta_time"));
    const WaveDef& wave = cfg_->waves[static_cast<size_t>(current_wave_)];

    elapsed_time_ += dt;

    // Two wave modes: timed waves spawn for `duration` seconds after the delay,
    // fixed-count waves spawn until `count` enemies exist.
    bool quota_done = wave.duration > 0.0f
        ? (elapsed_time_ - wave.delay) >= wave.duration
        : enemies_spawned_ >= wave.count;

    if (elapsed_time_ >= wave.delay && !quota_done) {
        spawn_timer_ += dt;
        if (spawn_timer_ >= wave.spawn_interval) {
            spawn_timer_ -= wave.spawn_interval;
            spawn_enemy(wave, entity_manager, component_storage);
            if (wave.duration <= 0.0f && enemies_spawned_ >= wave.count) quota_done = true;
        }
    }

    if (!quota_done) return;

    // Iteration 3 (D70): a boss wave is held open by BossSystem until the boss is
    // dead AND its reward has been taken. Placed before the straggler force-kill,
    // which would otherwise execute the boss the moment the adds ran out.
    if (clear_hold_) { stall_timer_ = 0.0f; return; }

    // D4: a wave ends only on a *cleared* arena, so the shop always opens with
    // nothing alive. R3: an enemy that somehow becomes unreachable would soft-lock
    // the run forever, so stragglers are force-killed after the stall timeout
    // (EnemyDeathSystem picks them up on the next frame like any other kill).
    if (!component_storage.entities_with_component<EnemyTag>().empty()) {
        stall_timer_ += dt;
        if (stall_timer_ >= cfg_->wave_stall_timeout) {
            for (Entity e : component_storage.entities_with_component<EnemyTag>()) {
                auto h = component_storage.get_component<Health>(e);
                if (h.has_value()) h->get().current = 0.0f;
            }
        }
        return;
    }

    current_wave_++;
    enemies_spawned_ = 0;
    elapsed_time_ = 0.0f;
    spawn_timer_ = 0.0f;
    stall_timer_ = 0.0f;
    wave_just_cleared_ = true;
    if (current_wave_ >= total) {
        all_waves_complete_ = true;
        blackboard.set("all_waves_complete", true);
    }
}
