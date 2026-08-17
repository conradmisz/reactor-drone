/**
 * Unit tests for secondary fire (gameplay pack v2.3 tier 3, D221).
 * The charge rules are pure; the data test pins each weapon's authored
 * secondary id so a catalogue typo cannot ship a dead right-click.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/secondary_fire.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("charge fraction ramps to full at CHARGE_MAX_S and clamps", "[secondary]") {
    CHECK_THAT(secondary::charge_frac(0.0f), WithinAbs(0.0, 1e-6));
    CHECK_THAT(secondary::charge_frac(secondary::CHARGE_MAX_S * 0.5f), WithinAbs(0.5, 1e-6));
    CHECK_THAT(secondary::charge_frac(secondary::CHARGE_MAX_S), WithinAbs(1.0, 1e-6));
    CHECK_THAT(secondary::charge_frac(99.0f), WithinAbs(1.0, 1e-6));
    CHECK_THAT(secondary::charge_frac(-1.0f), WithinAbs(0.0, 1e-6));
}

// D231 item 8: the scaled cooldown is GONE — the charge bank (passive refill,
// CHARGE_REFILL_S empty->full) is the gate now, so a tap costs exactly the
// charge it spent and nothing else.

TEST_CASE("charge damage runs 1x tap to 4x full", "[secondary]") {
    CHECK_THAT(secondary::charge_damage_mult(0.0f), WithinAbs(1.0, 1e-6));
    CHECK_THAT(secondary::charge_damage_mult(1.0f), WithinAbs(4.0, 1e-6));
}

TEST_CASE("every authored weapon has its spec'd secondary", "[secondary][data]") {
    const GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    struct Want { const char* weapon; const char* secondary; };
    for (const Want w : {Want{"55 Iron", "charge_shot"}, Want{"Moonshot", "crescent_burst"},
                         Want{"Flak Cannon", "lava_stream"}, Want{"Hailstorm", "blizzard"}}) {
        const int i = find_weapon(cfg.weapons, w.weapon);
        REQUIRE(i >= 0);
        CHECK(cfg.weapons[i].secondary == w.secondary);
        CHECK(cfg.weapons[i].secondary_cd > 0.0f);
    }
}
