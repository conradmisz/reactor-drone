#include "boss_system.hpp"

#include "active_items.hpp"
#include "collision_layers.hpp"
#include "enemy_components.hpp"
#include "enemy_fire_system.hpp"
#include "hazard_patch.hpp"
#include "item_system.hpp"        // ship_of
#include "player_components.hpp"
#include "specialty_system.hpp"
#include "engine/ecs/systems/screen_stack_system.hpp"
#include "engine/ecs/systems/ui_system.hpp"
#include <algorithm>
#include <cmath>

namespace {

constexpr float TAU = 6.28318530717958647692f;
constexpr const char* SCREEN_BOSS_REWARD = "boss_reward";
/// The three reward buttons, matched as string literals. test_boss.cpp pins these
/// against the widget list in GameData.json — the intermission's contract test
/// idiom, and the only thing that keeps a renamed widget from going silent.
constexpr const char* PICK_FN[3] = {"on_active_pick_0", "on_active_pick_1",
                                    "on_active_pick_2"};
constexpr const char* WIDGET[3] = {"reward_0", "reward_1", "reward_2"};

/// 0-based ordinal of `wave` among the boss waves, and whether it is the last.
int boss_ordinal(const std::vector<WaveDef>& waves, int wave, bool& is_final) {
    int n = 0, total = 0, mine = 0;
    for (size_t i = 0; i < waves.size(); ++i) {
        if (!waves[i].boss) continue;
        if (static_cast<int>(i) + 1 == wave) mine = n;
        if (static_cast<int>(i) + 1 <= wave) ++n;
        ++total;
    }
    is_final = (n == total);
    return mine;
}

bool centre_of(ComponentStorage& s, Entity e, float& cx, float& cy) {
    auto p = s.get_component<Position>(e);
    auto z = s.get_component<Size>(e);
    if (!p.has_value() || !z.has_value()) return false;
    cx = p->get().x + z->get().width * 0.5f;
    cy = p->get().y + z->get().height * 0.5f;
    return true;
}

}  // namespace

void BossSystem::reset() {
    spawned_wave_ = -1;
    boss_ = 0;
    boss_alive_ = false;
    reward_open_ = false;
    shift_requested_ = false;
    final_boss_ = false;
    offer_.clear();
}

