/**
 * test_boss.cpp — the every-10th-wave boss, its reward, and the three actives
 * (#4, D70-D75).
 *
 * The failures this pins are all invisible to the compiler:
 *   - a boss on a wave that is not a boss wave, or none on one that is;
 *   - a boss wave that clears before the boss dies (the spawner's clear hold);
 *   - a reward screen whose buttons carry callback names boss_system.cpp does not
 *     compare against — a screen that builds, renders and does nothing forever;
 *   - a boss that ignores the arena it is standing in;
 *   - an active that fires every frame because the cooldown was never written;
 *   - a forcefield that fires at exactly the threshold instead of below it.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>
#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/systems/screen_stack_system.hpp"
#include "engine/ecs/systems/ui_render_math.hpp"
#include "engine/ecs/systems/ui_system.hpp"
#include "engine/gamedata_loader.hpp"
#include "engine/project_paths.hpp"
#include "engine/ui_style.hpp"
#include "game/active_items.hpp"
#include "game/arena_config.hpp"
#include "game/boss_system.hpp"
#include "game/enemy_components.hpp"
#include "game/enemy_fire_system.hpp"
#include "game/player_components.hpp"
#include "game/wave_spawner_system.hpp"

using Catch::Matchers::WithinAbs;

namespace {

// The literals boss_system.cpp compares UISystem::UI_CLICK_KEY against.
constexpr const char* kPickFn[3] = {"on_active_pick_0", "on_active_pick_1",
                                    "on_active_pick_2"};
constexpr const char* kScreenName = "boss_reward";

struct BossWorld {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    GameConfig cfg;
    WaveSpawnerSystem spawner;
    BossSystem boss;

    explicit BossWorld(bool boss_wave) {
        bb.set<double>("delta_time", 0.25);
        cfg.enemy_types.push_back(EnemyType{});
        WaveDef w;
        w.boss = boss_wave;
        cfg.waves.push_back(w);
        cfg.boss.health = 100.0f;
        cfg.boss.summon_interval = 1.0f;
        cfg.boss.summon_count = 3;
        // One boss wave means it is also the LAST one, so the finale bonus would
        // otherwise be folded into every count here. It gets its own case below.
        cfg.boss.final_summon_bonus = 0;
        cfg.boss.final_mult = 1.0f;
        for (const char* e : {"missiles", "laser", "repulsor_field"}) {
            ActiveItemDef d;
            d.name = e;
            d.effect = e;
            cfg.actives.push_back(d);
        }
        spawner.set_config(&cfg);
        boss.set_config(&cfg);
    }

    Entity player() {
        Entity p = em.create_entity();
        cs.add_component<Position>(p, Position{0.0f, 0.0f});
        cs.add_component<Size>(p, Size{40.0f, 40.0f});
        cs.add_component<PlayerTag>(p, PlayerTag{});
        cs.add_component<Health>(p, Health{100.0f, 100.0f});
        cs.add_component<ShipState>(p, ShipState{});
        return p;
    }

    Entity find_boss() const {
        for (Entity e : cs.entities_with_component<EnemyBehavior>()) {
            auto b = cs.get_component<EnemyBehavior>(e);
            if (b.has_value() && b->get().kind == behavior_kinds::BOSS) return e;
        }
        return 0;
    }

    size_t enemies() const { return cs.entities_with_component<EnemyTag>().size(); }
};

struct LoadedWorld {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    LoadedWorld() {
        load_game_data(project_paths::assets_dir() + "/GameData.json", em, cs, bb);
    }
    std::vector<Entity> widgets() const {
        std::vector<Entity> out;
        for (Entity e : cs.entities_with_component<UIElement>()) {
            auto m = cs.get_component<ScreenMembership>(e);
            if (m.has_value() && m->get().screen_name == kScreenName) out.push_back(e);
        }
        return out;
    }
};

}  // namespace

TEST_CASE("only a boss wave spawns a boss", "[Game][boss]") {
    {
        BossWorld w(false);
        w.player();
        w.boss.update(w.cs, w.em, w.bb, w.spawner);
        CHECK(w.find_boss() == 0);
        CHECK_FALSE(w.spawner.clear_hold());
    }
    BossWorld w(true);
    w.player();
    w.boss.update(w.cs, w.em, w.bb, w.spawner);
    Entity b = w.find_boss();
    REQUIRE(b != 0);
    CHECK(w.boss.boss_alive());
    // The wave is held open the moment the boss exists — that, not a special
    // clear rule, is "the wave clears when the boss dies".
    CHECK(w.spawner.clear_hold());
    // Exactly one, and it stays one across frames.
    w.boss.update(w.cs, w.em, w.bb, w.spawner);
    CHECK(w.find_boss() == b);

    auto h = w.cs.get_component<Health>(b);
    REQUIRE(h.has_value());
    CHECK(h->get().max_hp >= 100.0f);
    CHECK(w.cs.has_component<EnemyTag>(b));
}

TEST_CASE("the boss summons adds on its interval, not every frame", "[Game][boss]") {
    BossWorld w(true);
    w.player();
    w.boss.update(w.cs, w.em, w.bb, w.spawner);
    REQUIRE(w.enemies() == 1);   // the boss itself

    // summon_interval 1.0s at dt 0.25: three ticks are not enough.
    for (int i = 0; i < 3; ++i) w.boss.update(w.cs, w.em, w.bb, w.spawner);
    CHECK(w.enemies() == 1);
    w.boss.update(w.cs, w.em, w.bb, w.spawner);
    CHECK(w.enemies() == 1 + 3);
    // ...and it re-arms rather than summoning again immediately.
    w.boss.update(w.cs, w.em, w.bb, w.spawner);
    CHECK(w.enemies() == 1 + 3);
}

TEST_CASE("the last boss wave is a harder fight, not just a bigger one",
          "[Game][boss]") {
    // Wave 50 (the user: "extra hard, spawns lots of enemies, hits hard") is the
    // last boss wave, so it takes final_mult on HP and final_summon_bonus adds.
    BossWorld w(true);
    w.cfg.boss.final_mult = 2.0f;
    w.cfg.boss.final_summon_bonus = 4;
    w.player();
    w.boss.update(w.cs, w.em, w.bb, w.spawner);
    Entity b = w.find_boss();
    REQUIRE(b != 0);
    CHECK_THAT(w.cs.get_component<Health>(b)->get().max_hp, WithinAbs(200.0f, 1e-2f));

    for (int i = 0; i < 4; ++i) w.boss.update(w.cs, w.em, w.bb, w.spawner);
    CHECK(w.enemies() == 1 + 3 + 4);
}

TEST_CASE("the boss's kill releases the wave only once the reward is taken",
          "[Game][boss]") {
    BossWorld w(true);
    Entity p = w.player();
    w.boss.update(w.cs, w.em, w.bb, w.spawner);
    Entity b = w.find_boss();
    REQUIRE(b != 0);

    w.cs.get_component<Health>(b)->get().current = 0.0f;
    w.boss.update(w.cs, w.em, w.bb, w.spawner);

    CHECK_FALSE(w.boss.boss_alive());
    CHECK(w.boss.reward_open());
    CHECK(w.spawner.clear_hold());   // still held: the pick has not happened
    CHECK(w.bb.get_or<std::string>(ScreenStackSystem::CMD_PUSH, std::string()) ==
          std::string(kScreenName));
    CHECK(w.boss.offer().size() == 3);

    // Take the second offer.
    const int chosen = w.boss.offer()[1];
    w.bb.set<std::string>(UISystem::UI_CLICK_KEY, std::string(kPickFn[1]));
    w.boss.update(w.cs, w.em, w.bb, w.spawner);

    CHECK_FALSE(w.boss.reward_open());
    CHECK_FALSE(w.spawner.clear_hold());
    auto s = w.cs.get_component<ShipState>(p);
    REQUIRE(s.has_value());
    CHECK(s->get().active_id ==
          actives::active_id_for(w.cfg.actives[static_cast<size_t>(chosen)].effect));
    // The click must be consumed, or the intermission underneath re-reads it.
    CHECK(w.bb.get_or<std::string>(UISystem::UI_CLICK_KEY, std::string()).empty());
}

TEST_CASE("a later boss offers the held active as an upgrade first", "[Game][boss]") {
    BossWorld w(true);
    Entity p = w.player();
    w.cs.get_component<ShipState>(p)->get().active_id = actives::ids::LASER;

    w.boss.update(w.cs, w.em, w.bb, w.spawner);
    w.cs.get_component<Health>(w.find_boss())->get().current = 0.0f;
    w.boss.update(w.cs, w.em, w.bb, w.spawner);

    REQUIRE_FALSE(w.boss.offer().empty());
    CHECK(actives::active_id_for(
              w.cfg.actives[static_cast<size_t>(w.boss.offer()[0])].effect) ==
          actives::ids::LASER);

    // Re-picking it is the upgrade: a shorter cooldown, not a second copy.
    w.bb.set<std::string>(UISystem::UI_CLICK_KEY, std::string(kPickFn[0]));
    w.boss.update(w.cs, w.em, w.bb, w.spawner);
    CHECK(w.cs.get_component<ShipState>(p)->get().active_id == actives::ids::LASER);
    CHECK(w.bb.get_or<float>("ship.active_cd_mult", 1.0f) < 1.0f);
}

TEST_CASE("the boss is themed to the arena it spawns in", "[Game][boss]") {
    BossWorld w(true);
    ArenaDef foundry;
    foundry.name = "Foundry";
    foundry.first_wave = 1;
    foundry.enemy_r = 250; foundry.enemy_g = 140; foundry.enemy_b = 40;
    EnemyType miner;
    miner.name = "foundry_miner";
    miner.behavior = "miner";
    w.cfg.enemy_types.push_back(miner);
    foundry.specialty_unit = static_cast<int>(w.cfg.enemy_types.size()) - 1;
    w.cfg.arenas.push_back(foundry);

    w.player();
    w.boss.update(w.cs, w.em, w.bb, w.spawner);
    Entity b = w.find_boss();
    REQUIRE(b != 0);

    auto tint = w.cs.get_component<Tint>(b);
    REQUIRE(tint.has_value());
    CHECK(tint->get().r == 250);
    CHECK(tint->get().g == 140);
    CHECK(tint->get().b == 40);
    // ...and it borrows that arena's signature attack, carried on `tier`.
    auto beh = w.cs.get_component<EnemyBehavior>(b);
    REQUIRE(beh.has_value());
    CHECK(beh->get().tier == behavior_kinds::MINER);
}

TEST_CASE("the shipped final-wave arena and the hard-mode boss knob are authored",
          "[Game][boss][config]") {
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");

    // The LAST arena: the final-wave void (wave 30 since Lane O, D125). Indexed
    // off size() rather than at a literal 8/9 — roguelite phase 5 inserted two
    // themes ahead of it, and what matters here is that the finale is last.
    REQUIRE(cfg.arenas.size() == 11);
    const ArenaDef& fin = cfg.arenas.back();
    CHECK(fin.name == "Singularity");
    CHECK(fin.first_wave == 30);
    CHECK(active_arena_index(cfg.arenas, 30) == static_cast<int>(cfg.arenas.size()) - 1);
    CHECK(active_arena_index(cfg.arenas, 29) != 8);
    CHECK_FALSE(fin.backdrop_layers.empty());
    CHECK_FALSE(fin.obstacles.empty());

    // Boss waves exist and the last one is the last wave of the arc.
    std::vector<int> boss_waves;
    for (size_t i = 0; i < cfg.waves.size(); ++i)
        if (cfg.waves[i].boss) boss_waves.push_back(static_cast<int>(i) + 1);
    CHECK(boss_waves == std::vector<int>{10, 20, 30});

    // D73: Hard's one boss knob, scaled in apply_difficulty and nowhere else.
    REQUIRE(cfg.difficulties.size() >= 2);
    CHECK(cfg.difficulties[1].boss_mult > 1.0f);
    const float base_hp = cfg.boss.health;
    const float base_dmg = cfg.boss.contact_damage;
    apply_difficulty(cfg, cfg.difficulties[1]);
    CHECK(cfg.boss.health > base_hp);
    CHECK(cfg.boss.contact_damage > base_dmg);
}

// --- the reward screen contract ------------------------------------------

TEST_CASE("the boss_reward screen exists and boots inactive", "[Game][boss][ui]") {
    LoadedWorld w;
    bool found = false;
    for (Entity e : w.cs.entities_with_component<UIScreen>()) {
        auto s = w.cs.get_component<UIScreen>(e);
        if (!s.has_value() || s->get().screen_name != kScreenName) continue;
        found = true;
        REQUIRE_FALSE(s->get().active);
    }
    REQUIRE(found);
    REQUIRE_FALSE(w.widgets().empty());
}

TEST_CASE("every reward button carries the callback name boss_system.cpp compares to",
          "[Game][boss][ui]") {
    LoadedWorld w;
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");

    // One button per offered choice, or a boss kill hands out fewer actives than
    // the config promises.
    REQUIRE(static_cast<int>(cfg.actives.size()) >= cfg.boss.reward_choices);
    // Gameplay pack (D221): the catalog is trimmed to Heat-Seeking Missiles
    // (more boss items are a logged TODO), so one choice is the shipped shape.
    REQUIRE(cfg.boss.reward_choices >= 1);

    auto table = w.bb.get_or<std::shared_ptr<StyleTable>>("ui_styles", nullptr);
    REQUIRE(table != nullptr);

    for (int i = 0; i < 3; ++i) {
        bool found = false;
        for (Entity e : w.widgets()) {
            auto el = w.cs.get_component<UIElement>(e);
            if (!el.has_value() || el->get().on_click_fn != kPickFn[i]) continue;
            found = true;
            const UIElement& ui = el->get();
            INFO("reward button " << i);
            CHECK(ui.element_type == "button");
            // BossSystem rewrites the label per boss, resolving the widget by
            // name through ui.widget_id.<name> — the HUD's lookup. If the loader
            // never published that key, the labels stay blank forever.
            const std::string key = "ui.widget_id.reward_" + std::to_string(i);
            INFO("blackboard key: " << key);
            CHECK(w.bb.get_or<double>(key, -1.0) == static_cast<double>(e));
            CHECK(table->contains(ui.style_id));
            CHECK(ui.rect.x >= 0.0f);
            CHECK(ui.rect.y >= 0.0f);
            CHECK(ui.rect.x + ui.rect.w <= UI_DESIGN_WIDTH);
            CHECK(ui.rect.y + ui.rect.h <= UI_DESIGN_HEIGHT);
            CHECK(ui.rect.w >= 100.0f);
            CHECK(ui.rect.h >= 30.0f);
        }
        INFO("missing reward callback: " << kPickFn[i]);
        CHECK(found);
    }

    // Non-overlapping, or one button steals another's clicks.
    std::vector<UIRect> rects;
    for (Entity e : w.widgets()) {
        auto el = w.cs.get_component<UIElement>(e);
        if (el.has_value() && el->get().element_type == "button")
            rects.push_back(el->get().rect);
    }
    REQUIRE(rects.size() == 3);
    for (size_t i = 0; i < rects.size(); ++i)
        for (size_t j = i + 1; j < rects.size(); ++j)
            CHECK((rects[i].x + rects[i].w <= rects[j].x ||
                   rects[j].x + rects[j].w <= rects[i].x));
}

// --- the actives ----------------------------------------------------------

TEST_CASE("an active fires once and then sits on its cooldown", "[Game][actives]") {
    BossWorld w(false);
    Entity p = w.player();
    w.cs.get_component<ShipState>(p)->get().active_id = actives::ids::MISSILES;
    w.cfg.actives[0].cooldown = 30.0f;
    w.cfg.actives[0].duration = 4.0f;

    actives::tick(w.cs, w.em, w.bb, w.cfg, /*fire=*/true);
    const size_t after_first = w.cs.entities_with_component<ProjectileTag>().size();
    CHECK(after_first == 8);            // 8 radial missiles, wave one
    CHECK_THAT(w.cs.get_component<ShipState>(p)->get().active_cd,
               WithinAbs(30.0f, 1e-3f));

    // dt is 0.25 here, so wave two (0.45 s later) is still pending on this tick.
    actives::tick(w.cs, w.em, w.bb, w.cfg, /*fire=*/true);
    CHECK(w.cs.entities_with_component<ProjectileTag>().size() == after_first);

    // ...and lands on the next one — off the salvo timer, not off the key.
    actives::tick(w.cs, w.em, w.bb, w.cfg, /*fire=*/false);
    CHECK(w.cs.entities_with_component<ProjectileTag>().size() == 16);

    // Neither wave re-fires: the key is still down but the cooldown is not spent.
    actives::tick(w.cs, w.em, w.bb, w.cfg, /*fire=*/true);
    CHECK(w.cs.entities_with_component<ProjectileTag>().size() == 16);

    // Not pressing the key does not fire it either.
    w.cs.get_component<ShipState>(p)->get().active_cd = 0.0f;
    actives::tick(w.cs, w.em, w.bb, w.cfg, /*fire=*/false);
    CHECK(w.cs.entities_with_component<ProjectileTag>().size() == 16);
}

