#include "enemy_death_system.hpp"
#include "enemy_components.hpp"   // EnemyTag, Health
#include "player_components.hpp"  // ContactDamage, Pickup
#include "collision_layers.hpp"
#include "feedback.hpp"           // add_trauma
#include "engine/ecs/fx_events.hpp"  // engine-suite D138: grid impulses / scar stamps
#include "engine/project_paths.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace {
// Iteration 3 (D68): the Prism splitter's children. Two units at 60% size and
// 40% of the parent's max HP is a net 20% HP gain for the player's trouble, paid
// for with 35% more speed — the split is meant to be an inconvenience, not a
// health bar that doubles.
constexpr float SPLIT_SIZE_FRAC  = 0.6f;
constexpr float SPLIT_HP_FRAC    = 0.4f;
constexpr float SPLIT_SPEED_MULT = 1.35f;

// D193 (playtest #13): every unit is worth this much more than its enemy type
// says. A flat add, not a multiplier — the per-type currency values are small
// ints and the point was to lift the floor, not to widen the spread.
constexpr int CREDIT_BASE_BONUS = 2;

// D193 (playtest #12): the BIG UNIT. From wave BIG_FIRST_WAVE, a kill that lands
// in the middle of a pile-up — BIG_MIN_DEATHS enemies dying in the same frame
// within BIG_RADIUS of each other — pays out in big units instead of small ones.
// // ponytail: "same frame" IS the time window. EnemyDeathSystem already sees
// // every death of the frame in one pass, so the cluster test is a distance
// // count over that list; a rolling multi-frame window would need state.
constexpr int   BIG_FIRST_WAVE = 15;
constexpr int   BIG_MIN_DEATHS = 3;
constexpr float BIG_RADIUS     = 180.0f;
constexpr int   BIG_VALUE      = 15;
constexpr float BIG_SCALE      = 1.6f;   // same chit sprite, just larger

/// Do two axis-aligned boxes overlap? Both are given as centre + half-extents.
bool boxes_overlap(float ax, float ay, float ahw, float ahh,
                   float bx, float by, float bhw, float bhh) {
    return std::fabs(ax - bx) < (ahw + bhw) && std::fabs(ay - by) < (ahh + bhh);
}

/// Centre + half-extents of an entity that has a Position and (usually) a Size.
bool entity_box(ComponentStorage& s, Entity e, float& cx, float& cy, float& hw, float& hh) {
    auto pos = s.get_component<Position>(e);
    if (!pos.has_value()) return false;
    auto sz = s.get_component<Size>(e);
    const float w = sz.has_value() ? sz->get().width : 0.0f;
    const float h = sz.has_value() ? sz->get().height : 0.0f;
    cx = pos->get().x + w * 0.5f;
    cy = pos->get().y + h * 0.5f;
    hw = w * 0.5f;
    hh = h * 0.5f;
    return true;
}
}  // namespace

