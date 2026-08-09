/**
 * Unit tests for Gameplay Phase B difficulty scaling (apply_difficulty, D50).
 *
 * The scaling is what makes Hard mode exist at all, and it is applied to a copy
 * of the loaded config on every run start — so the two things worth pinning are
 * that it scales what it claims to, and that it never touches the player.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/gamedata_loader.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"

using Catch::Matchers::WithinAbs;

namespace {

/// Three fixed-count waves + one timed one, with rosters that widen by wave —
/// the same shape as the shipped table, small enough to assert on.
GameConfig make_config() {
    GameConfig cfg;

    EnemyType spark; spark.name = "spark"; spark.currency = 1; spark.health = 20.0f;
    EnemyType hulk;  hulk.name  = "hulk";  hulk.currency = 4;  hulk.health = 90.0f;
    cfg.enemy_types = {spark, hulk};

    WaveDef w1; w1.count = 10; w1.spawn_interval = 0.5f; w1.types = {0};
    WaveDef w2; w2.count = 20; w2.spawn_interval = 0.4f; w2.types = {0, 1};
    WaveDef w3; w3.count = 30; w3.spawn_interval = 0.3f; w3.types = {1}; w3.hp_mult = 1.2f;
    WaveDef w4; w4.duration = 20.0f; w4.spawn_interval = 0.5f; w4.types = {0, 1};
    w4.hp_mult = 1.5f; w4.speed_mult = 1.1f;
    cfg.waves = {w1, w2, w3, w4};

    ArenaDef arena; arena.name = "Core";
    HazardDef hz; hz.damage = 10.0f;
    arena.hazards = {hz};
    cfg.arenas = {arena};

    return cfg;
}

DifficultyDef hard() {
    DifficultyDef d;
    d.name = "Hard";
    d.count_mult = 1.5f;
    d.spawn_interval_mult = 0.7f;
    d.hp_mult = 1.3f;
    d.speed_mult = 1.15f;
    d.currency_mult = 1.4f;
    d.hazard_damage_mult = 1.5f;
    d.type_lookahead = 2;
    return d;
}

}  // namespace

TEST_CASE("apply_difficulty scales counts, spacing and the per-wave multipliers",
          "[Game][difficulty]") {
    GameConfig cfg = make_config();
    apply_difficulty(cfg, hard());

    CHECK(cfg.waves[0].count == 15);   // 10 * 1.5
    CHECK(cfg.waves[1].count == 30);
    CHECK(cfg.waves[2].count == 45);
    CHECK_THAT(cfg.waves[0].spawn_interval, WithinAbs(0.35f, 1e-5f));
    CHECK_THAT(cfg.waves[2].hp_mult, WithinAbs(1.2f * 1.3f, 1e-5f));
    CHECK_THAT(cfg.waves[0].hp_mult, WithinAbs(1.3f, 1e-5f));
    CHECK_THAT(cfg.waves[3].speed_mult, WithinAbs(1.1f * 1.15f, 1e-5f));

    // A timed wave keeps its duration: extra pressure is spacing, not run length.
    CHECK_THAT(cfg.waves[3].duration, WithinAbs(20.0f, 1e-5f));
}

TEST_CASE("apply_difficulty pulls enemy-type unlocks forward by type_lookahead",
          "[Game][difficulty][types]") {
    GameConfig cfg = make_config();
    apply_difficulty(cfg, hard());

    const std::vector<int> both = {0, 1};
    // Wave 1 merges waves 1-3: {0} u {0,1} u {1} = {0,1} — hulks from wave one.
    CHECK(cfg.waves[0].types == both);
    CHECK(cfg.waves[2].types == both);  // merges w3 {1} + w4 {0,1}
    // Merging reads the ORIGINAL rosters, so the last wave keeps its own.
    CHECK(cfg.waves[3].types == both);
}

TEST_CASE("an empty roster means every type and is never narrowed",
          "[Game][difficulty][types]") {
    GameConfig cfg = make_config();
    cfg.waves[0].types.clear();   // "all types"
    cfg.waves[2].types.clear();
    apply_difficulty(cfg, hard());

    CHECK(cfg.waves[0].types.empty());
    CHECK(cfg.waves[1].types.empty());  // absorbs wave 3's "all"
}

TEST_CASE("difficulty scales enemy payout and hazards, never the player",
          "[Game][difficulty][economy]") {
    GameConfig cfg = make_config();
    const PlayerConfig player_before = cfg.player;
    const ShopConfig shop_before = cfg.shop;
    apply_difficulty(cfg, hard());

    CHECK(cfg.enemy_types[0].currency == 1);  // 1 * 1.4 = 1.4 -> 1
    CHECK(cfg.enemy_types[1].currency == 6);  // 4 * 1.4 = 5.6 -> 6
    CHECK_THAT(cfg.arenas[0].hazards[0].damage, WithinAbs(15.0f, 1e-5f));

    // The user's call: Hard is enemy-side only, never a harsher player economy.
    CHECK_THAT(cfg.player.start_health, WithinAbs(player_before.start_health, 1e-5f));
    CHECK_THAT(cfg.player.move_speed, WithinAbs(player_before.move_speed, 1e-5f));
    CHECK_THAT(cfg.player.weapon.damage, WithinAbs(player_before.weapon.damage, 1e-5f));
    CHECK_THAT(cfg.shop.price_growth, WithinAbs(shop_before.price_growth, 1e-5f));
    CHECK_THAT(cfg.economy.pickup_lifetime, WithinAbs(12.0f, 1e-5f));  // the default, untouched
}

TEST_CASE("the Normal difficulty is the identity", "[Game][difficulty]") {
    GameConfig cfg = make_config();
    const GameConfig before = make_config();
    apply_difficulty(cfg, DifficultyDef{});   // all multipliers default to 1.0

    for (size_t i = 0; i < cfg.waves.size(); ++i) {
        CHECK(cfg.waves[i].count == before.waves[i].count);
        CHECK(cfg.waves[i].types == before.waves[i].types);
        CHECK_THAT(cfg.waves[i].spawn_interval,
                   WithinAbs(before.waves[i].spawn_interval, 1e-5f));
        CHECK_THAT(cfg.waves[i].hp_mult, WithinAbs(before.waves[i].hp_mult, 1e-5f));
    }
    CHECK(cfg.enemy_types[1].currency == before.enemy_types[1].currency);
    CHECK_THAT(cfg.arenas[0].hazards[0].damage,
               WithinAbs(before.arenas[0].hazards[0].damage, 1e-5f));
}

TEST_CASE("the shipped GameData difficulties are Normal-first and enemy-side",
          "[Game][difficulty][data]") {
    // Guards the one assumption main.cpp makes about the data: index 0 is the
    // default run, index 1 is the harder one the menu's HARD button selects.
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    REQUIRE(cfg.difficulties.size() >= 2);
    CHECK(cfg.difficulties[0].name == "Normal");
    CHECK(cfg.difficulties[1].count_mult > 1.0f);
    CHECK(cfg.difficulties[1].spawn_interval_mult < 1.0f);
    CHECK(cfg.difficulties[1].type_lookahead > 0);

    for (const DifficultyDef& d : cfg.difficulties) {
        CHECK(d.spawn_interval_mult > 0.0f);   // a 0 would spawn every frame
        CHECK(d.count_mult > 0.0f);
    }

    // Applying Hard to the shipped table must stay inside the spawner's assumptions.
    apply_difficulty(cfg, cfg.difficulties[1]);
    for (const WaveDef& w : cfg.waves) {
        CHECK(w.count >= 1);
        CHECK(w.spawn_interval >= 0.05f);
        for (int t : w.types) CHECK(t < static_cast<int>(cfg.enemy_types.size()));
    }
}

TEST_CASE("the shipped main_menu screen carries the callbacks main.cpp compares",
          "[Game][difficulty][menu]") {
    // Same invisible seam as test_intermission_screen.cpp: these two names live
    // as string literals in main.cpp, so a rename in the JSON would leave a
    // title screen whose buttons silently do nothing.
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    load_game_data(project_paths::assets_dir() + "/GameData.json", em, cs, bb);

    bool normal = false, hard_btn = false, screen_found = false, boots_inactive = false;
    for (Entity e : cs.entities_with_component<UIElement>()) {
        auto m = cs.get_component<ScreenMembership>(e);
        if (!m.has_value() || m->get().screen_name != "main_menu") continue;
        auto el = cs.get_component<UIElement>(e);
        if (el->get().on_click_fn == "on_start_normal_click") normal = true;
        if (el->get().on_click_fn == "on_start_hard_click") hard_btn = true;
    }
    for (Entity e : cs.entities_with_component<UIScreen>()) {
        auto s = cs.get_component<UIScreen>(e);
        if (!s.has_value() || s->get().screen_name != "main_menu") continue;
        screen_found = true;
        boots_inactive = !s->get().active;   // pushed by main.cpp, not by the loader
    }
    CHECK(normal);
    CHECK(hard_btn);
    CHECK(screen_found);
    CHECK(boots_inactive);
}
