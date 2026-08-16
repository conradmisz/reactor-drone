/**
 * test_bullet_pattern.cpp — the danmaku interpreter (engine suite, Lane Y, D148).
 *
 * `op_angles` is the whole choreography and it is pure, so the shapes are pinned
 * here without a world. The rest of the tests cover the things that would turn a
 * boss fight into a crash or a frame-eater: name resolution, the shot ceiling, and
 * an unknown op being ignored rather than firing garbage.
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/bullet_pattern.hpp"

using Catch::Matchers::WithinAbs;

namespace {

constexpr float TAU = 6.28318530717958647692f;

BulletPatternOp op(const char* type, int count) {
    BulletPatternOp o;
    o.type = type;
    o.count = count;
    return o;
}

}  // namespace

TEST_CASE("op names resolve to kinds, never to row indices", "[Game][pattern]") {
    using bullet_pattern::OpKind;
    CHECK(bullet_pattern::op_kind_for("ring") == OpKind::Ring);
    CHECK(bullet_pattern::op_kind_for("fan") == OpKind::Fan);
    CHECK(bullet_pattern::op_kind_for("spiral") == OpKind::Spiral);
    CHECK(bullet_pattern::op_kind_for("aimed") == OpKind::Aimed);
    CHECK(bullet_pattern::op_kind_for("wait") == OpKind::Wait);
    CHECK(bullet_pattern::op_kind_for("") == OpKind::Unknown);
    CHECK(bullet_pattern::op_kind_for("nonsense") == OpKind::Unknown);
}

TEST_CASE("a ring is evenly spaced over the full circle", "[Game][pattern]") {
    std::vector<float> a;
    bullet_pattern::op_angles(op("ring", 8), /*phase=*/0.0f, /*aim=*/1.0f, a);
    REQUIRE(a.size() == 8);
    for (size_t i = 1; i < a.size(); ++i)
        CHECK_THAT(a[i] - a[i - 1], WithinAbs(TAU / 8.0f, 1e-5f));
}

TEST_CASE("the phase rotates a ring without changing its shape", "[Game][pattern]") {
    std::vector<float> a, b;
    bullet_pattern::op_angles(op("ring", 6), 0.0f, 0.0f, a);
    bullet_pattern::op_angles(op("ring", 6), 0.7f, 0.0f, b);
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i)
        CHECK_THAT(b[i] - a[i], WithinAbs(0.7f, 1e-5f));
}

TEST_CASE("a fan is centred on the phase, an aimed fan on the player",
          "[Game][pattern]") {
    BulletPatternOp f = op("fan", 5);
    f.spread_deg = 40.0f;
    std::vector<float> a;
    bullet_pattern::op_angles(f, /*phase=*/1.0f, /*aim=*/2.5f, a);
    REQUIRE(a.size() == 5);
    CHECK_THAT(a[2], WithinAbs(1.0f, 1e-5f));                    // centre = phase
    CHECK_THAT(a.back() - a.front(), WithinAbs(40.0f * TAU / 360.0f, 1e-5f));

    BulletPatternOp aimed = f;
    aimed.type = "aimed";
    bullet_pattern::op_angles(aimed, 1.0f, 2.5f, a);
    CHECK_THAT(a[2], WithinAbs(2.5f, 1e-5f));                    // centre = the drone
}

TEST_CASE("a single-shot fan fires exactly at its centre", "[Game][pattern]") {
    BulletPatternOp f = op("fan", 1);
    f.spread_deg = 90.0f;
    std::vector<float> a;
    bullet_pattern::op_angles(f, 0.3f, 9.0f, a);
    REQUIRE(a.size() == 1);
    CHECK_THAT(a[0], WithinAbs(0.3f, 1e-5f));   // not centre - spread/2
}

TEST_CASE("wait and unknown ops emit nothing", "[Game][pattern]") {
    std::vector<float> a{1.0f, 2.0f};           // pre-dirtied: must be cleared
    bullet_pattern::op_angles(op("wait", 8), 0.0f, 0.0f, a);
    CHECK(a.empty());
    bullet_pattern::op_angles(op("nonsense", 8), 0.0f, 0.0f, a);
    CHECK(a.empty());
    bullet_pattern::op_angles(op("ring", 0), 0.0f, 0.0f, a);
    CHECK(a.empty());
}

TEST_CASE("one op can never exceed the shot ceiling", "[Game][pattern]") {
    std::vector<float> a;
    // A mistyped count is a typo, not a design: the ceiling is what stops it
    // costing a frame (each shot carries a trail emitter, ~10 live particles).
    bullet_pattern::op_angles(op("ring", 100000), 0.0f, 0.0f, a);
    CHECK(a.size() == static_cast<size_t>(bullet_pattern::MAX_SHOTS_PER_OP));
    bullet_pattern::op_angles(op("ring", -5), 0.0f, 0.0f, a);
    CHECK(a.empty());
}

TEST_CASE("patterns resolve by NAME against the shipped data", "[Game][pattern]") {
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    REQUIRE(cfg.patterns.size() >= 2);

    const int bloom = bullet_pattern::pattern_index(cfg.patterns, "reactor_bloom");
    REQUIRE(bloom >= 0);
    CHECK(cfg.patterns[static_cast<size_t>(bloom)].name == "reactor_bloom");
    CHECK(bullet_pattern::pattern_index(cfg.patterns, "no_such_pattern") == -1);
    CHECK(bullet_pattern::pattern_index(cfg.patterns, "") == -1);

    // Every authored op is a kind the interpreter knows — an unknown one would
    // silently emit nothing, which reads as a broken boss rather than a typo.
    for (const BulletPatternDef& p : cfg.patterns) {
        REQUIRE_FALSE(p.ops.empty());
        for (const BulletPatternOp& o : p.ops) {
            INFO("pattern " << p.name << " op " << o.type);
            CHECK(bullet_pattern::op_kind_for(o.type) != bullet_pattern::OpKind::Unknown);
            CHECK(o.count <= bullet_pattern::MAX_SHOTS_PER_OP);
        }
    }

    // INERT: no shipped enemy type points at one (only --suite does).
    for (const EnemyType& t : cfg.enemy_types) CHECK(t.pattern.empty());
}