TEST_CASE("the repulsion device fires BELOW 20% hull, not at it", "[Game][actives]") {
    // Pure boundary: at exactly the threshold the drone still owns its panic
    // button. A `<=` here would spend it for the player.
    CHECK_FALSE(actives::field_should_fire(20.0f, 100.0f, 0.0f));
    CHECK(actives::field_should_fire(19.99f, 100.0f, 0.0f));
    CHECK_FALSE(actives::field_should_fire(19.99f, 100.0f, 0.1f));   // on cooldown
    CHECK_FALSE(actives::field_should_fire(0.0f, 100.0f, 0.0f));     // already dead
    CHECK_FALSE(actives::field_should_fire(1.0f, 0.0f, 0.0f));       // no max hull

    BossWorld w(false);
    Entity p = w.player();
    w.cs.get_component<ShipState>(p)->get().active_id = actives::ids::REPULSOR_FIELD;
    w.cfg.actives[2].duration = 5.0f;
    w.cfg.actives[2].amount = 400.0f;

    // At exactly 20 hull of 100: nothing happens, no key pressed or otherwise.
    w.cs.get_component<Health>(p)->get().current = 20.0f;
    actives::tick(w.cs, w.em, w.bb, w.cfg, false);
    CHECK_THAT(w.cs.get_component<Health>(p)->get().current, WithinAbs(20.0f, 1e-3f));

    // One point lower and it heals to full by itself.
    w.cs.get_component<Health>(p)->get().current = 19.0f;
    actives::tick(w.cs, w.em, w.bb, w.cfg, false);
    CHECK_THAT(w.cs.get_component<Health>(p)->get().current, WithinAbs(100.0f, 1e-3f));
    CHECK(w.cs.get_component<ShipState>(p)->get().active_cd > 0.0f);
    CHECK(w.bb.get_or<float>("active.field_t", 0.0f) > 0.0f);
}

