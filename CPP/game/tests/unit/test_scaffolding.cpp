/**
 * test_scaffolding.cpp — the iteration-3 Phase 0 contract (D51).
 *
 * Phase 0 adds no behaviour. What it adds is a set of promises that five parallel
 * lanes are about to build on, and every one of them is the kind of thing that
 * fails silently:
 *
 *   - the two new component types really are registered (a missing explicit
 *     instantiation is a link error in the lane's build, not here, and reads like
 *     a build-system fault);
 *   - enemy shots can reach the player and cannot reach enemies (a wrong mask bit
 *     is a projectile that passes straight through the drone);
 *   - the new config blocks parse, and their defaults are INERT — the whole point
 *     of the phase is that the game plays exactly as it did before it.
 *
 * When a lane lands, its own tests replace the "inert" assertions here.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>

#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/destruction.hpp"
#include "engine/ecs/fx_events.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/collision_layers.hpp"
#include "game/enemy_components.hpp"
#include "game/player_components.hpp"

using Catch::Matchers::WithinAbs;

namespace {

/// The collision rule both bodies must satisfy, mirroring collision_math.
bool collide(uint8_t a_layer, uint8_t a_mask, uint8_t b_layer, uint8_t b_mask) {
    return (a_layer & b_mask) != 0 && (b_layer & a_mask) != 0;
}

}  // namespace

TEST_CASE("the new component types are registered and swept on destroy",
          "[Game][scaffold]") {
    EntityManager em;
    ComponentStorage cs;

    Entity shot = em.create_entity();
    cs.add_component<EnemyShot>(shot, EnemyShot{});
    cs.add_component<EnemyBehavior>(shot, EnemyBehavior{behavior_kinds::SHOOTER, 2, 0.5f, 1.5f, 0.0f});

    REQUIRE(cs.has_component<EnemyShot>(shot));
    REQUIRE(cs.entities_with_component<EnemyShot>().size() == 1);
    auto b = cs.get_component<EnemyBehavior>(shot);
    REQUIRE(b.has_value());
    CHECK(b->get().kind == behavior_kinds::SHOOTER);
    CHECK(b->get().tier == 2);

    // A dead shot that kept its tag would make the next entity to reuse that id
    // look like a live projectile — the same ghost-component bug the UI layer hit.
    cs.add_component<DestroyRequest>(shot, DestroyRequest{});
    destroy_marked_entities(em, cs);
    CHECK_FALSE(cs.has_component<EnemyShot>(shot));
    CHECK_FALSE(cs.has_component<EnemyBehavior>(shot));
}

TEST_CASE("an enemy shot hits the drone and nothing else", "[Game][scaffold][layers]") {
    using namespace layers;

    CHECK(collide(ENEMY_SHOT, ENEMY_SHOT_MASK, PLAYER, PLAYER_MASK));
    CHECK(collide(ENEMY_SHOT, ENEMY_SHOT_MASK, OBSTACLE, OBSTACLE_MASK));
    // No friendly fire in either direction.
    CHECK_FALSE(collide(ENEMY_SHOT, ENEMY_SHOT_MASK, ENEMY, ENEMY_MASK));
    CHECK_FALSE(collide(PROJECTILE, PROJECTILE_MASK, PLAYER, PLAYER_MASK));
    // The bit is genuinely new — it must not alias an existing layer.
    CHECK((ENEMY_SHOT & (PLAYER | ENEMY | PROJECTILE | OBSTACLE | HAZARD)) == 0);
    // Everything that worked before still works.
    CHECK(collide(ENEMY, ENEMY_MASK, PLAYER, PLAYER_MASK));
    CHECK(collide(HAZARD, HAZARD_MASK, PLAYER, PLAYER_MASK));
    CHECK(collide(PROJECTILE, PROJECTILE_MASK, ENEMY, ENEMY_MASK));
}

TEST_CASE("the scaffolded config blocks parse from the shipped GameData",
          "[Game][scaffold][config]") {
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");

    // Present and parsed...
    CHECK(cfg.sustain.max_live == 3);
    CHECK_THAT(cfg.dash.duration, WithinAbs(0.15f, 1e-5f));
    CHECK(cfg.minimap.max_blips == 120);
    CHECK_THAT(cfg.boss.health_growth, WithinAbs(1.6f, 1e-5f));

    // ...and INERT. These assertions were the whole gate on Phase 0: they said
    // "no behaviour shipped". The lane that turns each feature on deletes its
    // line here and asserts the real thing in its own test file.
    // Lane B (D56/D58): `sustain.interval` and `minimap.enabled` are live, and
    // asserted in test_sustain_spawn.cpp / test_minimap.cpp.
    // Lane D (D66-D75): `actives` is live, asserted in test_active_items.cpp.

    // Boss flags are live as of Lane A (D53) — asserted in test_wave_arc.cpp.
    // `actives`, the enemy behaviours and ArenaDef::specialty_unit went live with
    // Lane D (D66-D75); their three "still inert" lines were deleted here and the
    // real behaviour is asserted in test_enemy_fire / test_specialty / test_boss.
}

TEST_CASE("the scaffolded ShipState fields default to off", "[Game][scaffold]") {
    ShipState s;
    CHECK_THAT(s.dash_cd, WithinAbs(0.0f, 1e-5f));
    CHECK_THAT(s.dash_timer, WithinAbs(0.0f, 1e-5f));
    CHECK(s.active_id == -1);
    CHECK_THAT(s.active_cd, WithinAbs(0.0f, 1e-5f));
    for (int lvl : s.gear_levels) CHECK(lvl == 0);

    // Health/Shield joined the same enum the drop path already switches on, so
    // their values must not collide with the two that ship today.
    CHECK(static_cast<int>(PickupKind::Health) == 2);
    CHECK(static_cast<int>(PickupKind::Shield) == 3);
}

TEST_CASE("every main.cpp hook a lane is told to own actually exists",
          "[Game][scaffold][hooks]") {
    // The plan hands each lane exactly one comment-delimited block in main.cpp and
    // forbids it from editing anything else there. If a hook is missing or gets
    // renamed, that lane silently has nowhere to land — so pin the names.
    const std::string path = std::string(GAME_SOURCE_DIR) + "/main.cpp";
    FILE* f = std::fopen(path.c_str(), "rb");
    REQUIRE(f != nullptr);
    std::string src;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) src.append(buf, n);
    std::fclose(f);

    for (const char* hook : {"dash", "enemy-fire", "specialty", "boss", "actives",
                             "sustain-spawn", "minimap", "arena-vfx", "shop-menu",
                             // Engine-suite Phase 0 (D138): one landing pad per lane.
                             "timescale", "director", "forces", "surge", "crumble",
                             "pattern", "telemetry", "grid-render", "scars-render",
                             "palette", "audio"}) {
        const std::string open = std::string("// === HOOK: ") + hook + " ===";
        const std::string close = std::string("// === END HOOK: ") + hook + " ===";
        INFO("hook: " << hook);
        CHECK(src.find(open) != std::string::npos);
        CHECK(src.find(close) != std::string::npos);
        CHECK(src.find(open) < src.find(close));
    }
}

// === Engine-suite Phase 0 (D138) ===============================================
// Same contract as the iteration-3 phase above: the blocks parse from the shipped
// GameData, and their defaults are INERT. The lane that turns each feature on
// deletes its lines here and asserts the real thing in its own test file.

TEST_CASE("the engine-suite config blocks parse and default inert",
          "[Game][scaffold][suite]") {
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");

    // Present and parsed (defaults reachable through the struct)...
    CHECK(cfg.resonance.cols == 40);
    CHECK(cfg.resonance.rows == 28);
    CHECK(cfg.forces.max_sources == 32);
    CHECK(cfg.flight_report.max_samples == 4096);
    CHECK(cfg.audio.voices == 8);
    CHECK_THAT(cfg.timescale.min_scale, WithinAbs(0.35f, 1e-5f));
    CHECK_THAT(cfg.director.min_mult, WithinAbs(0.7f, 1e-5f));

    // ...and INERT. These lines ARE the Phase-0 gate: no behaviour shipped.
    CHECK_FALSE(cfg.timescale.enabled);   // Lane P deletes this line
    CHECK_FALSE(cfg.director.enabled);    // Lane Q
    CHECK_FALSE(cfg.resonance.enabled);        // Lane R
    CHECK_FALSE(cfg.flight_report.enabled);  // Lane S
    CHECK_FALSE(cfg.scars.enabled);       // Lane V
    CHECK_FALSE(cfg.palettes.enabled);    // Lane W
    CHECK(cfg.patterns.empty());          // Lane Y
    CHECK_FALSE(cfg.audio.enabled);       // Lane Z
    for (const ArenaDef& a : cfg.arenas) CHECK(a.surges.empty());   // Lane X
    for (const ArenaDef& a : cfg.arenas)
        for (const ObstacleDef& o : a.obstacles) CHECK(o.hp == 0.0f);  // Lane U
    for (const EnemyType& t : cfg.enemy_types) CHECK(t.pattern.empty());  // Lane Y
}

TEST_CASE("the fx-events vocabulary round-trips and clears per frame",
          "[Game][scaffold][suite]") {
    Blackboard bb;
    // Publishers may fire before the first clear — get_or covers the cold start.
    fx_events::push_impulse(bb, 100.0f, 200.0f, 2.5f);
    fx_events::push_impulse(bb, 5.0f, 6.0f, 1.0f);
    fx_events::push_stamp(bb, 50.0f, 60.0f, 3, 1.5f, 0.7f);

    auto imps = bb.get<std::vector<fx_events::Impulse>>(fx_events::GRID_IMPULSES);
    REQUIRE(imps.size() == 2);
    CHECK_THAT(imps[0].strength, WithinAbs(2.5f, 1e-6f));
    auto stamps = bb.get<std::vector<fx_events::Stamp>>(fx_events::SCAR_STAMPS);
    REQUIRE(stamps.size() == 1);
    CHECK(stamps[0].kind == 3);

    fx_events::clear_frame(bb);
    CHECK(bb.get<std::vector<fx_events::Impulse>>(fx_events::GRID_IMPULSES).empty());
    CHECK(bb.get<std::vector<fx_events::Stamp>>(fx_events::SCAR_STAMPS).empty());
}
