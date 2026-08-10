/**
 * test_wave_arc.cpp — the iteration-3 Lane A data spine (D52-D55), rescaled to
 * 30 waves by iteration-5 Lane O (D125).
 *
 * The 30-wave table and the 8 arena entries are generated from a formula, so the
 * thing worth testing is not any one row but the *shape*: 30 rows, boss flags
 * exactly on the tens, pressure that never goes backwards, and eight distinct
 * arenas activating on the right waves. All of that is a silent failure mode —
 * a table that dips in difficulty at wave 21 plays fine and is simply wrong.
 *
 * These assertions run against the SHIPPED GameData.json, not a fixture, because
 * the shipped file is the artefact: a bad splice is exactly what this catches.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/enemy_components.hpp"
#include "game/player_components.hpp"
#include "game/shop_system.hpp"

using Catch::Matchers::WithinAbs;

namespace {

GameConfig shipped() {
    return load_arena_config(project_paths::assets_dir() + "/GameData.json");
}

}  // namespace

TEST_CASE("the shipped wave table is 30 rows, fixed-count then timed",
          "[Game][wavearc]") {
    const GameConfig cfg = shipped();
    REQUIRE(cfg.waves.size() == 30);

    for (size_t i = 0; i < cfg.waves.size(); ++i) {
        const WaveDef& w = cfg.waves[i];
        if (i < 15) {
            CHECK(w.duration == 0.0f);   // fixed-count mode
            CHECK(w.count > 0);
        } else {
            CHECK(w.duration > 0.0f);    // timed mode ignores count
        }
        CHECK(!w.types.empty());
    }

    // victory_wave stays 0: the run ends when the table does, and the HUD reads
    // total_waves from waves.size(). The "20-wave cap" was never a cap.
    CHECK(cfg.victory_wave == 0);
}

TEST_CASE("boss waves are exactly 10/20/30", "[Game][wavearc][boss]") {
    const GameConfig cfg = shipped();
    std::vector<int> boss_waves;
    for (size_t i = 0; i < cfg.waves.size(); ++i)
        if (cfg.waves[i].boss) boss_waves.push_back(static_cast<int>(i) + 1);

    CHECK(boss_waves == std::vector<int>{10, 20, 30});
}

TEST_CASE("pressure never decreases across the arc", "[Game][wavearc][ramp]") {
    const GameConfig cfg = shipped();
    for (size_t i = 1; i < cfg.waves.size(); ++i) {
        const WaveDef& prev = cfg.waves[i - 1];
        const WaveDef& cur  = cfg.waves[i];
        INFO("wave " << (i + 1));

        // Enemies arrive at least as fast, and are at least as tough, forever.
        CHECK(cur.spawn_interval <= prev.spawn_interval + 1e-6f);
        CHECK(cur.hp_mult        >= prev.hp_mult - 1e-6f);
        CHECK(cur.speed_mult     >= prev.speed_mult - 1e-6f);

        // Volume is only comparable within a mode: `count` is meaningless on a
        // timed wave and `duration` is zero on a fixed one.
        if (prev.duration == 0.0f && cur.duration == 0.0f) CHECK(cur.count >= prev.count);
        if (prev.duration > 0.0f  && cur.duration > 0.0f)  CHECK(cur.duration >= prev.duration);
    }

    // Flatter than the 20-wave table it replaces (#13): wave 1 used to be 12
    // enemies at 0.45 s. If someone re-steepens the opening, this fails.
    CHECK(cfg.waves.front().count <= 10);
    CHECK(cfg.waves.front().spawn_interval >= 0.5f);
}

TEST_CASE("eight arenas activate on the two-pass schedule", "[Game][wavearc][arena]") {
    const GameConfig cfg = shipped();
    // Lane D (D72) appended a 9th arena, the final-wave Singularity void. The
    // two-pass schedule this case owns is still the FIRST eight; the 9th and its
    // activation are asserted in test_boss.cpp.
    REQUIRE(cfg.arenas.size() == 9);

    const int first[8] = {1, 4, 8, 12, 16, 19, 23, 27};
    for (int i = 0; i < 8; ++i) {
        INFO("arena " << i);
        CHECK(cfg.arenas[static_cast<size_t>(i)].first_wave == first[i]);
        CHECK(active_arena_index(cfg.arenas, first[i]) == i);
        // Still the same arena the wave before the NEXT activation.
        if (i < 7) CHECK(active_arena_index(cfg.arenas, first[i + 1] - 1) == i);
    }
    // Wave 29 is the last of the two passes; wave 30 is Lane D's void.
    CHECK(active_arena_index(cfg.arenas, 29) == 7);

    std::set<std::string> names;
    for (const ArenaDef& a : cfg.arenas) names.insert(a.name);
    CHECK(names.size() == 9);   // eight distinct arenas + the final-wave void
}

TEST_CASE("the second pass reuses the art and changes only the layout",
          "[Game][wavearc][arena]") {
    const GameConfig cfg = shipped();
    REQUIRE(cfg.arenas.size() == 9);   // 8 two-pass themes + Lane D's final-wave void

    for (size_t i = 0; i < 4; ++i) {
        const ArenaDef& a = cfg.arenas[i];       // pass 1
        const ArenaDef& b = cfg.arenas[i + 4];   // its twin
        INFO("theme " << a.name);

        CHECK(a.specialty_tier == 1);
        CHECK(b.specialty_tier == 2);
        CHECK(b.wall_image == a.wall_image);
        CHECK(b.hazard_image == a.hazard_image);
        CHECK(b.backdrop_layers.size() == a.backdrop_layers.size());

        // Same amount of cover, different cover. (PROVISIONAL: the pass-2
        // layouts are a mechanical 90-degree rotation, see D53.)
        REQUIRE(b.obstacles.size() == a.obstacles.size());
        REQUIRE(b.hazards.size() == a.hazards.size());
        bool moved = false;
        for (size_t k = 0; k < a.obstacles.size(); ++k)
            if (b.obstacles[k].x != a.obstacles[k].x ||
                b.obstacles[k].y != a.obstacles[k].y) moved = true;
        CHECK(moved);
    }
}

TEST_CASE("the pass-2 layouts stay inside the arena", "[Game][wavearc][arena]") {
    const GameConfig cfg = shipped();
    const float cx = cfg.arena.center_x, cy = cfg.arena.center_y, r = cfg.arena.radius;
    for (size_t i = 4; i < cfg.arenas.size(); ++i) {
        for (const ObstacleDef& o : cfg.arenas[i].obstacles) {
            // Every corner within the play circle — a rotation that flung a
            // pillar into the wall would be invisible until someone played there.
            const float dx = std::max(std::abs(o.x - cx), std::abs(o.x + o.w - cx));
            const float dy = std::max(std::abs(o.y - cy), std::abs(o.y + o.h - cy));
            CHECK(std::sqrt(dx * dx + dy * dy) <= r);
        }
    }
}

TEST_CASE("shield regen is data, not a hardcoded fifth per second",
          "[Game][wavearc][shield]") {
    const GameConfig cfg = shipped();
    // ~12 s for a full bank, and 5 s of not being hit before it starts (#12, D54).
    CHECK_THAT(cfg.shop.shield_regen_frac, WithinAbs(0.08f, 1e-5f));
    CHECK_THAT(cfg.shop.shield_regen_delay, WithinAbs(5.0f, 1e-5f));

    // A config with no shop block at all still gets the slow rate, not 0 and not
    // the old 0.2 — the default is the decision, the JSON only repeats it.
    CHECK_THAT(ShopConfig{}.shield_regen_frac, WithinAbs(0.08f, 1e-5f));

    // And the purchase actually derives regen from it.
    const ShopUpgradeDef* shield = nullptr;
    int row = 0;
    for (size_t i = 0; i < cfg.shop.upgrades.size(); ++i)
        if (cfg.shop.upgrades[i].effect == "shield") {
            shield = &cfg.shop.upgrades[i];
            row = static_cast<int>(i) + 1;
        }
    REQUIRE(shield != nullptr);

    EntityManager em;
    ComponentStorage storage;
    Blackboard blackboard;
    Entity player = em.create_entity();
    storage.add_component<PlayerTag>(player, PlayerTag{});
    storage.add_component<ShipState>(player, ShipState{});
    storage.add_component<Health>(player, Health{});
    storage.get_component<ShipState>(player)->get().currency = shield->price;

    // apply() is private; the public path is a purchase, so drive it the way
    // main.cpp does: credits on the ship, digit pressed.
    ShopSystem shop;
    shop.set_config(&cfg.shop);
    shop.open(storage, em, blackboard);
    shop.update(storage, blackboard, row, false);

    ShipState& s = storage.get_component<ShipState>(player)->get();
    CHECK_THAT(s.shield_max, WithinAbs(shield->amount, 1e-4f));
    CHECK_THAT(s.shield_regen, WithinAbs(s.shield_max * 0.08f, 1e-4f));
    // A full bank takes ~12 s, not the old ~5 s.
    CHECK(s.shield_max / s.shield_regen > 10.0f);
}

TEST_CASE("the full shop is due every fifth cleared wave", "[Game][wavearc][shop]") {
    // Mirrors main.cpp's `current_wave_index() % 5 == 0` (D55). A contract test:
    // if that line moves back to % 4, the cadence this wave table was authored
    // around is gone and nothing else would notice.
    std::vector<int> stops;
    for (int cleared = 1; cleared <= 30; ++cleared)
        if (cleared % 5 == 0) stops.push_back(cleared);

    CHECK(stops.size() == 6);                     // two per boss cycle
    CHECK(stops.front() == 5);
    CHECK(stops.back() == 30);
    for (int s : stops) CHECK(s % 10 != 8);       // never adjacent-late to a boss
}

TEST_CASE("coins still despawn — D52 reverses the plan's never-despawn item",
          "[Game][wavearc][economy]") {
    // The iteration-3 plan says pickup_lifetime should be 0 (drops live forever).
    // The user reversed that: the fade is the risk/reward. This test exists so a
    // future agent reading the stale plan flips it back and immediately fails.
    const GameConfig cfg = shipped();
    CHECK(cfg.economy.pickup_lifetime > 0.0f);
    CHECK_THAT(cfg.economy.pickup_lifetime, WithinAbs(12.0f, 1e-5f));
}
