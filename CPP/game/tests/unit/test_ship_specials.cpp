/**
 * Unit tests for the ship special attributes (gameplay pack v2.3, D221).
 * The veil's trigger/re-arm rules are pure; the data test pins the specials
 * and projectile identities actually authored in GameData.json.
 */
#include <catch2/catch_test_macros.hpp>

#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/ship_specials.hpp"

TEST_CASE("phoenix veil fires below 10% only while armed", "[specials][veil]") {
    CHECK(veil::should_fire(0.09f, true));
    CHECK_FALSE(veil::should_fire(0.09f, false));    // spent — no re-trigger at 9%
    CHECK_FALSE(veil::should_fire(0.10f, true));     // exactly 10% is not "below"
    CHECK_FALSE(veil::should_fire(0.50f, true));
}

TEST_CASE("phoenix veil re-arms at 25%, not the moment it ends", "[specials][veil]") {
    CHECK_FALSE(veil::should_rearm(0.12f, false));   // hovering low stays spent
    CHECK(veil::should_rearm(0.25f, false));
    CHECK(veil::should_rearm(0.90f, false));
    CHECK_FALSE(veil::should_rearm(0.90f, true));    // already armed — no-op
}

TEST_CASE("GameData authors the pack specials and projectile identities", "[specials][data]") {
    const GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    REQUIRE(cfg.ships.size() >= 3);
    CHECK(cfg.ships[0].special == "equip_cd");       // Falcon
    CHECK(cfg.ships[1].special == "phoenix_veil");   // Owl
    CHECK(cfg.ships[2].special == "ram_dash");       // Gryphon
    CHECK(cfg.ships[2].shield > 0.0f);               // ram dash needs a shield to refill

    const int moon = find_weapon(cfg.weapons, "Moonshot");
    const int iron = find_weapon(cfg.weapons, "55 Iron");
    REQUIRE(moon >= 0); REQUIRE(iron >= 0);
    CHECK(cfg.weapons[moon].stats.pierce);                       // the wide crescent
    CHECK(cfg.weapons[moon].stats.projectile_size >
          cfg.weapons[iron].stats.projectile_size * 2.0f);
    CHECK_FALSE(cfg.weapons[iron].stats.pierce);
}
