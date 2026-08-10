/**
 * test_prestige.cpp — the opt-in prestige restart (Iteration 5, Lane O, D126-D129).
 *
 * Two silent failure modes are worth pinning. First, the buff is applied at ONE
 * site from a pristine config (D50's rule): if it ever compounds, a player's
 * second run of a session is quietly stronger than their first and nothing else
 * would notice. Second, the level comes off a file a player can hand-edit, so a
 * `"prestige": 999` must load as the cap rather than as a god ship.
 *
 * The wave-table shape lives in test_wave_arc.cpp; this file only covers what
 * prestige adds.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "game/meta_save.hpp"
#include "game/prestige.hpp"

using Catch::Matchers::WithinAbs;

namespace {

std::string tmp_meta(const std::string& contents) {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "reactor_drone_prestige_tests";
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "meta.json").string();
    std::ofstream out(path, std::ios::trunc);
    out << contents;
    return path;
}

}  // namespace

TEST_CASE("prestige bonuses are linear in the level and capped", "[Game][prestige]") {
    CHECK_THAT(prestige_bonus(0).health_mult, WithinAbs(1.0f, 1e-6f));
    CHECK_THAT(prestige_bonus(0).damage_mult, WithinAbs(1.0f, 1e-6f));

    CHECK_THAT(prestige_bonus(2).health_mult, WithinAbs(1.20f, 1e-6f));
    CHECK_THAT(prestige_bonus(2).speed_mult,  WithinAbs(1.10f, 1e-6f));
    CHECK_THAT(prestige_bonus(2).damage_mult, WithinAbs(1.16f, 1e-6f));

    // Beyond the cap is the cap, not more — including absurd and negative input.
    CHECK_THAT(prestige_bonus(999).health_mult,
               WithinAbs(prestige_bonus(PRESTIGE_MAX_LEVEL).health_mult, 1e-6f));
    CHECK_THAT(prestige_bonus(-3).health_mult, WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("apply_prestige scales the player block once, from a pristine copy",
          "[Game][prestige]") {
    const PlayerConfig base;   // 100 hp / 260 speed / 20 damage

    PlayerConfig p = base;
    apply_prestige(p, 3);
    CHECK_THAT(p.start_health,  WithinAbs(base.start_health * 1.30f, 1e-3f));
    CHECK_THAT(p.move_speed,    WithinAbs(base.move_speed * 1.15f, 1e-3f));
    CHECK_THAT(p.weapon.damage, WithinAbs(base.weapon.damage * 1.24f, 1e-3f));

    // NOT idempotent, exactly like apply_ship and apply_difficulty — which is why
    // main.cpp re-copies base_config every run. Applying twice compounds, and this
    // case exists so that stays a documented property rather than a surprise.
    apply_prestige(p, 3);
    CHECK(p.start_health > base.start_health * 1.30f);

    // Level 0 is a no-op, so a player who has never prestiged flies stock.
    PlayerConfig q = base;
    apply_prestige(q, 0);
    CHECK_THAT(q.start_health,  WithinAbs(base.start_health, 1e-4f));
    CHECK_THAT(q.weapon.damage, WithinAbs(base.weapon.damage, 1e-4f));
}

TEST_CASE("the meta save round-trips the prestige level beside the score",
          "[Game][prestige][meta_save]") {
    const std::string path = tmp_meta("");
    MetaSave m;
    m.lifetime_score = 4321;
    m.prestige = 2;
    REQUIRE(meta_write(path, m));

    const MetaSave back = meta_load(path);
    CHECK(back.lifetime_score == 4321);
    CHECK(back.prestige == 2);
}

TEST_CASE("a hand-edited prestige level cannot exceed the cap",
          "[Game][prestige][meta_save]") {
    CHECK(meta_load(tmp_meta(R"({"lifetime_score": 0, "prestige": 999})")).prestige
          == PRESTIGE_MAX_LEVEL);
    CHECK(meta_load(tmp_meta(R"({"lifetime_score": 0, "prestige": -4})")).prestige == 0);
    // A file from before prestige existed still loads, at level 0.
    CHECK(meta_load(tmp_meta(R"({"lifetime_score": 900})")).prestige == 0);
    // And a corrupt file is still a default, not an exception.
    CHECK(meta_load(tmp_meta("{not json")).prestige == 0);
}

TEST_CASE("the prestige summary is one printable line at every level",
          "[Game][prestige][ui]") {
    CHECK(prestige_summary(0).find("PRESTIGE 0") == 0);
    const std::string s = prestige_summary(2);
    CHECK(s.find("+20% HULL") != std::string::npos);
    CHECK(s.find("+16% DAMAGE") != std::string::npos);
    CHECK(s.find('\n') == std::string::npos);   // one label, one line
    CHECK(prestige_summary(99) == prestige_summary(PRESTIGE_MAX_LEVEL));
}
