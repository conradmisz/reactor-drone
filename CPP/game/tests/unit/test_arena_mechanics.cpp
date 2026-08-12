/**
 * test_arena_mechanics.cpp — the two new level types (roguelite phase 5,
 * design §4).
 *
 * [arena_mech] The Shroud's darkness curve and The Drift's current, plus the
 * re-banded 11-arena ladder they were inserted into.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/gamedata_loader.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/arena_mechanics.hpp"
#include "game/enemy_components.hpp"
#include "game/player_components.hpp"

using Catch::Matchers::WithinAbs;

namespace {

struct World {
    EntityManager em;
    ComponentStorage cs;
    Entity player = 0;

    World() {
        player = em.create_entity();
        cs.add_component<PlayerTag>(player, PlayerTag{});
        cs.add_component<Position>(player, Position{400.0f, 300.0f});
        cs.add_component<Size>(player, Size{32.0f, 32.0f});
    }

    Entity enemy(float x, float y) {
        Entity e = em.create_entity();
        cs.add_component<EnemyTag>(e, EnemyTag{});
        cs.add_component<Position>(e, Position{x, y});
        cs.add_component<Size>(e, Size{24.0f, 24.0f});
        cs.add_component<Color>(e, Color{200, 100, 50, 255});
        return e;
    }
    uint8_t alpha_of(Entity e) {
        auto t = cs.get_component<Tint>(e);
        return t.has_value() ? t->get().a : 255;
    }
};

}  // namespace

TEST_CASE("the shroud fade is full inside the light and never reaches zero", "[arena_mech]") {
    CHECK(arena_mechanics::shroud_alpha(0.0f, 260.0f) == 255);
    CHECK(arena_mechanics::shroud_alpha(260.0f, 260.0f) == 255);   // the edge is still lit
    CHECK(arena_mechanics::shroud_alpha(390.0f, 260.0f) < 255);    // halfway out, dimmer
    CHECK(arena_mechanics::shroud_alpha(390.0f, 260.0f) > arena_mechanics::SHROUD_MIN_ALPHA);
    // Past the fade it floors, rather than going invisible or wrapping negative.
    CHECK(arena_mechanics::shroud_alpha(520.0f, 260.0f) == arena_mechanics::SHROUD_MIN_ALPHA);
    CHECK(arena_mechanics::shroud_alpha(99999.0f, 260.0f) == arena_mechanics::SHROUD_MIN_ALPHA);
    // A light radius of 0 is the mechanic switched off — every other arena.
    CHECK(arena_mechanics::shroud_alpha(99999.0f, 0.0f) == 255);
}

TEST_CASE("the shroud dims distant enemies and keeps their hue", "[arena_mech]") {
    World w;
    Entity close = w.enemy(420.0f, 300.0f);
    Entity far = w.enemy(1400.0f, 300.0f);

    arena_mechanics::tick_shroud(w.cs, 260.0f);
    CHECK(w.alpha_of(close) == 255);
    CHECK(w.alpha_of(far) == arena_mechanics::SHROUD_MIN_ALPHA);
    // The hue is the enemy's own arena tint, only its alpha moved.
    auto t = w.cs.get_component<Tint>(far);
    REQUIRE(t.has_value());
    CHECK(t->get().r == 200);
    CHECK(t->get().g == 100);
    CHECK(t->get().b == 50);
}

TEST_CASE("a flashing enemy is never dimmed by the shroud", "[arena_mech]") {
    // Being lit up when you hit it is the one piece of feedback darkness must
    // not eat — and writing Tint under a live Flash would fight FlashSystem.
    World w;
    Entity far = w.enemy(1400.0f, 300.0f);
    w.cs.add_component<Flash>(far, Flash{0.12f, 0.12f, 255, 255, 255});

    arena_mechanics::tick_shroud(w.cs, 260.0f);
    CHECK_FALSE(w.cs.has_component<Tint>(far));
}

TEST_CASE("the shroud is inert with no player and no light radius", "[arena_mech]") {
    World w;
    Entity e = w.enemy(1400.0f, 300.0f);
    arena_mechanics::tick_shroud(w.cs, 0.0f);
    CHECK_FALSE(w.cs.has_component<Tint>(e));

    // No player (between a death and a respawn): nothing to measure from.
    World w2;
    Entity lone = w2.enemy(1400.0f, 300.0f);
    w2.cs.remove_component<PlayerTag>(w2.player);
    arena_mechanics::tick_shroud(w2.cs, 260.0f);
    CHECK_FALSE(w2.cs.has_component<Tint>(lone));
}

TEST_CASE("the drift pushes the drone, the enemies and the loot alike", "[arena_mech]") {
    World w;
    Entity e = w.enemy(500.0f, 300.0f);
    Entity loot = w.em.create_entity();
    w.cs.add_component<Pickup>(loot, Pickup{});
    w.cs.add_component<Position>(loot, Position{600.0f, 300.0f});

    arena_mechanics::tick_drift(w.cs, 38.0f, -14.0f, 0.5f);
    CHECK_THAT(w.cs.get_component<Position>(w.player)->get().x, WithinAbs(419.0f, 0.001f));
    CHECK_THAT(w.cs.get_component<Position>(w.player)->get().y, WithinAbs(293.0f, 0.001f));
    CHECK_THAT(w.cs.get_component<Position>(e)->get().x, WithinAbs(519.0f, 0.001f));
    CHECK_THAT(w.cs.get_component<Position>(loot)->get().x, WithinAbs(619.0f, 0.001f));

    // No current, no dt: both are no-ops rather than NaN or a one-frame jump.
    arena_mechanics::tick_drift(w.cs, 0.0f, 0.0f, 0.5f);
    CHECK_THAT(w.cs.get_component<Position>(e)->get().x, WithinAbs(519.0f, 0.001f));
    arena_mechanics::tick_drift(w.cs, 38.0f, -14.0f, 0.0f);
    CHECK_THAT(w.cs.get_component<Position>(e)->get().x, WithinAbs(519.0f, 0.001f));
}

TEST_CASE("the re-banded ladder covers all 30 waves and holds its own themes",
          "[arena_mech][data]") {
    const GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    REQUIRE(cfg.arenas.size() == 11);

    // Strictly ascending first_wave, starting at 1 — active_arena_index picks the
    // LAST arena whose first_wave <= the current wave, so a duplicate or an
    // out-of-order band would silently make one theme unreachable.
    CHECK(cfg.arenas[0].first_wave == 1);
    for (size_t i = 1; i < cfg.arenas.size(); ++i) {
        INFO("arena " << cfg.arenas[i].name);
        CHECK(cfg.arenas[i].first_wave > cfg.arenas[i - 1].first_wave);
    }
    // Every wave of the 30-wave arc resolves to some arena.
    for (int wave = 1; wave <= 30; ++wave) {
        const int idx = active_arena_index(cfg.arenas, wave);
        INFO("wave " << wave);
        CHECK(idx >= 0);
        CHECK(idx < static_cast<int>(cfg.arenas.size()));
    }

    // The two new themes are in the ladder and each carries exactly its own
    // mechanic — a theme with both would have no identity of its own.
    int shroud = -1, drift = -1;
    for (size_t i = 0; i < cfg.arenas.size(); ++i) {
        if (cfg.arenas[i].name == "The Shroud") shroud = static_cast<int>(i);
        if (cfg.arenas[i].name == "The Drift") drift = static_cast<int>(i);
    }
    REQUIRE(shroud >= 0);
    REQUIRE(drift >= 0);
    CHECK(cfg.arenas[shroud].light_radius > 0.0f);
    CHECK(cfg.arenas[shroud].drift_x == 0.0f);
    CHECK(cfg.arenas[shroud].drift_y == 0.0f);
    CHECK((cfg.arenas[drift].drift_x != 0.0f || cfg.arenas[drift].drift_y != 0.0f));
    CHECK(cfg.arenas[drift].light_radius == 0.0f);

    // The current must stay under the slowest enemy's speed, or it could pin
    // something against the wall forever and stall the wave-clear gate.
    float slowest = 1e9f;
    for (const auto& t : cfg.enemy_types) slowest = std::min(slowest, t.speed);
    const float current = std::sqrt(cfg.arenas[drift].drift_x * cfg.arenas[drift].drift_x +
                                   cfg.arenas[drift].drift_y * cfg.arenas[drift].drift_y);
    CHECK(current < slowest);

    // And the nine original themes must not have picked up a mechanic by accident.
    for (const auto& a : cfg.arenas) {
        if (a.name == "The Shroud" || a.name == "The Drift") continue;
        INFO("arena " << a.name);
        CHECK(a.light_radius == 0.0f);
        CHECK(a.drift_x == 0.0f);
        CHECK(a.drift_y == 0.0f);
    }
}
