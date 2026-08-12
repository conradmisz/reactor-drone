#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include "game/telemetry.hpp"

TEST_CASE("heat_bin maps the arena circle's bounding square to 32x32", "[telemetry]") {
    // Arena: centre (1000, 1000), radius 800 -> square x,y in [200, 1800]
    REQUIRE(telemetry::heat_bin(200.0f, 200.0f, 1000.0f, 1000.0f, 800.0f) == 0);           // min corner
    REQUIRE(telemetry::heat_bin(1799.9f, 1799.9f, 1000.0f, 1000.0f, 800.0f) == 1023);      // max corner
    REQUIRE(telemetry::heat_bin(1000.0f, 1000.0f, 1000.0f, 1000.0f, 800.0f) == 16 * 32 + 16);
    REQUIRE(telemetry::heat_bin(-5000.0f, 9000.0f, 1000.0f, 1000.0f, 800.0f) == 31 * 32 + 0); // clamped
}

TEST_CASE("b64 encodes RFC 4648 vectors", "[telemetry]") {
    auto enc = [](const char* s) {
        return telemetry::b64(reinterpret_cast<const uint8_t*>(s), std::string(s).size());
    };
    REQUIRE(enc("Man") == "TWFu");
    REQUIRE(enc("Ma") == "TWE=");
    REQUIRE(enc("M") == "TQ==");
    REQUIRE(enc("") == "");
}

TEST_CASE("frame_sample opens waves, accumulates damage and econ, bins heat", "[telemetry]") {
    telemetry::RunReport r;
    // Wave 1 opens with the drone at full state.
    telemetry::frame_sample(r, 0.1, 1, 100.0f, 50.0f, 0, 1000.0f, 1000.0f, 0, 1000.0f, 1000.0f, 800.0f);
    REQUIRE(r.waves.size() == 1);
    REQUIRE(r.waves[0].wave == 1);
    REQUIRE(r.waves[0].hp == 100.0f);
    REQUIRE(r.waves[0].units == 0);
    // Take 30 hull damage, earn 10 units.
    telemetry::frame_sample(r, 0.1, 1, 70.0f, 50.0f, 10, 1000.0f, 1000.0f, 0, 1000.0f, 1000.0f, 800.0f);
    REQUIRE(r.waves[0].damage_taken == 30.0f);
    REQUIRE(r.earned == 10);
    REQUIRE(r.spent == 0);
    // Spend 6 (currency 10 -> 4): spent, not negative earned.
    telemetry::frame_sample(r, 0.1, 1, 70.0f, 50.0f, 4, 1000.0f, 1000.0f, 0, 1000.0f, 1000.0f, 800.0f);
    REQUIRE(r.spent == 6);
    // A hull INCREASE (shop hull upgrade) is not damage.
    telemetry::frame_sample(r, 0.1, 1, 90.0f, 50.0f, 4, 1000.0f, 1000.0f, 0, 1000.0f, 1000.0f, 800.0f);
    REQUIRE(r.waves[0].damage_taken == 30.0f);
    // Wave 2 opens carrying the CURRENT state; wave 1 kept its seconds.
    telemetry::frame_sample(r, 0.1, 2, 90.0f, 50.0f, 4, 1000.0f, 1000.0f, 0, 1000.0f, 1000.0f, 800.0f);
    REQUIRE(r.waves.size() == 2);
    REQUIRE(r.waves[0].seconds > 0.39);  // 4 frames x 0.1s landed on wave 1
    REQUIRE(r.waves[1].units == 4);
    // 0.5 s at one spot -> at least 2 samples in the centre bin of arena 0.
    for (int i = 0; i < 5; ++i)
        telemetry::frame_sample(r, 0.1, 2, 90.0f, 50.0f, 4, 1000.0f, 1000.0f, 0, 1000.0f, 1000.0f, 800.0f);
    REQUIRE(r.heat.count(0) == 1);
    REQUIRE(r.heat.at(0)[16 * 32 + 16] >= 2);
    REQUIRE(r.dur_s > 0.89);
}

TEST_CASE("serialize emits envelope, sections and base64 heat", "[telemetry]") {
    telemetry::RunReport r;
    r.player_id = "11111111-aaaa-bbbb-cccc-000000000001";
    r.session_id = "s1"; r.game_version = "2.0.0"; r.difficulty = "Normal";
    r.outcome = "death"; r.seed = 42; r.ship = 1; r.wave = 7; r.score = 1200;
    r.died = true; r.death_x = 3.0f; r.death_y = 4.0f; r.death_wave = 7; r.killed_by = "enemy:2";
    telemetry::frame_sample(r, 0.3, 7, 10.0f, 0.0f, 5, 1000.0f, 1000.0f, 2, 1000.0f, 1000.0f, 800.0f);
    r.upg_counts[3] = 2;
    r.consumables_used["REPAIR KIT"] = 1;
    r.ui["shop"] = 3;
    const auto j = nlohmann::json::parse(telemetry::serialize(r));
    REQUIRE(j["v"] == 1);
    REQUIRE(j["player_id"] == "11111111-aaaa-bbbb-cccc-000000000001");
    REQUIRE(j["outcome"] == "death");
    REQUIRE(j["death"]["x"] == 3.0f);
    REQUIRE(j["death"]["killed_by"] == "enemy:2");
    REQUIRE(j["death"].contains("bin"));
    REQUIRE(j["heat"]["2"].is_string());               // arena idx -> base64 grid
    REQUIRE(j["econ"]["upg_counts"][3] == 2);
    REQUIRE(j["econ"]["consumables_used"]["REPAIR KIT"] == 1);
    REQUIRE(j["ui"]["shop"] == 3);
    REQUIRE(j["waves"][0]["wave"] == 7);
    REQUIRE(j["combat"]["shots"] == 0);
    // No death section when the run didn't end in one.
    telemetry::RunReport alive;
    alive.player_id = r.player_id; alive.session_id = "s1";
    alive.game_version = "2.0.0"; alive.difficulty = "Normal";
    REQUIRE(nlohmann::json::parse(telemetry::serialize(alive)).contains("death") == false);
}
