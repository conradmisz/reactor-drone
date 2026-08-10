/**
 * test_specialty.cpp — the four per-arena specialty units (#9, D68).
 *
 * Each theme fields one unit and the second pass over the same four themes
 * (waves 26-50) fields the same unit, harder. What fails silently here:
 *   - the wrong arena's unit (an index/name map that quietly resolves to -1);
 *   - a poison patch that never expires, i.e. a permanent hazard field;
 *   - a mine that triggers at the wrong radius (unhittable, or instant);
 *   - a splitter that splits into one, three, or forever.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/destruction.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/collision_layers.hpp"
#include "game/enemy_components.hpp"
#include "game/enemy_death_system.hpp"
#include "game/enemy_fire_system.hpp"
#include "game/player_components.hpp"
#include "game/specialty_system.hpp"
#include "game/wave_spawner_system.hpp"

#include <cmath>
#include <string>

using Catch::Matchers::WithinAbs;

namespace {

constexpr float PI = 3.14159265358979f;

GameConfig shipped() {
    return load_arena_config(project_paths::assets_dir() + "/GameData.json");
}

/// The behaviour kind an arena's specialty_unit resolves to, or SEEKER.
int arena_kind(const GameConfig& cfg, const std::string& name) {
    for (const ArenaDef& a : cfg.arenas) {
        if (a.name != name) continue;
        if (a.specialty_unit < 0) return behavior_kinds::SEEKER;
        return enemy_fire::behavior_kind_for(
            cfg.enemy_types[static_cast<size_t>(a.specialty_unit)].behavior);
    }
    return -1;
}

struct World {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    GameConfig cfg;
    SpecialtySystem sys;

    World() {
        bb.set<double>("delta_time", 0.25);
        sys.set_config(&cfg);
    }

    Entity player(float x, float y) {
        Entity p = em.create_entity();
        cs.add_component<Position>(p, Position{x, y});
        cs.add_component<Size>(p, Size{40.0f, 40.0f});
        cs.add_component<PlayerTag>(p, PlayerTag{});
        cs.add_component<Health>(p, Health{100.0f, 100.0f});
        return p;
    }

    Entity unit(int kind, int tier, float timer, float cooldown,
                float x = 0.0f, float y = 0.0f) {
        Entity e = em.create_entity();
        cs.add_component<Position>(e, Position{x, y});
        cs.add_component<Size>(e, Size{60.0f, 60.0f});
        cs.add_component<EnemyTag>(e, EnemyTag{});
        cs.add_component<Health>(e, Health{100.0f, 100.0f});
        cs.add_component<EnemyBehavior>(e, EnemyBehavior{kind, tier, timer, cooldown, 0.0f});
        return e;
    }

    size_t hazards() const {
        size_t n = 0;
        for (Entity e : cs.entities_with_component<ContactDamage>()) {
            auto c = cs.get_component<Collider>(e);
            if (c.has_value() && c->get().layer == layers::HAZARD) ++n;
        }
        return n;
    }
};

}  // namespace

TEST_CASE("each theme fields its own specialty unit, on both passes",
          "[Game][specialty][config]") {
    GameConfig cfg = shipped();

    CHECK(arena_kind(cfg, "Bio-lab")  == behavior_kinds::SPITTER);
    CHECK(arena_kind(cfg, "Foundry")  == behavior_kinds::MINER);
    CHECK(arena_kind(cfg, "Core")     == behavior_kinds::BULWARK);
    CHECK(arena_kind(cfg, "Prism")    == behavior_kinds::SPLITTER);
    // The second pass is the SAME unit — a different one would be a different
    // arena, not a harder one.
    CHECK(arena_kind(cfg, "Bio-lab II") == behavior_kinds::SPITTER);
    CHECK(arena_kind(cfg, "Foundry II") == behavior_kinds::MINER);
    CHECK(arena_kind(cfg, "Core II")    == behavior_kinds::BULWARK);
    CHECK(arena_kind(cfg, "Prism II")   == behavior_kinds::SPLITTER);

    for (const ArenaDef& a : cfg.arenas) CHECK(a.specialty_unit >= 0);
}

TEST_CASE("the specialty unit is picked by wave, and escalates on the second pass",
          "[Game][specialty][config]") {
    GameConfig cfg = shipped();
    REQUIRE(cfg.specialty.every_n_spawns > 0);

    // Wave 1 is Core (pass 1), wave 16 is Core II (pass 2) since Lane O (D125).
    const int a1 = active_arena_index(cfg.arenas, 1);
    const int a2 = active_arena_index(cfg.arenas, 16);
    REQUIRE(a1 >= 0);
    REQUIRE(a2 >= 0);
    CHECK(cfg.arenas[static_cast<size_t>(a1)].specialty_unit ==
          cfg.arenas[static_cast<size_t>(a2)].specialty_unit);
    CHECK(cfg.arenas[static_cast<size_t>(a1)].specialty_tier == 1);
    CHECK(cfg.arenas[static_cast<size_t>(a2)].specialty_tier == 2);
    CHECK(cfg.specialty.tier2_hp_mult > 1.0f);
    CHECK(cfg.specialty.tier2_speed_mult > 1.0f);
}

TEST_CASE("the injection cadence is deterministic and prefers the specialty unit",
          "[Game][specialty]") {
    SpecialtyConfig sp;
    sp.every_n_spawns = 7;
    sp.moon_every_n_spawns = 4;
    const std::vector<int> moons{3, 4};

    // Spawn 0..6 -> only the 4th (index 3) is a moon; the 7th (index 6) is the
    // specialty unit.
    CHECK(injected_type(0, sp, 9, moons) == -1);
    CHECK(injected_type(3, sp, 9, moons) == 3);
    CHECK(injected_type(6, sp, 9, moons) == 9);
    // Moons cycle rather than always picking the same one.
    CHECK(injected_type(7, sp, 9, moons) == 4);
    // Both cadences claim spawn index 27; the specialty unit outranks the moon.
    CHECK(injected_type(27, sp, 9, moons) == 9);
    // No specialty unit authored: the moon cadence still runs.
    CHECK(injected_type(6, sp, -1, moons) == -1);
    // Feature off: never injects.
    SpecialtyConfig off;
    CHECK(injected_type(6, off, 9, moons) == -1);
}

TEST_CASE("a poison patch damages the drone and expires", "[Game][specialty]") {
    World w;
    w.cfg.enemy_types.push_back(EnemyType{});
    w.cfg.enemy_types[0].behavior = "spitter";
    w.cfg.enemy_types[0].shot_damage = 13.0f;
    w.player(500.0f, 500.0f);
    w.unit(behavior_kinds::SPITTER, 1, 0.0f, 1000.0f);

    w.sys.update(w.cs, w.em, w.bb);
    REQUIRE(w.hazards() == 1);

    for (Entity e : w.cs.entities_with_component<ContactDamage>()) {
        auto col = w.cs.get_component<Collider>(e);
        if (!col.has_value() || col->get().layer != layers::HAZARD) continue;
        auto life = w.cs.get_component<Lifetime>(e);
        REQUIRE(life.has_value());
        // Short-lived by construction: a patch with no Lifetime is a permanent
        // hazard the arena never authored.
        CHECK(life->get().remaining > 0.0f);
        CHECK(life->get().remaining <= specialty::PATCH_LIFETIME * 2.0f);
        auto cd = w.cs.get_component<ContactDamage>(e);
        CHECK_THAT(cd->get().amount, WithinAbs(13.0f, 1e-4f));
        CHECK(cd->get().score == 0);
    }
}

TEST_CASE("a mine arms, then triggers only inside its radius", "[Game][specialty]") {
    World w;
    w.cfg.enemy_types.push_back(EnemyType{});
    w.cfg.enemy_types[0].behavior = "miner";
    w.cfg.enemy_types[0].shot_damage = 25.0f;
    // Player far away; the dropper sits at the origin.
    Entity p = w.player(5000.0f, 5000.0f);
    w.unit(behavior_kinds::MINER, 1, 0.0f, 1000.0f);

    w.sys.update(w.cs, w.em, w.bb);   // drops one mine at (30,30)

    // The mine is the tier-0 MINER entity; the dropper is tier 1.
    Entity mine = 0;
    for (Entity e : w.cs.entities_with_component<EnemyBehavior>()) {
        auto b = w.cs.get_component<EnemyBehavior>(e);
        if (b.has_value() && b->get().kind == behavior_kinds::MINER && b->get().tier == 0)
            mine = e;
    }
    REQUIRE(mine != 0);
    CHECK(w.hazards() == 0);          // a mine is not itself a hazard until it blows
    CHECK(w.cs.has_component<Lifetime>(mine));

    // Arm it, with the drone still out of range: nothing happens.
    for (int i = 0; i < 8; ++i) w.sys.update(w.cs, w.em, w.bb);
    CHECK_FALSE(w.cs.has_component<DestroyRequest>(mine));
    CHECK(w.hazards() == 0);

    // Walk the drone just inside the trigger radius.
    auto mp = w.cs.get_component<Position>(mine);
    auto pp = w.cs.get_component<Position>(p);
    REQUIRE(mp.has_value());
    pp->get().x = mp->get().x + specialty::MINE_SIZE * 0.5f - 20.0f;
    pp->get().y = mp->get().y + specialty::MINE_SIZE * 0.5f - 20.0f;
    w.sys.update(w.cs, w.em, w.bb);

    CHECK(w.cs.has_component<DestroyRequest>(mine));
    CHECK(w.hazards() == 1);          // the blast IS the damage, not the mine
}

TEST_CASE("the bulwark's armour is frontal only", "[Game][specialty]") {
    CHECK(specialty::inside_arc(0.0f, 0.0f, specialty::BULWARK_ARC));
    CHECK(specialty::inside_arc(0.0f, specialty::BULWARK_ARC * 0.99f,
                                specialty::BULWARK_ARC));
    CHECK_FALSE(specialty::inside_arc(0.0f, PI, specialty::BULWARK_ARC));   // flanked
    // Wrapping: facing just under +pi, attacked from just over -pi, is a hit
    // from the FRONT, not from behind.
    CHECK(specialty::inside_arc(3.1f, -3.1f, specialty::BULWARK_ARC));
    CHECK(specialty::BULWARK_ARMOR < 1.0f);

    // And it is actually written onto Health, where DamageApplySystem reads it.
    World w;
    w.player(1000.0f, 0.0f);
    Entity b = w.unit(behavior_kinds::BULWARK, 1, 1.0f, 1.0f);
    for (int i = 0; i < 40; ++i) w.sys.update(w.cs, w.em, w.bb);   // let it finish turning
    auto h = w.cs.get_component<Health>(b);
    REQUIRE(h.has_value());
    CHECK(h->get().armor_multiplier < 1.0f);
}

TEST_CASE("a splitter splits into exactly two, and the children do not",
          "[Game][specialty]") {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    EnemyDeathSystem death;
    death.set_economy(EconomyConfig{}, 1234u);

    Entity parent = em.create_entity();
    cs.add_component<Position>(parent, Position{100.0f, 100.0f});
    cs.add_component<Size>(parent, Size{80.0f, 80.0f});
    cs.add_component<Health>(parent, Health{0.0f, 200.0f});
    cs.add_component<EnemyTag>(parent, EnemyTag{});
    cs.add_component<ContactDamage>(parent, ContactDamage{20.0f, 40, 4, 1.0f});
    cs.add_component<PathFollower>(parent, PathFollower{1, 0.0f, 90.0f, 0, 0, 0, 0});
    cs.add_component<EnemyBehavior>(parent,
        EnemyBehavior{behavior_kinds::SPLITTER, 1, 0.0f, 1.0f, 0.0f});

    death.update(cs, em, bb);
    destroy_marked_entities(em, cs);

    auto enemies = cs.entities_with_component<EnemyTag>();
    REQUIRE(enemies.size() == 2);
    for (Entity c : enemies) {
        // No behaviour at all on a child: that is what bounds the recursion.
        CHECK_FALSE(cs.has_component<EnemyBehavior>(c));
        auto sz = cs.get_component<Size>(c);
        auto h = cs.get_component<Health>(c);
        REQUIRE(sz.has_value());
        REQUIRE(h.has_value());
        CHECK(sz->get().width < 80.0f);
        CHECK(h->get().max_hp < 200.0f);
        CHECK(h->get().current > 0.0f);
    }

    // ...and killing a child produces no grandchildren.
    for (Entity c : enemies) cs.get_component<Health>(c)->get().current = 0.0f;
    death.update(cs, em, bb);
    destroy_marked_entities(em, cs);
    CHECK(cs.entities_with_component<EnemyTag>().empty());
}
