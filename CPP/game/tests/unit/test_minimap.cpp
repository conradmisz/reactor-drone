/**
 * test_minimap.cpp — the arena minimap (#7, D58).
 *
 * Two halves. The mapping is pure maths and is pinned exhaustively here, because
 * a minimap that is subtly wrong is worse than none — a player trusts it. The
 * system half is pinned on the two things that would only show up under load: the
 * pool never growing past its cap, and never being rebuilt frame to frame.
 *
 * This file also takes over the "minimap is inert" assertion test_scaffolding.cpp
 * used to make (D51: the landing lane deletes its line there).
 */
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/systems/ui_render_math.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/enemy_components.hpp"
#include "game/minimap_math.hpp"
#include "game/minimap_system.hpp"
#include "game/player_components.hpp"

using Catch::Matchers::WithinAbs;
using minimap_math::Rect;

namespace {

constexpr float CX = 480.0f, CY = 330.0f, R = 320.0f;
const Rect FRAME{640.0f, 410.0f, 140.0f, 140.0f};
constexpr float BLIP = 5.0f;

ArenaConfig test_arena() {
    ArenaConfig a;
    a.center_x = CX; a.center_y = CY; a.radius = R;
    return a;
}

MinimapConfig test_minimap(int cap) {
    MinimapConfig m;
    m.enabled = true;
    m.x = FRAME.x; m.y = FRAME.y; m.size = FRAME.w;
    m.max_blips = cap;
    return m;
}

/// Widgets whose rect is non-degenerate — i.e. actually drawn this frame.
int visible_blips(const ComponentStorage& cs) {
    int n = 0;
    for (Entity e : cs.entities_with_component<UIElement>()) {
        const auto& el = cs.get_component<UIElement>(e)->get();
        if (el.style_id.rfind("minimap_", 0) != 0) continue;
        if (el.rect.w > 0.0f && el.rect.h > 0.0f) ++n;
    }
    return n;
}

int pool_widgets(const ComponentStorage& cs) {
    int n = 0;
    for (Entity e : cs.entities_with_component<ScreenMembership>()) {
        if (cs.get_component<ScreenMembership>(e)->get().screen_name ==
            MinimapSystem::SCREEN_NAME) ++n;
    }
    return n;
}

int blips_with_style(const ComponentStorage& cs, const char* style) {
    int n = 0;
    for (Entity e : cs.entities_with_component<UIElement>()) {
        const auto& el = cs.get_component<UIElement>(e)->get();
        if (el.style_id == style && el.rect.w > 0.0f) ++n;
    }
    return n;
}

Entity spawn_at(ComponentStorage& cs, EntityManager& em, float x, float y) {
    Entity e = em.create_entity();
    cs.add_component<Position>(e, Position{x - 10.0f, y - 10.0f});
    cs.add_component<Size>(e, Size{20.0f, 20.0f});
    return e;
}

}  // namespace

// --------------------------------------------------------------------------
// The mapping
// --------------------------------------------------------------------------

TEST_CASE("the arena centre maps to the frame centre", "[Game][minimap][math]") {
    const Rect r = minimap_math::blip_rect(CX, CY, CX, CY, R, FRAME, BLIP);
    CHECK_THAT(r.x + r.w * 0.5f, WithinAbs(FRAME.x + FRAME.w * 0.5f, 1e-4f));
    CHECK_THAT(r.y + r.h * 0.5f, WithinAbs(FRAME.y + FRAME.h * 0.5f, 1e-4f));
    CHECK_THAT(r.w, WithinAbs(BLIP, 1e-6f));
}

TEST_CASE("the mapping preserves direction and is linear inside the arena",
          "[Game][minimap][math]") {
    // Half-way to the east wall lands half-way to the frame's east edge.
    const Rect r = minimap_math::blip_rect(CX + R * 0.5f, CY, CX, CY, R, FRAME, BLIP);
    const float ex = FRAME.w * 0.5f - BLIP * 0.5f;
    CHECK_THAT(r.x + r.w * 0.5f,
               WithinAbs(FRAME.x + FRAME.w * 0.5f + 0.5f * ex, 1e-4f));
    CHECK_THAT(r.y + r.h * 0.5f, WithinAbs(FRAME.y + FRAME.h * 0.5f, 1e-4f));

    // North is up: world +Y maps to design-canvas +Y (both bottom-left origin).
    const Rect n = minimap_math::blip_rect(CX, CY + R * 0.5f, CX, CY, R, FRAME, BLIP);
    CHECK(n.y + n.h * 0.5f > FRAME.y + FRAME.h * 0.5f);
}

