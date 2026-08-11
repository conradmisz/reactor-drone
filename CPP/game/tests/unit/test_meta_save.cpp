/**
 * Unit tests for the persistent meta-save and ship unlocks (Lane F, D80-D83).
 *
 * The meta-save is the only file the game reads that a player can corrupt, so
 * the load path is tested against garbage as hard as against a good file. The
 * unlock rule is tested at the exact boundary, because "4000 points" is a
 * promise made to the player in the menu.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/meta_save.hpp"

using Catch::Matchers::WithinAbs;

namespace {

std::string tmp_file(const char* name) {
    std::filesystem::path p = std::filesystem::temp_directory_path() / "reactor_drone_meta_tests";
    std::filesystem::create_directories(p);
    return (p / name).string();
}

void write_text(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::trunc);
    out << text;
}

/// The two shipped ships, without needing GameData.json.
std::vector<ShipDef> two_ships() {
    ShipDef standard;
    standard.name = "Standard";
    standard.weapon.fire_rate = 4.0f;
    standard.weapon.damage = 20.0f;
    ShipDef purple;
    purple.name = "Purple Gatling";
    purple.unlock_score = 4000;
    purple.weapon.fire_rate = 12.0f;
    purple.weapon.damage = 6.0f;
    purple.sidecar = "images/v2/player_drone.json";
    purple.idle_clip = "march";
    return {standard, purple};
}

}  // namespace

// (Task 2b review) A "meta_save_path() starts with user_data_dir()" check was
// here and was removed: on Linux user_data_dir() == class_root(), so the
// assertion passes whether meta_save_path() is built from either one — it
// cannot fail on a regression that reintroduces class_root() at the call
// site. The call site itself is a one-line, self-evidently-correct read
// (`project_paths::user_data_dir() + "/saves/meta.json"` in meta_save.cpp);
// what's actually worth testing — the Windows-shaped path-trimming logic — is
// covered by strip_trailing_seps' own tests in test_project_paths.cpp. The
// platform-divergent behavior itself (Windows saves landing in the prefpath,
// not under class_root()) is covered by the Wine portability run in the Task
// 2b report, not by CI.

TEST_CASE("meta save round-trips the lifetime score", "[meta_save]") {
    const std::string path = tmp_file("roundtrip.json");
    std::filesystem::remove(path);

    MetaSave m;
    m.lifetime_score = 12345;
    REQUIRE(meta_write(path, m));

    REQUIRE(meta_load(path).lifetime_score == 12345);
}

TEST_CASE("meta write creates a missing directory", "[meta_save]") {
    std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "reactor_drone_meta_tests" / "nested_saves";
    std::filesystem::remove_all(dir);
    const std::string path = (dir / "meta.json").string();

    MetaSave m;
    m.lifetime_score = 7;
    REQUIRE(meta_write(path, m));
    REQUIRE(meta_load(path).lifetime_score == 7);
}

TEST_CASE("a missing meta save loads as zero, not a failure", "[meta_save]") {
    const std::string path = tmp_file("does_not_exist.json");
    std::filesystem::remove(path);

    REQUIRE(meta_load(path).lifetime_score == 0);
}

TEST_CASE("a corrupt meta save falls back to defaults", "[meta_save]") {
    const std::string path = tmp_file("corrupt.json");

    SECTION("not JSON at all") {
        write_text(path, "{{{ this is not json");
        REQUIRE(meta_load(path).lifetime_score == 0);
    }
    SECTION("empty file") {
        write_text(path, "");
        REQUIRE(meta_load(path).lifetime_score == 0);
    }
    SECTION("valid JSON of the wrong shape") {
        write_text(path, "[1, 2, 3]");
        REQUIRE(meta_load(path).lifetime_score == 0);
    }
    SECTION("right key, wrong type") {
        write_text(path, "{\"lifetime_score\": \"lots\"}");
        REQUIRE(meta_load(path).lifetime_score == 0);
    }
    SECTION("a negative total is clamped") {
        write_text(path, "{\"lifetime_score\": -500}");
        REQUIRE(meta_load(path).lifetime_score == 0);
    }
}

TEST_CASE("the purple ship unlocks at exactly 4000 lifetime points", "[meta_save][ships]") {
    const std::vector<ShipDef> ships = two_ships();

    REQUIRE(ship_unlocked(ships[0], 0));          // Standard is always available
    REQUIRE_FALSE(ship_unlocked(ships[1], 3999));
    REQUIRE(ship_unlocked(ships[1], 4000));
    REQUIRE(ship_unlocked(ships[1], 4001));

    REQUIRE(unlocked_ship_count(ships, 3999) == 1);
    REQUIRE(unlocked_ship_count(ships, 4000) == 2);
}

TEST_CASE("a locked ship is never selectable", "[meta_save][ships]") {
    const std::vector<ShipDef> ships = two_ships();

    // Below the threshold the cycle button cannot land on the purple hull.
    REQUIRE(next_unlocked_ship(ships, 0, 3999) == 0);
    // At the threshold it cycles, and wraps back.
    REQUIRE(next_unlocked_ship(ships, 0, 4000) == 1);
    REQUIRE(next_unlocked_ship(ships, 1, 4000) == 0);
    // An empty ship list degrades to "ship 0" rather than indexing out of range.
    REQUIRE(next_unlocked_ship({}, 0, 999999) == 0);
}

TEST_CASE("a ship overlays the player config at run start", "[ships]") {
    const std::vector<ShipDef> ships = two_ships();
    PlayerConfig base;
    base.sidecar = "images/v2/player_drone.json";
    base.idle_clip = "march";
    base.move_speed = 260.0f;
    base.weapon.fire_rate = 4.0f;
    base.weapon.damage = 20.0f;

    PlayerConfig p = base;
    apply_ship(p, ships[1]);
    CHECK_THAT(p.weapon.fire_rate, WithinAbs(12.0, 1e-6));
    CHECK_THAT(p.weapon.damage, WithinAbs(6.0, 1e-6));
    // A ship only describes what you fly, never how fast you fly it.
    CHECK_THAT(p.move_speed, WithinAbs(260.0, 1e-6));
    CHECK(p.sidecar == "images/v2/player_drone.json");

    // Empty art fields keep the base ship's, so a weapon-only ship needs no art.
    ShipDef weapon_only;
    weapon_only.sidecar.clear();
    weapon_only.idle_clip.clear();
    weapon_only.weapon.damage = 99.0f;
    PlayerConfig q = base;
    apply_ship(q, weapon_only);
    CHECK(q.sidecar == base.sidecar);
    CHECK(q.idle_clip == base.idle_clip);
    CHECK_THAT(q.weapon.damage, WithinAbs(99.0, 1e-6));
}

TEST_CASE("GameData ships parse: Standard free, purple gated at 4000", "[ships][data]") {
    const GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    REQUIRE(cfg.ships.size() >= 2);
    CHECK(cfg.ships[0].unlock_score == 0);
    CHECK(cfg.ships[1].unlock_score == 4000);
    // The gatling premise: a much faster cadence for less damage per shot.
    CHECK(cfg.ships[1].weapon.fire_rate > cfg.ships[0].weapon.fire_rate * 2.0f);
    CHECK(cfg.ships[1].weapon.damage < cfg.ships[0].weapon.damage * 0.5f);
    // Ship 0 must not silently retune the authored player weapon.
    CHECK_THAT(cfg.ships[0].weapon.fire_rate, WithinAbs(cfg.player.weapon.fire_rate, 1e-6));
    CHECK_THAT(cfg.ships[0].weapon.damage, WithinAbs(cfg.player.weapon.damage, 1e-6));
}
