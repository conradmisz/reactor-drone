#include "wave_spawner_system.hpp"
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

void WaveSpawnerSystem::update(Blackboard& blackboard,
                               EntityManager& entity_manager,
                               ComponentStorage& component_storage) {
    if (!cfg_ || cfg_->waves.empty() || cfg_->enemy_types.empty()) return;
    if (!blackboard.has("delta_time")) return;

    const int total = total_waves();
    blackboard.set("total_waves", total);

    if (all_waves_complete_) {
        blackboard.set("wave", total);
        return;
    }

    if (current_wave_ >= total) {
        all_waves_complete_ = true;
        blackboard.set("all_waves_complete", true);
        blackboard.set("wave", total);
        return;
    }

    blackboard.set("wave", current_wave_ + 1);  // 1-based for HUD

    float dt = static_cast<float>(blackboard.get<double>("delta_time"));
    const WaveDef& wave = cfg_->waves[static_cast<size_t>(current_wave_)];

    elapsed_time_ += dt;
    if (elapsed_time_ < wave.delay) return;

    spawn_timer_ += dt;
    if (spawn_timer_ < wave.spawn_interval) return;
    spawn_timer_ -= wave.spawn_interval;

    // Choose enemy type: cycle the wave's type list, or all types if none listed.
    int type_index;
    if (!wave.types.empty()) {
        type_index = wave.types[static_cast<size_t>(enemies_spawned_) % wave.types.size()];
    } else {
        type_index = enemies_spawned_ % static_cast<int>(cfg_->enemy_types.size());
    }
    type_index = std::max(0, std::min(type_index, static_cast<int>(cfg_->enemy_types.size()) - 1));
    const EnemyType& type = cfg_->enemy_types[static_cast<size_t>(type_index)];

    // Ring spawn: random angle at spawn_radius around the arena center.
    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530717958647692f);
    float angle = angle_dist(rng_);
    float ring_x = cfg_->arena.center_x + cfg_->arena.spawn_radius * std::cos(angle);
    float ring_y = cfg_->arena.center_y + cfg_->arena.spawn_radius * std::sin(angle);
    float half = type.size * 0.5f;

    Entity e = entity_manager.create_entity();
    component_storage.add_component<Position>(e, Position{ring_x - half, ring_y - half});
    component_storage.add_component<Size>(e, Size{type.size, type.size});
    component_storage.add_component<Color>(e, Color{220, 60, 60, 255});  // fallback
    component_storage.add_component<Velocity>(e, Velocity{0.0f, 0.0f});
    const sidecar_loader::LoadedSprite* sprite = resolve_sprite(type.sidecar, type.clip);
    if (sprite != nullptr) {
        component_storage.add_component<SpriteSheet>(e, sprite->sprite_sheet);
        component_storage.add_component<Animation>(e, sprite->animation);
    }
    // PathFollower reused purely as the enemy's seek speed (EnemySeekSystem).
    component_storage.add_component<PathFollower>(e, PathFollower{1, 0.0f, type.speed});
    component_storage.add_component<Health>(e, Health{type.health, type.health});
    component_storage.add_component<EnemyTag>(e, EnemyTag{});
    component_storage.add_component<ContactDamage>(e,
        ContactDamage{type.contact_damage, type.score, type.xp});
    component_storage.add_component<Collider>(e,
        Collider{type.size, type.size, layers::ENEMY, layers::ENEMY_MASK});
    component_storage.add_component<CircleCollider>(e, CircleCollider{half, 0.0f, 0.0f});

    enemies_spawned_++;
    if (enemies_spawned_ >= wave.count) {
        current_wave_++;
        enemies_spawned_ = 0;
        elapsed_time_ = 0.0f;
        spawn_timer_ = 0.0f;
        if (current_wave_ >= total) {
            all_waves_complete_ = true;
            blackboard.set("all_waves_complete", true);
        }
    }
}