TEST_CASE("a point outside the arena clamps to the rim, keeping its bearing",
          "[Game][minimap][math]") {
    // Far to the north-east: must land on the 45-degree rim, not in the corner.
    const Rect r = minimap_math::blip_rect(CX + 9000.0f, CY + 9000.0f,
                                           CX, CY, R, FRAME, BLIP);
    const float dx = r.x + r.w * 0.5f - (FRAME.x + FRAME.w * 0.5f);
    const float dy = r.y + r.h * 0.5f - (FRAME.y + FRAME.h * 0.5f);
    const float ex = FRAME.w * 0.5f - BLIP * 0.5f;
    CHECK_THAT(dx, WithinAbs(dy, 1e-3f));                       // bearing kept
    CHECK_THAT(std::sqrt(dx * dx + dy * dy), WithinAbs(ex, 1e-3f));  // on the rim
    CHECK(minimap_math::contains(FRAME, r));

    // A clamped point is reported as such, an interior one is not.
    CHECK(minimap_math::arena_unit_offset(CX + R * 2.0f, CY, CX, CY, R).clamped);
    CHECK_FALSE(minimap_math::arena_unit_offset(CX, CY, CX, CY, R).clamped);
}

TEST_CASE("no blip is ever drawn outside the frame", "[Game][minimap][math]") {
    for (int i = 0; i < 720; ++i) {
        const float a = static_cast<float>(i) * 0.5f;
        for (float scale : {0.0f, 0.3f, 1.0f, 1.0001f, 4.0f, 1000.0f}) {
            const Rect r = minimap_math::blip_rect(
                CX + std::cos(a) * R * scale, CY + std::sin(a) * R * scale,
                CX, CY, R, FRAME, BLIP);
            INFO("angle " << a << " scale " << scale);
            CHECK(minimap_math::contains(FRAME, r));
        }
    }
}

TEST_CASE("degenerate inputs collapse to the centre rather than dividing by zero",
          "[Game][minimap][math]") {
    const Rect zero_radius = minimap_math::blip_rect(9999.0f, 9999.0f, CX, CY, 0.0f,
                                                     FRAME, BLIP);
    CHECK_THAT(zero_radius.x + zero_radius.w * 0.5f,
               WithinAbs(FRAME.x + FRAME.w * 0.5f, 1e-4f));

    // A blip wider than the frame degenerates to the frame centre.
    const Rect huge = minimap_math::blip_rect(CX + R, CY, CX, CY, R, FRAME, 500.0f);
    CHECK_THAT(huge.x + huge.w * 0.5f, WithinAbs(FRAME.x + FRAME.w * 0.5f, 1e-4f));
}

// --------------------------------------------------------------------------
// The pool
// --------------------------------------------------------------------------

TEST_CASE("the blip pool is allocated once and never exceeds the cap",
          "[Game][minimap][pool]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    bb.set<int>("phase", 1);   // Lane H: the radar only draws in a playing phase
    MinimapSystem mm;
    mm.set_config(test_minimap(8), test_arena());

    for (int i = 0; i < 40; ++i) {
        Entity e = spawn_at(cs, em, CX + static_cast<float>(i), CY);
        cs.add_component<EnemyTag>(e, EnemyTag{});
    }

    mm.update(cs, em, bb);
    CHECK(mm.pool_size() == 8);
    CHECK(pool_widgets(cs) == 8);
    CHECK(visible_blips(cs) == 8);
    CHECK(mm.peak_requested() == 40);

    const int after_first = pool_widgets(cs);
    for (int f = 0; f < 30; ++f) mm.update(cs, em, bb);
    CHECK(pool_widgets(cs) == after_first);   // no per-frame create/destroy churn
    CHECK(visible_blips(cs) == 8);
}

TEST_CASE("the player and the boss survive the cap; the swarm's tail is dropped",
          "[Game][minimap][pool]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    bb.set<int>("phase", 1);   // Lane H: the radar only draws in a playing phase
    MinimapSystem mm;
    mm.set_config(test_minimap(3), test_arena());

    Entity p = spawn_at(cs, em, CX, CY);
    cs.add_component<PlayerTag>(p, PlayerTag{});
    for (int i = 0; i < 20; ++i) {
        Entity e = spawn_at(cs, em, CX + 20.0f + static_cast<float>(i), CY);
        cs.add_component<EnemyTag>(e, EnemyTag{});
    }
    Entity boss = spawn_at(cs, em, CX - 100.0f, CY);
    cs.add_component<EnemyTag>(boss, EnemyTag{});
    cs.add_component<EnemyBehavior>(boss, EnemyBehavior{behavior_kinds::BOSS, 1, 0, 0, 0});

    mm.update(cs, em, bb);
    CHECK(visible_blips(cs) == 3);
    CHECK(blips_with_style(cs, MinimapSystem::STYLE_PLAYER) == 1);
    // Player + boss + one of the swarm.
    CHECK(blips_with_style(cs, MinimapSystem::STYLE_ENEMY) == 2);
}