TEST_CASE("the laser's hit test is a forward ray", "[Game][actives]") {
    // On the ray, 100 units out.
    CHECK_THAT(actives::ray_distance(0, 0, 0.0f, 600.0f, 100.0f, 0.0f),
               WithinAbs(0.0f, 1e-3f));
    // Beside it.
    CHECK_THAT(actives::ray_distance(0, 0, 0.0f, 600.0f, 100.0f, 20.0f),
               WithinAbs(20.0f, 1e-3f));
    // Behind the muzzle and past the end are both misses, or a "beam" would be
    // a full-length line through the drone in both directions.
    CHECK(actives::ray_distance(0, 0, 0.0f, 600.0f, -100.0f, 0.0f) > 1e6f);
    CHECK(actives::ray_distance(0, 0, 0.0f, 600.0f, 700.0f, 0.0f) > 1e6f);
}

TEST_CASE("every shipped active maps onto a known id", "[Game][actives][config]") {
    // Gameplay pack (D221): the catalogue is trimmed to Heat-Seeking Missiles
    // (more boss items are a logged TODO). The laser/repulsor PLUMBING stays —
    // their ids and effects keep their own unit tests above — so this test now
    // pins "everything authored maps", not a fixed count of three.
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    REQUIRE(cfg.actives.size() >= 1);
    bool missiles_seen = false;
    for (const ActiveItemDef& d : cfg.actives) {
        const int id = actives::active_id_for(d.effect);
        INFO("effect: " << d.effect);
        REQUIRE(id >= 0);
        if (id == actives::ids::MISSILES) missiles_seen = true;
        CHECK_THAT(d.cooldown, WithinAbs(30.0f, 1e-3f));   // the note fixes this
        CHECK(actives::active_def(cfg.actives, id) != nullptr);
    }
    CHECK(missiles_seen);
}
