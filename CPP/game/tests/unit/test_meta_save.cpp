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

/// A small roster without needing GameData.json (gameplay pack, D221):
/// a free starter, a purchasable ship and a locked (unreleased) one.
std::vector<ShipDef> roster() {
    ShipDef falcon;
    falcon.name = "Falcon";
    falcon.default_weapon = "55 Iron";
    ShipDef owl;
    owl.name = "Owl";
    owl.scrap_cost = 400;
    owl.default_weapon = "Moonshot";
    owl.sidecar = "images/v2/player_drone.json";
    owl.idle_clip = "march";
    ShipDef gatling;
    gatling.name = "Gatling";
    gatling.scrap_cost = 1200;
    gatling.locked = true;
    gatling.default_weapon = "Hailstorm";
    return {falcon, owl, gatling};
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

TEST_CASE("ship ownership: free always, bought when listed, locked never", "[meta_save][ships]") {
    const std::vector<ShipDef> ships = roster();
    MetaSave m;

    REQUIRE(ship_owned(m, ships[0]));            // Falcon: cost 0, always owned
    REQUIRE_FALSE(ship_owned(m, ships[1]));      // Owl: not bought yet
    m.owned_ships.push_back("Owl");
    REQUIRE(ship_owned(m, ships[1]));
    // A locked ship is owned by no one — even if the save claims it.
    m.owned_ships.push_back("Gatling");
    REQUIRE_FALSE(ship_owned(m, ships[2]));

    REQUIRE(owned_ship_count(roster(), MetaSave{}) == 1);
    REQUIRE(owned_ship_count(ships, m) == 2);
}

TEST_CASE("an unowned ship is never selectable", "[meta_save][ships]") {
    const std::vector<ShipDef> ships = roster();
    MetaSave m;

    // Nothing else owned: the cycle button is a no-op.
    REQUIRE(next_owned_ship(ships, 0, m) == 0);
    // Owl bought: it cycles, and wraps back — never landing on locked Gatling.
    m.owned_ships.push_back("Owl");
    REQUIRE(next_owned_ship(ships, 0, m) == 1);
    REQUIRE(next_owned_ship(ships, 1, m) == 0);
    // An empty ship list degrades to "ship 0" rather than indexing out of range.
    REQUIRE(next_owned_ship({}, 0, m) == 0);
}

TEST_CASE("weapons are owned through ships, never stored", "[meta_save][weapons]") {
    const std::vector<ShipDef> ships = roster();
    MetaSave m;
    REQUIRE(weapon_owned(m, ships, "55 Iron"));       // Falcon grants it
    REQUIRE_FALSE(weapon_owned(m, ships, "Moonshot"));
    m.owned_ships.push_back("Owl");
    REQUIRE(weapon_owned(m, ships, "Moonshot"));
    // Locked Gatling grants nothing, so Hailstorm stays out of reach.
    REQUIRE_FALSE(weapon_owned(m, ships, "Hailstorm"));
}

TEST_CASE("scrap_for_run pays waves, bosses, milestones and victory", "[meta_save][scrap]") {
    ScrapConfig c;  // 5 / 25 / 100, milestone 100 per 10 waves (D239)
    CHECK(scrap_for_run(0, false, c) == 0);
    CHECK(scrap_for_run(9, false, c) == 45);            // no boss, no milestone yet
    CHECK(scrap_for_run(10, false, c) == 175);          // boss 25 + milestone 100
    CHECK(scrap_for_run(19, false, c) == 220);          // still ONE whole milestone
    CHECK(scrap_for_run(29, false, c) == 395);          // died on the last boss
    CHECK(scrap_for_run(30, true, c) == 625);           // full victory, three milestones
    CHECK(scrap_for_run(-3, false, c) == 0);            // garbage clamps
    CHECK(scrap_for_run(99, true, c) == 625);           // over-count clamps to 30

    // D239: either knob at zero turns the milestone purse off entirely.
    ScrapConfig off = c;
    off.milestone_bonus = 0;
    CHECK(scrap_for_run(30, true, off) == 325);         // the pre-D239 number
    off = c;
    off.milestone_every = 0;
    CHECK(scrap_for_run(30, true, off) == 325);
}

TEST_CASE("a ship overlays stats, a weapon overlays the gun", "[ships]") {
    PlayerConfig base;
    base.sidecar = "images/v2/player_drone.json";
    base.idle_clip = "march";
    base.move_speed = 260.0f;
    base.start_health = 100.0f;
    base.weapon.fire_rate = 4.0f;

    ShipDef tank;                      // Gryphon-shaped
    tank.sidecar.clear();
    tank.idle_clip.clear();
    tank.hull = 140.0f;
    tank.shield = 30.0f;
    tank.speed = 220.0f;
    PlayerConfig p = base;
    apply_ship(p, tank);
    CHECK_THAT(p.start_health, WithinAbs(140.0, 1e-6));
    CHECK_THAT(p.start_shield, WithinAbs(30.0, 1e-6));
    CHECK_THAT(p.move_speed, WithinAbs(220.0, 1e-6));
    // The ship no longer touches the gun — that's apply_weapon's job.
    CHECK_THAT(p.weapon.fire_rate, WithinAbs(4.0, 1e-6));
    // Empty art fields keep the base ship's, so a stats-only ship needs no art.
    CHECK(p.sidecar == base.sidecar);
    CHECK(p.idle_clip == base.idle_clip);

    WeaponDef gat;
    gat.stats.fire_rate = 12.0f;
    gat.stats.damage = 6.0f;
    gat.fire_time = 8.0f;
    gat.recharge_time = 2.0f;
    BatteryConfig bat;
    apply_weapon(p, bat, gat);
    CHECK_THAT(p.weapon.fire_rate, WithinAbs(12.0, 1e-6));
    CHECK_THAT(p.weapon.damage, WithinAbs(6.0, 1e-6));
    CHECK_THAT(bat.fire_time, WithinAbs(8.0, 1e-6));
    CHECK_THAT(bat.recharge_time, WithinAbs(2.0, 1e-6));
}

TEST_CASE("GameData parses the pack roster: 4 ships, 4 weapons, scrap table", "[ships][data]") {
    const GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    REQUIRE(cfg.ships.size() >= 4);
    CHECK(cfg.ships[0].scrap_cost == 0);               // the starter is free
    CHECK_FALSE(cfg.ships[0].locked);
    CHECK(cfg.ships[3].locked);                        // the 4th drone is unreleased
    // Every ship's default weapon exists in the catalogue.
    for (const ShipDef& s : cfg.ships)
        CHECK(find_weapon(cfg.weapons, s.default_weapon) >= 0);
    REQUIRE(cfg.weapons.size() >= 4);
    // The gatling premise lives on Hailstorm now: fast cadence, low damage.
    const int hail = find_weapon(cfg.weapons, "Hailstorm");
    const int iron = find_weapon(cfg.weapons, "55 Iron");
    REQUIRE(hail >= 0); REQUIRE(iron >= 0);
    CHECK(cfg.weapons[hail].stats.fire_rate > cfg.weapons[iron].stats.fire_rate * 2.0f);
    CHECK(cfg.weapons[hail].stats.damage < cfg.weapons[iron].stats.damage * 0.5f);
    // 55 Iron must not silently retune the authored player weapon.
    CHECK_THAT(cfg.weapons[iron].stats.fire_rate, WithinAbs(cfg.player.weapon.fire_rate, 1e-6));
    CHECK(cfg.scrap.per_wave > 0);
    CHECK(cfg.scrap.boss_bonus > 0);
    CHECK(cfg.scrap.victory_bonus > 0);
}

TEST_CASE("meta save round-trips scrap, ownership and the loadout", "[meta_save][scrap]") {
    const std::string path = tmp_file("pack_roundtrip.json");
    std::filesystem::remove(path);

    MetaSave m;
    m.scrap = 275;
    m.owned_ships = {"Owl", "Gryphon"};
    m.equipped_ship = "Gryphon";
    m.equipped_weapon = "Moonshot";
    REQUIRE(meta_write(path, m));
    const MetaSave r = meta_load(path);
    CHECK(r.scrap == 275);
    REQUIRE(r.owned_ships.size() == 2);
    CHECK(r.owned_ships[0] == "Owl");
    CHECK(r.equipped_ship == "Gryphon");
    CHECK(r.equipped_weapon == "Moonshot");

    // A pre-pack save (no pack keys at all) loads clean with pack defaults.
    write_text(path, "{\"lifetime_score\": 500, \"prestige\": 1}");
    const MetaSave old = meta_load(path);
    CHECK(old.lifetime_score == 500);
    CHECK(old.scrap == 0);
    CHECK(old.owned_ships.empty());
    CHECK(old.equipped_ship.empty());
    // Negative scrap in a hand-edited file clamps to zero.
    write_text(path, "{\"scrap\": -40}");
    CHECK(meta_load(path).scrap == 0);
}

TEST_CASE("identity fields round-trip through meta save", "[meta]") {
    MetaSave m;
    m.player_id = "abc-123"; m.player_name = "Conrad"; m.registered = true;
    std::string path = "/tmp/rd_meta_test.json";
    REQUIRE(meta_write(path, m));
    MetaSave q = meta_load(path);
    REQUIRE(q.player_id == "abc-123");
    REQUIRE(q.player_name == "Conrad");
    REQUIRE(q.registered);
}
TEST_CASE("old save without identity fields loads with defaults", "[meta]") {
    std::string path = "/tmp/rd_meta_old.json";
    std::ofstream(path) << "{\"lifetime_score\": 500}";
    MetaSave q = meta_load(path);
    REQUIRE(q.lifetime_score == 500);
    REQUIRE(q.player_id.empty());
    REQUIRE_FALSE(q.registered);
}
TEST_CASE("uuid shape", "[meta]") {
    std::string u = generate_uuid();
    REQUIRE(u.size() == 36);
    REQUIRE(u[8] == '-'); REQUIRE(u[13] == '-'); REQUIRE(u[18] == '-'); REQUIRE(u[23] == '-');
    REQUIRE(u != generate_uuid());
}
