/**
 * Unit tests for the ship special attributes (gameplay pack v2.3, D221;
 * veil replaced by dash_charge in D229). The data test pins the specials and
 * projectile identities actually authored in GameData.json.
 */
#include <catch2/catch_test_macros.hpp>

#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/ship_specials.hpp"

TEST_CASE("GameData authors the pack specials and projectile identities", "[specials][data]") {
    const GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    REQUIRE(cfg.ships.size() >= 3);
    CHECK(cfg.ships[0].special == "equip_cd");       // Falcon
    CHECK(cfg.ships[1].special == "dash_charge");    // Owl (D229)
    CHECK(cfg.ships[2].special == "ram_dash");       // Gryphon
    CHECK(cfg.ships[2].shield > 0.0f);               // ram dash needs a shield to refill

    const int moon = find_weapon(cfg.weapons, "Moonshot");
    const int iron = find_weapon(cfg.weapons, "55 Iron");
    REQUIRE(moon >= 0); REQUIRE(iron >= 0);
    CHECK(cfg.weapons[moon].stats.pierce);                       // the wide crescent
    CHECK(cfg.weapons[moon].stats.crescent);                     // ...that LOOKS like one (D229)
    CHECK(cfg.weapons[moon].stats.projectile_size >
          cfg.weapons[iron].stats.projectile_size * 2.0f);
    CHECK_FALSE(cfg.weapons[iron].stats.pierce);
}

TEST_CASE("active requirement gate matches loadouts (D232)", "[specials][actives]") {
    CHECK(active_requirement_met("", "55 Iron", "Falcon"));
    CHECK(active_requirement_met("weapon:55 Iron|Moonshot", "Moonshot", "Owl"));
    CHECK_FALSE(active_requirement_met("weapon:55 Iron|Moonshot", "Flak Cannon", "Gryphon"));
    CHECK(active_requirement_met("weapon:Flak Cannon", "Flak Cannon", "Falcon"));
    CHECK(active_requirement_met("ship:Gryphon", "Hailstorm", "Gryphon"));
    CHECK_FALSE(active_requirement_met("ship:Gryphon", "Flak Cannon", "Owl"));

    // The three shipped gated actives exist and are gated.
    const GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    int gated = 0;
    for (const auto& a : cfg.actives) if (!a.requires_loadout.empty()) ++gated;
    CHECK(gated >= 3);
}
