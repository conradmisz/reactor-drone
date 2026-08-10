/**
 * Unit tests for the mid-run save/quit and resume (Lane K, D100/D102).
 *
 * Two things are being defended here. First, the file is player-editable, so the
 * load path is tested against garbage at least as hard as against a good file —
 * the meta-save's standard (D80). Second, and the reason this file exists at
 * all: a save must never be able to change a run it is not part of. The
 * determinism guard is that `run_save_apply` does nothing for a struct that is
 * not `present`, which is the only state a fresh run ever sees.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "game/player_components.hpp"
#include "game/run_save.hpp"

using Catch::Matchers::WithinAbs;

namespace {

std::string tmp_file(const char* name) {
    std::filesystem::path p = std::filesystem::temp_directory_path() / "reactor_drone_run_save_tests";
    std::filesystem::create_directories(p);
    return (p / name).string();
}

void write_text(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::trunc);
    out << text;
}

/// A minimal "just spawned" player, as spawn_world() would leave it.
Entity fresh_player(EntityManager& em, ComponentStorage& cs) {
    Entity p = em.create_entity();
    cs.add_component<PlayerTag>(p, PlayerTag{});
    cs.add_component<Health>(p, Health{100.0f, 100.0f});
    cs.add_component<ShipState>(p, ShipState{});
    cs.add_component<WeaponStats>(p, WeaponStats{});
    return p;
}

}  // namespace

TEST_CASE("a run save round-trips every field", "[run_save]") {
    const std::string path = tmp_file("roundtrip.json");
    std::filesystem::remove(path);

    RunSave s;
    s.present = true;
    s.seed = 4242u;
    s.difficulty = 1;
    s.difficulty_name = "Hard";
    s.ship_id = 1;
    s.wave = 7;
    s.score = 31337;
    s.hull = 44.5f;   s.hull_max = 120.0f;
    s.shield = 8.0f;  s.shield_max = 30.0f;
    s.shield_regen = 4.0f; s.shield_delay = 2.5f;
    s.credits = 512;  s.keys = 3;
    s.speed_mult = 1.25f;
    s.item_id = item_ids::MAGNET_CORE;
    s.consumable_id = consumable_ids::OVERDRIVE;
    s.active_id = 2;
    s.extra_shots = 2;
    s.upg_counts[0] = 4;  s.upg_counts[7] = 1;
    s.gear_levels[3] = 2;
    s.fire_rate = 9.5f; s.damage = 33.0f;
    s.projectile_speed = 640.0f; s.projectile_lifetime = 0.9f; s.spread = 0.07f;

    REQUIRE(run_save_write(path, s));
    const RunSave r = run_save_load(path);

    REQUIRE(r.present);
    CHECK(r.seed == 4242u);
    CHECK(r.difficulty == 1);
    CHECK(r.difficulty_name == "Hard");
    CHECK(r.ship_id == 1);
    CHECK(r.wave == 7);
    CHECK(r.score == 31337);
    CHECK(r.credits == 512);
    CHECK(r.keys == 3);
    CHECK(r.item_id == item_ids::MAGNET_CORE);
    CHECK(r.consumable_id == consumable_ids::OVERDRIVE);
    CHECK(r.active_id == 2);
    CHECK(r.extra_shots == 2);
    CHECK(r.upg_counts[0] == 4);
    CHECK(r.upg_counts[7] == 1);
    CHECK(r.gear_levels[3] == 2);
    CHECK_THAT(r.hull, WithinAbs(44.5, 1e-4));
    CHECK_THAT(r.hull_max, WithinAbs(120.0, 1e-4));
    CHECK_THAT(r.shield, WithinAbs(8.0, 1e-4));
    CHECK_THAT(r.shield_max, WithinAbs(30.0, 1e-4));
    CHECK_THAT(r.speed_mult, WithinAbs(1.25, 1e-4));
    CHECK_THAT(r.fire_rate, WithinAbs(9.5, 1e-4));
    CHECK_THAT(r.damage, WithinAbs(33.0, 1e-4));
    CHECK_THAT(r.spread, WithinAbs(0.07, 1e-4));
}

TEST_CASE("run save write creates a missing directory", "[run_save]") {
    std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "reactor_drone_run_save_tests" / "nested";
    std::filesystem::remove_all(dir);
    const std::string path = (dir / "run.json").string();

    RunSave s;
    s.present = true;
    s.wave = 3;
    REQUIRE(run_save_write(path, s));
    REQUIRE(run_save_load(path).wave == 3);
}

TEST_CASE("a missing run save is simply 'no save'", "[run_save]") {
    const std::string path = tmp_file("absent.json");
    std::filesystem::remove(path);
    CHECK_FALSE(run_save_load(path).present);
}

TEST_CASE("a corrupt run save never yields a usable run", "[run_save]") {
    const std::string path = tmp_file("corrupt.json");

    SECTION("not JSON at all") {
        write_text(path, "{ this is not json");
        CHECK_FALSE(run_save_load(path).present);
    }
    SECTION("empty file") {
        write_text(path, "");
        CHECK_FALSE(run_save_load(path).present);
    }
    SECTION("valid JSON of the wrong shape") {
        write_text(path, "[1,2,3]");
        CHECK_FALSE(run_save_load(path).present);
    }
    SECTION("an unknown version is not read at all") {
        write_text(path, "{\"version\": 99, \"wave\": 12}");
        CHECK_FALSE(run_save_load(path).present);
    }
    SECTION("no version key") {
        write_text(path, "{\"wave\": 12}");
        CHECK_FALSE(run_save_load(path).present);
    }
}

TEST_CASE("a partial or hostile run save falls back field by field", "[run_save]") {
    const std::string path = tmp_file("partial.json");

    // Right version, one good field, one of the wrong type, one negative, and
    // everything else absent. The run must still be resumable.
    write_text(path,
               "{\"version\": 1, \"wave\": 5, \"score\": \"lots\", \"credits\": -20,"
               " \"hull\": null, \"upg_counts\": [1, \"x\", 3]}");
    const RunSave r = run_save_load(path);

    REQUIRE(r.present);
    CHECK(r.wave == 5);
    CHECK(r.score == 0);        // wrong type -> default
    CHECK(r.credits == 0);      // negative -> clamped
    CHECK(r.upg_counts[0] == 1);
    CHECK(r.upg_counts[1] == 0);   // wrong element type -> default
    CHECK(r.upg_counts[2] == 3);
    CHECK_THAT(r.hull, WithinAbs(0.0, 1e-6));
}

TEST_CASE("run_save_clear removes the file and never throws", "[run_save]") {
    const std::string path = tmp_file("clearme.json");
    RunSave s; s.present = true; s.wave = 2;
    REQUIRE(run_save_write(path, s));
    run_save_clear(path);
    CHECK_FALSE(run_save_load(path).present);
    CHECK_NOTHROW(run_save_clear(path));   // clearing a file that is already gone
}

TEST_CASE("resume reconstructs the run's state on a freshly built world", "[run_save][resume]") {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    Entity p = fresh_player(em, cs);

    RunSave s;
    s.present = true;
    s.score = 900;
    s.extra_shots = 3;
    s.hull = 37.0f; s.hull_max = 140.0f;
    s.shield = 12.0f; s.shield_max = 25.0f; s.shield_regen = 5.0f; s.shield_delay = 2.0f;
    s.credits = 77; s.keys = 2;
    s.speed_mult = 1.4f;
    s.item_id = item_ids::SALVAGER;
    s.consumable_id = consumable_ids::EMP_BURST;
    s.gear_levels[1] = 3;
    s.upg_counts[2] = 5;
    s.fire_rate = 11.0f; s.damage = 7.5f;
    s.projectile_speed = 700.0f; s.projectile_lifetime = 0.8f; s.spread = 0.11f;

    run_save_apply(s, cs, bb);

    CHECK(bb.get_or<int>("score", -1) == 900);
    CHECK(bb.get_or<int>("ship.extra_shots", -1) == 3);

    auto h = cs.get_component<Health>(p);
    REQUIRE(h.has_value());
    CHECK_THAT(h->get().max_hp, WithinAbs(140.0, 1e-4));
    CHECK_THAT(h->get().current, WithinAbs(37.0, 1e-4));

    auto st = cs.get_component<ShipState>(p);
    REQUIRE(st.has_value());
    CHECK(st->get().currency == 77);
    CHECK(st->get().keys == 2);
    CHECK_THAT(st->get().shield, WithinAbs(12.0, 1e-4));
    CHECK_THAT(st->get().shield_max, WithinAbs(25.0, 1e-4));
    CHECK_THAT(st->get().speed_mult, WithinAbs(1.4, 1e-4));
    CHECK(st->get().item_id == item_ids::SALVAGER);
    CHECK(st->get().consumable_id == consumable_ids::EMP_BURST);
    CHECK(st->get().gear_levels[1] == 3);
    CHECK(st->get().upg_counts[2] == 5);

    auto w = cs.get_component<WeaponStats>(p);
    REQUIRE(w.has_value());
    CHECK_THAT(w->get().fire_rate, WithinAbs(11.0, 1e-4));
    CHECK_THAT(w->get().damage, WithinAbs(7.5, 1e-4));
    CHECK_THAT(w->get().spread, WithinAbs(0.11, 1e-4));
}

TEST_CASE("a capture/apply round-trip through the world is lossless", "[run_save][resume]") {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    Entity p = fresh_player(em, cs);

    // Bring the "live" run somewhere interesting.
    cs.get_component<Health>(p)->get().current = 61.0f;
    ShipState& live = cs.get_component<ShipState>(p)->get();
    live.currency = 240; live.keys = 1; live.shield_max = 40.0f; live.shield = 15.0f;
    live.gear_levels[0] = 2; live.upg_counts[1] = 6;
    bb.set<int>("score", 4321);
    bb.set<int>("ship.extra_shots", 1);

    const RunSave s = run_save_capture(cs, bb, /*wave=*/11, /*difficulty=*/1, "Hard",
                                       /*ship_id=*/1, /*seed=*/99u);
    CHECK(s.present);
    CHECK(s.wave == 11);
    CHECK(s.score == 4321);
    CHECK(s.credits == 240);

    // A brand-new world, as a resumed process would have.
    EntityManager em2;
    ComponentStorage cs2;
    Blackboard bb2;
    Entity p2 = fresh_player(em2, cs2);
    run_save_apply(s, cs2, bb2);

    CHECK(bb2.get_or<int>("score", -1) == 4321);
    CHECK(bb2.get_or<int>("ship.extra_shots", -1) == 1);
    CHECK_THAT(cs2.get_component<Health>(p2)->get().current, WithinAbs(61.0, 1e-4));
    const ShipState& back = cs2.get_component<ShipState>(p2)->get();
    CHECK(back.currency == 240);
    CHECK(back.keys == 1);
    CHECK(back.gear_levels[0] == 2);
    CHECK(back.upg_counts[1] == 6);
}