namespace loot_place {

bool blocked(ComponentStorage& storage, float cx, float cy, float half) {
    float ex, ey, ehw, ehh;

    // Obstacles and hazards both carry a Collider, and its layer bit is the one
    // thing that can never disagree with what the entity actually is. That single
    // test covers arena pillars, the permanent vents, Bio-lab poison patches and
    // mine blasts alike (D69's shared hazard recipe).
    for (Entity e : storage.entities_with_component<Collider>()) {
        auto col = storage.get_component<Collider>(e);
        if (!col.has_value()) continue;
        const uint8_t layer = col->get().layer;
        if ((layer & (layers::OBSTACLE | layers::HAZARD)) == 0) continue;
        if (!entity_box(storage, e, ex, ey, ehw, ehh)) continue;
        if (boxes_overlap(cx, cy, half, half, ex, ey, ehw, ehh)) return true;
    }

    // A deployed mine (D68: EnemyBehavior{MINER, tier 0}) carries no Collider at
    // all — it triggers on proximity — so it needs its own test or a coin would
    // happily sit on top of one.
    for (Entity e : storage.entities_with_component<EnemyBehavior>()) {
        auto beh = storage.get_component<EnemyBehavior>(e);
        if (!beh.has_value()) continue;
        if (beh->get().kind != behavior_kinds::MINER || beh->get().tier != 0) continue;
        if (!entity_box(storage, e, ex, ey, ehw, ehh)) continue;
        if (boxes_overlap(cx, cy, half, half, ex, ey, ehw, ehh)) return true;
    }

    // Other loot, including the coins this very drop has already placed — they
    // are in storage the moment they are created, so a scatter cannot stack.
    for (Entity e : storage.entities_with_component<Pickup>()) {
        if (!entity_box(storage, e, ex, ey, ehw, ehh)) continue;
        if (boxes_overlap(cx, cy, half, half, ex, ey, ehw, ehh)) return true;
    }
    return false;
}

void nudge_free(ComponentStorage& storage, float& x, float& y, float half, float reach) {
    if (!blocked(storage, x, y, half)) return;
    // Golden-angle spiral: evenly spread, never clumped, and completely
    // determined by the starting point — no RNG, so the draw count per kill is
    // the same whether this rejects nothing or everything.
    constexpr float GOLDEN_ANGLE = 2.39996322972865332f;
    for (int k = 1; k <= SEARCH_STEPS; ++k) {
        const float t = static_cast<float>(k) / static_cast<float>(SEARCH_STEPS);
        const float r = reach * std::sqrt(t);
        const float a = GOLDEN_ANGLE * static_cast<float>(k);
        const float nx = x + std::cos(a) * r;
        const float ny = y + std::sin(a) * r;
        if (!blocked(storage, nx, ny, half)) { x = nx; y = ny; return; }
    }
    // ponytail: nothing free within reach — keep the drawn point. Widening the
    // search is a bigger reach constant, not more code.
}

}  // namespace loot_place

const sidecar_loader::LoadedSprite* EnemyDeathSystem::effect_sprite() {
    if (effect_.has_value()) return &effect_.value();
    if (effect_failed_) return nullptr;
    try {
        std::string path = project_paths::assets_dir() + "/images/effect_explosion.json";
        effect_ = sidecar_loader::load(path, "expand");
        return &effect_.value();
    } catch (...) {
        effect_failed_ = true;  // fall back to no death sprite; game keeps running
        return nullptr;
    }
}