void BossSystem::spawn_boss(ComponentStorage& storage, EntityManager& entity_manager,
                            Blackboard& blackboard, int wave, int boss_index,
                            bool final_boss) {
    const BossConfig& bc = cfg_->boss;
    const ArenaDef* arena = nullptr;
    const int ai = active_arena_index(cfg_->arenas, wave);
    if (ai >= 0) arena = &cfg_->arenas[static_cast<size_t>(ai)];

    float hp = bc.health * std::pow(bc.health_growth, static_cast<float>(boss_index));
    if (final_boss) hp *= bc.final_mult;

    // Sited on the arena's spawn ring at a fixed angle relative to the drone, so
    // it always arrives off to one side rather than on top of the player, and so
    // the placement makes no RNG draw.
    float px = cfg_->arena.center_x, py = cfg_->arena.center_y;
    enemy_fire::player_centre(storage, px, py);
    const Vec2 at = ring_spawn_point(px, py, TAU * 0.25f,
                                     cfg_->arena.spawn_radius * 1.2f,
                                     cfg_->arena.center_x, cfg_->arena.center_y,
                                     cfg_->arena.radius);

    const float size = bc.size;
    const float half = size * 0.5f;
    Entity b = entity_manager.create_entity();
    storage.add_component<Position>(b, Position{at.x - half, at.y - half});
    storage.add_component<Size>(b, Size{size, size});
    storage.add_component<Velocity>(b, Velocity{0.0f, 0.0f});
    storage.add_component<Health>(b, Health{hp, hp});
    storage.add_component<EnemyTag>(b, EnemyTag{});
    storage.add_component<RenderLayer>(b, RenderLayer{3});
    storage.add_component<Collider>(b, Collider{size, size, layers::ENEMY, layers::ENEMY_MASK});
    storage.add_component<CircleCollider>(b, CircleCollider{half, 0.0f, 0.0f});
    storage.add_component<ContactDamage>(b,
        ContactDamage{bc.contact_damage, 500 * (boss_index + 1), 40 * (boss_index + 1), 1.0f});
    // Slow: a capital ship, not a runner. The fight is about its adds and its
    // signature attack, and a boss that can chase is a boss that ends the run.
    storage.add_component<PathFollower>(b, PathFollower{1, 0.0f, 34.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    // Themed to the live arena: it wears that arena's enemy tint and borrows that
    // arena's specialty attack (below). `tier` carries the borrowed kind so the
    // per-frame tick needs no second lookup.
    int signature = behavior_kinds::SHOOTER;
    if (arena != nullptr) {
        storage.add_component<Color>(b, Color{arena->enemy_r, arena->enemy_g, arena->enemy_b, 255});
        storage.add_component<Tint>(b, Tint{arena->enemy_r, arena->enemy_g, arena->enemy_b, 255, false});
        if (arena->specialty_unit >= 0 &&
            arena->specialty_unit < static_cast<int>(cfg_->enemy_types.size())) {
            signature = enemy_fire::behavior_kind_for(
                cfg_->enemy_types[static_cast<size_t>(arena->specialty_unit)].behavior);
        }
    }
    // ponytail: the hulk plate stood in for a capital ship. A real
    // battlecruiser sprite is offline generator work (assets/generator/v2).
    storage.add_component<Images>(b, Images{{"v2/enemy_hulk.png"}, 0});
    storage.add_component<EnemyBehavior>(b,
        EnemyBehavior{behavior_kinds::BOSS, signature, bc.summon_interval,
                      bc.summon_interval, 0.0f});

    ParticleEmitter aura;
    aura.shape = EmitterShape::Circle;
    aura.radius = half;
    aura.additive = true;
    aura.emission_rate = 90.0f;      // ~70 live particles for the whole fight
    aura.particle_lifetime = 0.8f;
    aura.min_speed = 0.0f; aura.max_speed = 40.0f;
    aura.cone_half_angle = 180.0f;
    aura.start_size = 10.0f; aura.end_size = 0.0f;
    aura.start_r = arena != nullptr ? arena->enemy_r : 255;
    aura.start_g = arena != nullptr ? arena->enemy_g : 120;
    aura.start_b = arena != nullptr ? arena->enemy_b : 60;
    aura.start_a = 200;
    aura.end_r = 60; aura.end_g = 10; aura.end_b = 40; aura.end_a = 0;
    aura.offset_x = half; aura.offset_y = half;
    storage.add_component<ParticleEmitter>(b, aura);

    boss_ = b;
    boss_alive_ = true;
    final_boss_ = final_boss;
    blackboard.set<std::string>("hud_message",
        (arena != nullptr ? arena->name : std::string("Reactor")) + " capital ship inbound");
    blackboard.set<float>("hud_message_timer", 4.0f);
}

void BossSystem::open_reward(ComponentStorage& storage, Blackboard& blackboard) {
    offer_.clear();
    Entity player = 0;
    ShipState* ship = items::ship_of(storage, player);
    const int held = ship != nullptr ? ship->active_id : -1;

    // The held active first (picking it is the upgrade), then the ones the player
    // does not own. Later bosses therefore always have something to offer.
    if (held >= 0) {
        for (size_t i = 0; i < cfg_->actives.size(); ++i)
            if (actives::active_id_for(cfg_->actives[i].effect) == held)
                offer_.push_back(static_cast<int>(i));
    }
    for (size_t i = 0; i < cfg_->actives.size(); ++i) {
        if (actives::active_id_for(cfg_->actives[i].effect) == held) continue;
        offer_.push_back(static_cast<int>(i));
    }
    if (static_cast<int>(offer_.size()) > cfg_->boss.reward_choices)
        offer_.resize(static_cast<size_t>(cfg_->boss.reward_choices));
    if (offer_.empty()) return;

    // Labels are rewritten from the catalogue every time the screen is pushed —
    // the menu_ship pattern, resolved by name through ui.widget_id.<name>, which
    // is the HUD's existing lookup and deliberately not a second mechanism.
    for (size_t i = 0; i < 3; ++i) {
        const double v = blackboard.get_or<double>(
            std::string("ui.widget_id.") + WIDGET[i], -1.0);
        if (v < 0.0) continue;
        auto el = storage.get_component<UIElement>(static_cast<Entity>(v));
        if (!el.has_value()) continue;
        if (i >= offer_.size()) { el->get().label_text.clear(); continue; }
        const ActiveItemDef& d = cfg_->actives[static_cast<size_t>(offer_[i])];
        el->get().label_text =
            (actives::active_id_for(d.effect) == held ? "UPGRADE " : "") + d.name;
    }

    blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH, std::string(SCREEN_BOSS_REWARD));
    reward_open_ = true;
}

bool BossSystem::handle_reward_click(ComponentStorage& storage, Blackboard& blackboard) {
    const std::string click =
        blackboard.get_or<std::string>(UISystem::UI_CLICK_KEY, std::string());
    if (click.empty()) return false;
    for (size_t i = 0; i < 3; ++i) {
        if (click != PICK_FN[i]) continue;
        blackboard.remove(UISystem::UI_CLICK_KEY);
        if (i >= offer_.size()) return false;
        const ActiveItemDef& d = cfg_->actives[static_cast<size_t>(offer_[i])];
        const int id = actives::active_id_for(d.effect);
        Entity player = 0;
        if (ShipState* ship = items::ship_of(storage, player)) {
            if (ship->active_id == id) {
                // Re-picking what you already hold is the upgrade: a shorter
                // cooldown, compounding, floored so it can never reach zero.
                const float m = blackboard.get_or<float>("ship.active_cd_mult", 1.0f);
                blackboard.set<float>("ship.active_cd_mult", std::max(0.35f, m * 0.75f));
            } else {
                ship->active_id = id;
                ship->active_cd = 0.0f;
            }
        }
        blackboard.set<std::string>("hud_message", d.name + " installed  —  press E");
        blackboard.set<float>("hud_message_timer", 4.0f);
        blackboard.set<bool>(ScreenStackSystem::CMD_POP, true);
        return true;
    }
    return false;
}

void BossSystem::update(ComponentStorage& storage, EntityManager& entity_manager,
                        Blackboard& blackboard, WaveSpawnerSystem& spawner) {
    if (cfg_ == nullptr || cfg_->waves.empty()) return;
    const int wave = spawner.current_wave_index() + 1;
    // A restarted run rewinds the wave counter; nothing else does.
    if (wave < spawned_wave_) { reset(); spawner.set_clear_hold(false); }
    if (wave < 1 || wave > static_cast<int>(cfg_->waves.size())) return;
    if (!cfg_->waves[static_cast<size_t>(wave) - 1].boss) return;

    if (spawned_wave_ != wave) {
        bool is_final = false;
        const int ordinal = boss_ordinal(cfg_->waves, wave, is_final);
        spawn_boss(storage, entity_manager, blackboard, wave, ordinal, is_final);
        spawned_wave_ = wave;
        spawner.set_clear_hold(true);
    }

    const float dt = static_cast<float>(blackboard.get_or<double>("delta_time", 0.0));

    if (boss_alive_) {
        auto hp = storage.get_component<Health>(boss_);
        const bool dead = !entity_manager.is_alive(boss_) || !hp.has_value() ||
                          hp->get().current <= 0.0f ||
                          storage.has_component<DestroyRequest>(boss_);
        if (dead) {
            boss_alive_ = false;
            open_reward(storage, blackboard);
            if (!reward_open_) spawner.set_clear_hold(false);   // no actives authored
        } else {
            // === SEAM: wave-50 mid-fight arena shift ===
            // The user's wave-50 boss "shifts the arena" into the Singularity map
            // mid-fight. That transition is Lane E's crossfade, which today only
            // fires on a *cleared* wave (main.cpp keys it on wave_just_cleared),
            // so there is no callable mid-wave entry point to reuse — and building
            // a second transition is exactly what this lane was told not to do.
            // BossSystem therefore only RAISES THE REQUEST here; nothing consumes
            // it. The integrator wires wants_arena_shift() into the arena-shift
            // path after Lane E merges. Until then the Singularity arena is still
            // reached, one wave edge earlier, via its first_wave: 50.
            if (final_boss_ && !shift_requested_ && hp.has_value() &&
                hp->get().current <= hp->get().max_hp * cfg_->boss.shift_hp_frac) {
                shift_requested_ = true;
            }
            // === END SEAM ===

            auto beh = storage.get_component<EnemyBehavior>(boss_);
            if (beh.has_value()) {
                if (beh->get().timer > 0.0f) {
                    beh->get().timer -= dt;
                } else {
                    beh->get().timer = beh->get().cooldown;
                    float bx, by;
                    if (centre_of(storage, boss_, bx, by)) {
                        const BossConfig& bc = cfg_->boss;
                        const int adds = bc.summon_count +
                                         (final_boss_ ? bc.final_summon_bonus : 0);
                        // Adds on fixed angles: no RNG in a boss fight, so the
                        // replay stream is untouched by one (R2).
                        for (int i = 0; i < adds; ++i) {
                            const float a = TAU * static_cast<float>(i) /
                                            static_cast<float>(std::max(1, adds));
                            const float r = cfg_->boss.size * 0.75f;
                            Entity add = entity_manager.create_entity();
                            const EnemyType& t = cfg_->enemy_types[0];
                            const float sz = t.size;
                            storage.add_component<Position>(add,
                                Position{bx + std::cos(a) * r - sz * 0.5f,
                                         by + std::sin(a) * r - sz * 0.5f});
                            storage.add_component<Size>(add, Size{sz, sz});
                            storage.add_component<Velocity>(add, Velocity{0.0f, 0.0f});
                            storage.add_component<Health>(add, Health{t.health, t.health});
                            storage.add_component<EnemyTag>(add, EnemyTag{});
                            storage.add_component<RenderLayer>(add, RenderLayer{2});
                            storage.add_component<PathFollower>(add,
                                PathFollower{1, 0.0f, t.speed, 0.0f, 0.0f, 0.0f, 0.0f});
                            storage.add_component<ContactDamage>(add,
                                ContactDamage{t.contact_damage, t.score, t.currency,
                                              t.drop_chance});
                            storage.add_component<Collider>(add,
                                Collider{sz, sz, layers::ENEMY, layers::ENEMY_MASK});
                            storage.add_component<CircleCollider>(add,
                                CircleCollider{sz * 0.5f, 0.0f, 0.0f});
                            if (auto c = storage.get_component<Color>(boss_); c.has_value()) {
                                storage.add_component<Color>(add, c->get());
                                storage.add_component<Tint>(add,
                                    Tint{c->get().r, c->get().g, c->get().b, 255, false});
                            }
                        }

                        // The borrowed signature attack, on the same timer as the
                        // summon: one clock, two effects, and the arena's identity
                        // arrives with its adds rather than on its own rhythm.
                        const int sig = beh->get().tier;
                        if (sig == behavior_kinds::SPITTER) {
                            hazard::PatchSpec p;
                            p.size = specialty::PATCH_SIZE * 1.4f;
                            p.lifetime = specialty::PATCH_LIFETIME * 1.5f;
                            p.damage = cfg_->boss.contact_damage * 0.4f;
                            for (int i = 0; i < 3; ++i) {
                                const float a = TAU * static_cast<float>(i) / 3.0f;
                                hazard::spawn_patch(storage, entity_manager,
                                    bx + std::cos(a) * cfg_->boss.size * 0.6f,
                                    by + std::sin(a) * cfg_->boss.size * 0.6f, p);
                            }
                        } else if (sig == behavior_kinds::MINER) {
                            hazard::PatchSpec p;
                            p.size = specialty::MINE_BLAST_SIZE;
                            p.lifetime = specialty::MINE_LIFETIME * 0.4f;
                            p.damage = cfg_->boss.contact_damage * 0.5f;
                            p.r = 255; p.g = 150; p.b = 50;
                            p.emission_rate = 20.0f;
                            for (int i = 0; i < 4; ++i) {
                                const float a = TAU * static_cast<float>(i) / 4.0f + 0.4f;
                                hazard::spawn_patch(storage, entity_manager,
                                    bx + std::cos(a) * cfg_->boss.size * 0.8f,
                                    by + std::sin(a) * cfg_->boss.size * 0.8f, p);
                            }
                        } else {
                            // Bulwark, splitter and the default all fall through to
                            // a radial volley — the one attack every arena can read.
                            for (int i = 0; i < 10; ++i) {
                                const float a = TAU * static_cast<float>(i) / 10.0f +
                                                beh->get().aim;
                                enemy_fire::spawn_shot(storage, entity_manager, bx, by, a,
                                                       240.0f,
                                                       cfg_->boss.contact_damage * 0.35f, 1);
                            }
                            beh->get().aim += 0.31f;   // rotate the volley each time
                        }
                    }
                }
            }
        }
    }

    if (reward_open_ && handle_reward_click(storage, blackboard)) {
        reward_open_ = false;
        offer_.clear();
        spawner.set_clear_hold(false);   // the boss wave may now finish
    }
}