TEST_CASE("an existing save cannot perturb a run it is not part of", "[run_save][determinism]") {
    // The determinism guarantee in one assertion: the only thing a fresh run ever
    // hands to run_save_apply is a struct that is not `present` (main.cpp calls it
    // exclusively from the resume branch of start_run), and that is a no-op. The
    // file is otherwise read once, at startup, into a struct the simulation never
    // sees — the same discipline the meta-save keeps (D80-D83). The end-to-end
    // proof is the replay canary, run with and without saves/run.json.
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    Entity p = fresh_player(em, cs);
    bb.set<int>("score", 0);

    RunSave absent;                  // present == false
    absent.score = 999999;           // ... with tempting contents
    absent.credits = 999999;
    absent.hull = 1.0f;
    run_save_apply(absent, cs, bb);

    CHECK(bb.get_or<int>("score", -1) == 0);
    CHECK(cs.get_component<ShipState>(p)->get().currency == 0);
    CHECK_THAT(cs.get_component<Health>(p)->get().current, WithinAbs(100.0, 1e-4));
}

TEST_CASE("a truncated save resumes as a live drone, never a corpse", "[run_save][resume]") {
    // hull/hull_max absent from the file: the freshly-spawned full-health values
    // must survive, because "resume" that spawns you dead is worse than no save.
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    Entity p = fresh_player(em, cs);

    RunSave s;
    s.present = true;
    s.wave = 4;
    run_save_apply(s, cs, bb);

    CHECK_THAT(cs.get_component<Health>(p)->get().current, WithinAbs(100.0, 1e-4));
    CHECK_THAT(cs.get_component<Health>(p)->get().max_hp, WithinAbs(100.0, 1e-4));
    CHECK_THAT(cs.get_component<WeaponStats>(p)->get().fire_rate, WithinAbs(4.0, 1e-4));
}

TEST_CASE("the run save cannot collide with the meta save", "[run_save]") {
    CHECK(run_save_path() != std::string());
    CHECK(run_save_path().find("run.json") != std::string::npos);
    CHECK(run_save_path().find("meta.json") == std::string::npos);
}