void EnemyDeathSystem::drop_loot(ComponentStorage& component_storage,
                                 EntityManager& entity_manager,
                                 float cx, float cy, int currency_value,
                                 float drop_chance, bool big) {
    const EconomyConfig& ec = economy_;
    const int lo = std::max(0, ec.min_drops);
    const int hi = std::max(lo, ec.max_drops);

    // R2: every draw below happens on every kill, in the same order, whatever the
    // outcome. `count` decides how many of the pre-rolled scatter offsets are
    // *used*, never how many are drawn. Phase 5's per-type drop_chance is the same
    // discipline: drawn unconditionally, in a fixed position, and only *then*
    // allowed to decide that nothing drops.
    std::uniform_int_distribution<int> count_dist(lo, hi);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> angle_dist(0.0f, 6.28318530717958647692f);

    const float drop_roll = unit(rng_);
    const int count = count_dist(rng_);
    const float key_roll = unit(rng_);

    // A big unit is the same chit, worth more and drawn larger. Nothing else
    // about it differs — no second sprite, no new PickupKind.
    const float unit_size  = ec.pickup_size * (big ? BIG_SCALE : 1.0f);
    const int   unit_value = big ? BIG_VALUE : currency_value + CREDIT_BASE_BONUS;
    // Lane K (D101): how far a coin may be nudged to get off a hazard. Three
    // scatter radii (~78px at the shipped tuning) clears a 90px arena vent from
    // dead centre, and still keeps the loot visibly "that enemy's". A coin that
    // finds nothing free inside it keeps its drawn spot.
    const float nudge_reach = ec.pickup_scatter * 3.0f;
    auto make_pickup = [&](float x, float y, PickupKind kind, int value,
                           uint8_t r, uint8_t g, uint8_t b,
                           const char* image = nullptr, float size = 0.0f) {
        const float w = size > 0.0f ? size : ec.pickup_size;
        const float h2 = w * 0.5f;
        // Pure, RNG-free placement fix-up. It must stay that way: a draw in here
        // would make the stream depend on how many candidates were rejected.
        loot_place::nudge_free(component_storage, x, y, h2, nudge_reach);
        Entity e = entity_manager.create_entity();
        component_storage.add_component<Position>(e, Position{x - h2, y - h2});
        component_storage.add_component<Size>(e, Size{w, w});
        component_storage.add_component<Color>(e, Color{r, g, b, 255});
        // D95: currency is a struck circular coin, not a gold square. Images wins
        // over Color in the render chain; Color remains the load fallback.
        if (image != nullptr)
            component_storage.add_component<Images>(e, Images{{image}, 0});
        component_storage.add_component<Pickup>(e,
            Pickup{static_cast<int>(kind), value, ec.pickup_magnet_speed});
        component_storage.add_component<Lifetime>(e, Lifetime{ec.pickup_lifetime});
        // D183: identity tint; PickupSystem blinks its alpha over the last 3 s
        // before the lifetime expires. Covers both render paths (texture
        // alpha-mod and modulate_color on the fallback rect).
        component_storage.add_component<Tint>(e, Tint{});
        // Above enemies(2) and the player(3) so loot is never hidden under a corpse.
        component_storage.add_component<RenderLayer>(e, RenderLayer{4});
    };

    const bool drops = drop_roll < drop_chance;

    for (int i = 0; i < hi; ++i) {
        float a = angle_dist(rng_);
        float d = unit(rng_) * ec.pickup_scatter;
        // rolled, not used — the draws still happened. Note the loop is never
        // short-circuited, so a no-drop kill consumes exactly as much of the RNG
        // stream as a paying one.
        if (!drops || i >= count) continue;
        make_pickup(cx + std::cos(a) * d, cy + std::sin(a) * d,
                    PickupKind::Currency, unit_value, 255, 210, 90,
                    "v2/pickup_coin.png", unit_size);
    }

    if (drops && key_roll < ec.key_drop_chance) {
        make_pickup(cx, cy, PickupKind::Key, 1, 255, 130, 245);
    }
}