TEST_CASE("blips colour-code by what they are", "[Game][minimap][pool]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    bb.set<int>("phase", 1);   // Lane H: the radar only draws in a playing phase
    MinimapSystem mm;
    mm.set_config(test_minimap(64), test_arena());

    Entity p = spawn_at(cs, em, CX, CY);
    cs.add_component<PlayerTag>(p, PlayerTag{});
    for (int i = 0; i < 4; ++i) {
        Entity e = spawn_at(cs, em, CX + 30.0f * static_cast<float>(i + 1), CY);
        cs.add_component<EnemyTag>(e, EnemyTag{});
    }
    for (int i = 0; i < 3; ++i) {
        Entity e = spawn_at(cs, em, CX, CY + 30.0f * static_cast<float>(i + 1));
        cs.add_component<Pickup>(e, Pickup{});
    }
    // Lane M (#4, D114): health packs are the one pickup worth crossing the arena
    // for, so they are green rather than loot-amber.
    for (int i = 0; i < 2; ++i) {
        Entity e = spawn_at(cs, em, CX - 30.0f * static_cast<float>(i + 1), CY);
        Pickup pk;
        pk.kind = static_cast<int>(PickupKind::Health);
        cs.add_component<Pickup>(e, pk);
    }

    mm.update(cs, em, bb);
    CHECK(blips_with_style(cs, MinimapSystem::STYLE_PLAYER) == 1);
    CHECK(blips_with_style(cs, MinimapSystem::STYLE_ENEMY) == 4);
    CHECK(blips_with_style(cs, MinimapSystem::STYLE_PICKUP) == 3);
    CHECK(blips_with_style(cs, MinimapSystem::STYLE_HEALTH) == 2);
    CHECK(visible_blips(cs) == 10);

    // Fewer things next frame parks the surplus rather than destroying it.
    for (Entity e : cs.entities_with_component<EnemyTag>()) {
        cs.add_component<DestroyRequest>(e, DestroyRequest{});
    }
    mm.update(cs, em, bb);
    CHECK(visible_blips(cs) == 6);   // player + 3 loot + 2 health packs
    CHECK(pool_widgets(cs) == 64);
}

TEST_CASE("a disabled minimap allocates nothing at all", "[Game][minimap][pool]") {
    ComponentStorage cs;
    EntityManager em;
    Blackboard bb;
    bb.set<int>("phase", 1);   // Lane H: the radar only draws in a playing phase
    MinimapSystem mm;
    MinimapConfig cfg = test_minimap(64);
    cfg.enabled = false;
    mm.set_config(cfg, test_arena());
    for (int i = 0; i < 10; ++i) mm.update(cs, em, bb);
    CHECK(mm.pool_size() == 0);
    CHECK(pool_widgets(cs) == 0);
}

TEST_CASE("the shipped GameData turns the minimap on and authors its frame",
          "[Game][minimap][config]") {
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    CHECK(cfg.minimap.enabled);
    CHECK(cfg.minimap.max_blips == 120);
    CHECK(cfg.minimap.size > 0.0f);
    // Lane M (#4, D115): the map's top and right margins must be EQUAL, and the
    // edge the player sees is the window's, not the design canvas's. The canvas
    // is letterboxed 50px in from each side of the 980x660 window, so an x that
    // fits inside 800 is 68px from the right edge while y=486 is 20px from the
    // top. The rect therefore runs past 800 on purpose; what is pinned here is
    // the margin equality after ui_canvas_transform.
    const UICanvasTransform xf = ui_canvas_transform(980.0f, 660.0f);
    const UIRect map = ui_apply_transform(
        xf, UIRect{cfg.minimap.x, cfg.minimap.y, cfg.minimap.size, cfg.minimap.size});
    const float right_margin = 980.0f - (map.x + map.w);
    const float top_margin   = 660.0f - (map.y + map.h);
    CHECK_THAT(right_margin, WithinAbs(top_margin, 1.0f));
    CHECK(top_margin > 0.0f);
    CHECK(map.x > 0.0f);
}