void EnemyDeathSystem::update(ComponentStorage& component_storage,
                              EntityManager& entity_manager,
                              Blackboard& blackboard) {
    int score = blackboard.get_or<int>("score", 0);

    // D193 #12: pre-pass over this frame's dead, so the loot loop below can ask
    // "how many of them died next to this one?". Reads only Position/Size and
    // draws no RNG, so it cannot move the drop stream (R2).
    const bool big_wave = blackboard.get_or<int>("wave", 0) >= BIG_FIRST_WAVE;
    std::vector<std::pair<float, float>> dead_centres;
    if (big_wave) {
        for (Entity e : component_storage.entities_with_component<EnemyTag>()) {
            auto hp = component_storage.get_component<Health>(e);
            if (!hp.has_value() || hp->get().current > 0.0f) continue;
            if (component_storage.has_component<DestroyRequest>(e)) continue;
            float cx, cy, hw, hh;
            if (entity_box(component_storage, e, cx, cy, hw, hh))
                dead_centres.emplace_back(cx, cy);
        }
    }

    for (Entity enemy : component_storage.entities_with_component<EnemyTag>()) {
        auto health_opt = component_storage.get_component<Health>(enemy);
        if (!health_opt.has_value()) continue;
        if (health_opt->get().current > 0.0f) continue;
        if (component_storage.has_component<DestroyRequest>(enemy)) continue;  // already dead

        // Reward: score now, currency as physical loot below (D5).
        auto cd = component_storage.get_component<ContactDamage>(enemy);
        int currency_value = 1;
        float drop_chance = 1.0f;
        if (cd.has_value()) {
            score += cd->get().score;
            currency_value = cd->get().currency;
            drop_chance = cd->get().drop_chance;
        } else {
            score += 10;
        }

        // Loot drop. Runs before the sprite/particle work so the RNG sequence
        // does not depend on whether the explosion sidecar happened to load (R2).
        {
            float lx = 0.0f, ly = 0.0f;
            if (auto pos = component_storage.get_component<Position>(enemy); pos.has_value()) {
                auto sz = component_storage.get_component<Size>(enemy);
                lx = pos->get().x + (sz.has_value() ? sz->get().width * 0.5f : 0.0f);
                ly = pos->get().y + (sz.has_value() ? sz->get().height * 0.5f : 0.0f);
            }
            // ponytail: O(deaths^2) over one frame's corpses. A wipe is tens of
            // entities; index it only if a profile says so.
            int nearby = 0;
            for (const auto& c : dead_centres)
                if (std::hypot(c.first - lx, c.second - ly) <= BIG_RADIUS) ++nearby;
            const bool big = big_wave && nearby >= BIG_MIN_DEATHS;
            drop_loot(component_storage, entity_manager, lx, ly, currency_value,
                      drop_chance, big);
        }

        // Spawn the one-shot explosion at the enemy's position.
        const sidecar_loader::LoadedSprite* fx = effect_sprite();
        if (fx != nullptr) {
            auto pos = component_storage.get_component<Position>(enemy);
            auto size = component_storage.get_component<Size>(enemy);
            if (pos.has_value()) {
                float w = size.has_value() ? size->get().width : 40.0f;
                float h = size.has_value() ? size->get().height : 40.0f;
                Entity effect = entity_manager.create_entity();
                component_storage.add_component<Position>(effect,
                    Position{pos->get().x, pos->get().y});
                component_storage.add_component<Size>(effect, Size{w, h});
                component_storage.add_component<SpriteSheet>(effect, fx->sprite_sheet);
                component_storage.add_component<Animation>(effect, fx->animation);
                // Total clip time = frame_count * frame_duration.
                float life = static_cast<float>(fx->animation.frame_count) *
                             fx->animation.frame_duration;
                component_storage.add_component<Lifetime>(effect, Lifetime{life});
                component_storage.add_component<RenderLayer>(effect, RenderLayer{4});
            }
        }

        // v2: additive particle burst alongside the explosion clip. A short-lived
        // host with a high-rate radial emitter sprays for a moment, then is
        // destroyed (emitter dies with host; particles fade via their lifetime).
        if (auto pos = component_storage.get_component<Position>(enemy); pos.has_value()) {
            auto sz = component_storage.get_component<Size>(enemy);
            float w = sz.has_value() ? sz->get().width : 40.0f;
            float h = sz.has_value() ? sz->get().height : 40.0f;
            Entity burst = entity_manager.create_entity();
            component_storage.add_component<Position>(burst,
                Position{pos->get().x + w * 0.5f, pos->get().y + h * 0.5f});
            ParticleEmitter e;
            e.shape = EmitterShape::Point;
            e.additive = true;
            e.emission_rate = 500.0f;
            e.particle_lifetime = 0.45f;
            e.min_speed = 60.0f; e.max_speed = 240.0f;
            e.cone_half_angle = 180.0f;
            e.start_r = 255; e.start_g = 200; e.start_b = 120; e.start_a = 255;
            e.end_r = 255;   e.end_g = 60;    e.end_b = 30;    e.end_a = 0;
            e.start_size = 7.0f; e.end_size = 0.0f;
            component_storage.add_component<ParticleEmitter>(burst, e);
            component_storage.add_component<Lifetime>(burst, Lifetime{0.10f});
        }

        // Iteration 3 (D68): the Prism arena's splitter. Two smaller units on
        // death, and only from a tier >= 1 parent — the children carry no
        // EnemyBehavior at all, which is what stops an infinite split.
        //
        // Sited after drop_loot deliberately: drop_loot is the only RNG in this
        // function, the split draws none, and putting the split first would still
        // be safe but would make the ordering rule harder to see (R2).
        if (auto beh = component_storage.get_component<EnemyBehavior>(enemy);
            beh.has_value() && beh->get().kind == behavior_kinds::SPLITTER &&
            beh->get().tier >= 1) {
            auto pos = component_storage.get_component<Position>(enemy);
            auto sz  = component_storage.get_component<Size>(enemy);
            auto hp  = component_storage.get_component<Health>(enemy);
            if (pos.has_value() && sz.has_value() && hp.has_value()) {
                const float w = sz->get().width * SPLIT_SIZE_FRAC;
                const float child_hp = std::max(1.0f, hp->get().max_hp * SPLIT_HP_FRAC);
                const float cx = pos->get().x + sz->get().width * 0.5f;
                const float cy = pos->get().y + sz->get().height * 0.5f;
                const float off = sz->get().width * 0.45f;
                for (int s = -1; s <= 1; s += 2) {
                    Entity child = entity_manager.create_entity();
                    component_storage.add_component<Position>(child,
                        Position{cx + static_cast<float>(s) * off - w * 0.5f, cy - w * 0.5f});
                    component_storage.add_component<Size>(child, Size{w, w});
                    component_storage.add_component<Velocity>(child, Velocity{0.0f, 0.0f});
                    component_storage.add_component<Health>(child, Health{child_hp, child_hp});
                    component_storage.add_component<EnemyTag>(child, EnemyTag{});
                    component_storage.add_component<RenderLayer>(child, RenderLayer{2});
                    component_storage.add_component<Collider>(child,
                        Collider{w, w, layers::ENEMY, layers::ENEMY_MASK});
                    component_storage.add_component<CircleCollider>(child,
                        CircleCollider{w * 0.5f, 0.0f, 0.0f});
                    if (auto pf = component_storage.get_component<PathFollower>(enemy);
                        pf.has_value()) {
                        PathFollower child_pf = pf->get();
                        child_pf.speed *= SPLIT_SPEED_MULT;
                        child_pf.repath_timer = 0.0f;
                        component_storage.add_component<PathFollower>(child, child_pf);
                    }
                    if (auto c = component_storage.get_component<Color>(enemy); c.has_value())
                        component_storage.add_component<Color>(child, c->get());
                    if (auto t = component_storage.get_component<Tint>(enemy); t.has_value())
                        component_storage.add_component<Tint>(child, t->get());
                    if (auto ss = component_storage.get_component<SpriteSheet>(enemy); ss.has_value())
                        component_storage.add_component<SpriteSheet>(child, ss->get());
                    if (auto an = component_storage.get_component<Animation>(enemy); an.has_value())
                        component_storage.add_component<Animation>(child, an->get());
                    ContactDamage child_cd{8.0f, 5, 1, 0.5f};
                    if (cd.has_value()) {
                        child_cd = cd->get();
                        child_cd.amount *= SPLIT_SIZE_FRAC;
                        child_cd.score = std::max(1, child_cd.score / 2);
                        child_cd.currency = std::max(1, child_cd.currency / 2);
                    }
                    component_storage.add_component<ContactDamage>(child, child_cd);
                }
            }
        }

        // v2 Phase 4: every kill adds a little camera trauma.
        blackboard.set<float>("feedback.trauma", feedback::add_trauma(
            blackboard.get_or<float>("feedback.trauma", 0.0f),
            blackboard.get_or<float>("fb.trauma_enemy_death", 0.25f)));

        // Engine suite (D138/D139/D140): a kill is one sim event with two
        // consumers. `sim.kills` is a plain monotonic counter Temporal Overload
        // reads to find a kill chain; the grid impulse and the scorch stamp are
        // the RENDER-ONLY half — published here, read by the grid and the scar
        // layer while drawing, and never read back by anything sim-side.
        // Scaled by the body's size so a hulk shakes the lattice harder.
        blackboard.set<int>("sim.kills", blackboard.get_or<int>("sim.kills", 0) + 1);
        if (auto dpos = component_storage.get_component<Position>(enemy);
            dpos.has_value()) {
            float dw = 40.0f;
            if (auto dsz = component_storage.get_component<Size>(enemy); dsz.has_value())
                dw = dsz->get().width;
            const float dcx = dpos->get().x + dw * 0.5f;
            const float dcy = dpos->get().y + dw * 0.5f;
            const float mag = dw / 40.0f;
            fx_events::push_impulse(blackboard, dcx, dcy, mag);
            fx_events::push_stamp(blackboard, dcx, dcy, /*kind=*/0, mag);
        }

        component_storage.add_component<DestroyRequest>(enemy, DestroyRequest{});
    }

    blackboard.set("score", score);
}
