/**
 * test_pause_screen.cpp — Lane M (#2, #5, #13, D113/D116/D117).
 *
 * The overlap test is the point of this file. "The pause menu has overlapping
 * text" has now been reported twice; the second time it was not a text-fitting
 * bug at all (fit_text_in_rect keeps every string inside its OWN rect, and did)
 * — it was a widget appended straight on top of two others. Nothing in the build
 * or in any existing test could see that, so this loads the real shipped
 * GameData.json and asserts that no two pause widgets intersect, including the
 * stat-line rects PauseStatsSystem places from code.
 *
 * The rest pins the seams a compiler cannot: the click callbacks main.cpp
 * compares as string literals, and the item-slot widget names PauseStatsSystem
 * resolves through "ui.widget_id.<name>".
 */

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "game/dash_system.hpp"        // dash_sweep_frame (#11)
#include "game/prestige.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/systems/ui_render_math.hpp"
#include "engine/gamedata_loader.hpp"
#include "engine/project_paths.hpp"
#include "engine/ui_style.hpp"
#include "game/pause_stats.hpp"

namespace {

struct LoadedWorld {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;

    LoadedWorld() {
        load_game_data(project_paths::assets_dir() + "/GameData.json", em, cs, bb);
    }

    std::vector<UIElement> widgets(const std::string& screen) const {
        std::vector<UIElement> out;
        for (Entity e : cs.entities_with_component<UIElement>()) {
            auto m = cs.get_component<ScreenMembership>(e);
            if (!m.has_value() || m->get().screen_name != screen) continue;
            out.push_back(cs.get_component<UIElement>(e)->get());
        }
        return out;
    }

    Entity by_name(const std::string& name) const {
        const double v = bb.get_or<double>("ui.widget_id." + name, -1.0);
        return v < 0.0 ? 0 : static_cast<Entity>(v);
    }
};

bool overlaps(const UIRect& a, const UIRect& b) {
    const bool dx = a.x + a.w <= b.x || b.x + b.w <= a.x;
    const bool dy = a.y + a.h <= b.y || b.y + b.h <= a.y;
    return !(dx || dy);
}

}  // namespace

TEST_CASE("no two pause widgets overlap — the #2 regression", "[Game][ui][pause]") {
    LoadedWorld w;
    const std::vector<UIElement> all = w.widgets("pause");
    REQUIRE_FALSE(all.empty());

    // Everything sits ON the backing panel by design, so it is the one rect
    // excluded. Every other pair must be disjoint.
    UIRect panel{};
    std::vector<UIRect> rects;
    for (const UIElement& el : all) {
        if (el.style_id == "panel") { panel = el.rect; continue; }
        rects.push_back(el.rect);
    }
    REQUIRE(panel.w > 0.0f);

    // The stat sheet is placed from code, so it belongs in the same comparison —
    // an authored widget dropped into that band is exactly the bug this catches.
    for (int i = 0; i < pause_stats::MAX_LINES; ++i) rects.push_back(pause_stats::line_rect(i));

    for (std::size_t a = 0; a < rects.size(); ++a) {
        INFO("rect " << a << " at " << rects[a].x << "," << rects[a].y
                     << " " << rects[a].w << "x" << rects[a].h);
        // Inside the panel it is drawn on...
        CHECK(rects[a].x >= panel.x);
        CHECK(rects[a].y >= panel.y);
        CHECK(rects[a].x + rects[a].w <= panel.x + panel.w);
        CHECK(rects[a].y + rects[a].h <= panel.y + panel.h);
        // ...and on the design canvas.
        CHECK(rects[a].x + rects[a].w <= UI_DESIGN_WIDTH);
        CHECK(rects[a].y + rects[a].h <= UI_DESIGN_HEIGHT);
        for (std::size_t b = a + 1; b < rects.size(); ++b) {
            INFO("overlaps rect " << b << " at " << rects[b].x << "," << rects[b].y);
            CHECK_FALSE(overlaps(rects[a], rects[b]));
        }
    }
}

TEST_CASE("the pause buttons carry the callbacks main.cpp compares to",
          "[Game][ui][pause]") {
    LoadedWorld w;
    std::vector<std::string> fns;
    for (const UIElement& el : w.widgets("pause")) {
        if (el.on_click_fn.empty()) continue;
        fns.push_back(el.on_click_fn);
        CHECK(el.element_type == "button");
        CHECK(el.rect.h >= 44.0f);              // D88's minimum hit target
    }
    for (const char* fn : {"on_resume_click", "on_save_run_click", "on_quit_click"}) {
        INFO("missing callback: " << fn);
        CHECK(std::find(fns.begin(), fns.end(), fn) != fns.end());
    }
    // Lane K resolves this one by name to rewrite it to SAVED / SAVE FAILED.
    CHECK(w.by_name("pause_save") != 0);

    // Every pause widget must outrank the always-active gameplay HUD: z_order is
    // sorted GLOBALLY across active screens, so a pause label at 10 is drawn
    // underneath the hull gauge at 12.
    for (const UIElement& el : w.widgets("pause")) CHECK(el.z_order >= 30);
}

TEST_CASE("the active-item slot is authored and layered above its frame",
          "[Game][ui][hud][item-slot]") {
    LoadedWorld w;
    const Entity frame = w.by_name(PauseStatsSystem::SLOT_FRAME);
    const Entity name  = w.by_name(PauseStatsSystem::SLOT_NAME);
    const Entity key   = w.by_name(PauseStatsSystem::SLOT_KEY);
    REQUIRE(frame != 0);
    REQUIRE(name != 0);
    REQUIRE(key != 0);

    const UIElement& f = w.cs.get_component<UIElement>(frame)->get();
    const UIElement& n = w.cs.get_component<UIElement>(name)->get();
    const UIElement& k = w.cs.get_component<UIElement>(key)->get();

    // Bottom-left of the design canvas, and square (#13).
    CHECK(f.rect.x < 200.0f);
    CHECK(f.rect.y < 200.0f);
    CHECK(f.rect.w == f.rect.h);
    // D234 (playtest #8 item 4): the NAME stays inside the square (the icon
    // sprite parks over it); the KEY prompt sits UNDER the box now — the
    // hud_dash_key pattern.
    CHECK(n.rect.x >= f.rect.x);
    CHECK(n.rect.y >= f.rect.y);
    CHECK(n.rect.x + n.rect.w <= f.rect.x + f.rect.w);
    CHECK(n.rect.y + n.rect.h <= f.rect.y + f.rect.h);
    CHECK(n.z_order > f.z_order);            // or the frame paints over the text
    CHECK(k.rect.y + k.rect.h <= f.rect.y);  // fully below the box
    CHECK(k.rect.x >= f.rect.x - 1.0f);      // aligned under it
    CHECK(k.z_order > f.z_order);

    auto table = w.bb.get_or<std::shared_ptr<StyleTable>>("ui_styles", nullptr);
    REQUIRE(table != nullptr);
    CHECK(table->contains(f.style_id));
    CHECK(table->contains(n.style_id));
    CHECK(table->contains(k.style_id));
    CHECK(table->contains("minimap_health"));   // #4's green health blip
}

TEST_CASE("the stat sheet reports the drone the player is actually flying",
          "[Game][ui][pause][stats]") {
    std::vector<ShopUpgradeDef> cat = {
        {"Hull Plating", "hull", 50, 25.0f, 8, 0.0f},
        {"Overclock", "fire_rate", 70, 0.6f, 6, 0.0f},
        {"Aux Thruster", "speed", 70, 0.12f, 5, 0.0f},
    };

    pause_stats::Snapshot s;
    s.hull = 84.0f; s.hull_max = 150.0f;
    s.base_speed = 260.0f; s.speed_mult = 1.24f;
    s.fire_rate = 5.2f; s.damage = 28.0f;
    s.upg_counts[0] = 2;      // Hull Plating x2
    s.upg_counts[2] = 2;      // Aux Thruster x2 — Overclock is skipped entirely

    auto joined = [](const std::vector<std::string>& v) {
        std::string s2;
        for (const std::string& l : v) s2 += l + "\n";
        return s2;
    };

    SECTION("bare drone") {
        const auto lines = pause_stats::stat_lines(s, cat);
        const std::string all = joined(lines);
        CHECK(all.find("84 / 150") != std::string::npos);
        CHECK(all.find("SHIELD     none") != std::string::npos);
        CHECK(all.find("322 px/s") != std::string::npos);   // 260 * 1.24
        CHECK(all.find("(base 260)") != std::string::npos);
        CHECK(all.find("5.2/s") != std::string::npos);
        // D227: DAMAGE is its own row now, so it can carry its own pip meter.
        CHECK(all.find("DAMAGE") != std::string::npos);
        CHECK(all.find("28") != std::string::npos);
        // Only PURCHASED rows, with the cumulative effect, not the per-stack one.
        CHECK(all.find("Hull Plating x2   +50 hull") != std::string::npos);
        CHECK(all.find("Aux Thruster x2   +24% speed") != std::string::npos);
        CHECK(all.find("Overclock") == std::string::npos);
        CHECK(all.find("PRESTIGE") == std::string::npos);   // Lane O has not landed
    }

    SECTION("nothing bought says so, rather than showing a bare header") {
        pause_stats::Snapshot bare;
        const std::string all = joined(pause_stats::stat_lines(bare, cat));
        CHECK(all.find("none purchased") != std::string::npos);
    }

    SECTION("the GEAR row is gone, even with equipment fitted") {
        // Playtest #1 item 8 (D227): the sheet no longer summarises gear — the
        // gear economy it described was retired from the shop in D225. The HUD
        // item slot still shows the active; this sheet does not.
        s.item_id = 0; s.item_name = "Magnet Core";
        s.consumable_id = 0; s.consumable_name = "Repair Kit";
        s.active_id = 1; s.active_name = "Laser Cannon";
        s.shield = 12.0f; s.shield_max = 30.0f;
        const std::string all = joined(pause_stats::stat_lines(s, cat));
        CHECK(all.find("12 / 30") != std::string::npos);
        CHECK(all.find("GEAR") == std::string::npos);
        CHECK(all.find("Magnet Core") == std::string::npos);
        CHECK(all.find("[Q] Repair Kit") == std::string::npos);
    }

    SECTION("the worst case still fits the pool") {
        pause_stats::Snapshot full;
        std::vector<ShopUpgradeDef> eight;
        for (int i = 0; i < 8; ++i) {
            eight.push_back({"Row" + std::to_string(i), "damage", 10, 1.0f, 8, 0.0f});
            full.upg_counts[i] = 3;
        }
        full.prestige = 2;
        full.item_id = 0; full.consumable_id = 0; full.active_id = 0;
        const auto lines = pause_stats::stat_lines(full, eight);
        CHECK(lines.size() == static_cast<std::size_t>(pause_stats::MAX_LINES));
        CHECK(lines.front().rfind("PRESTIGE", 0) == 0);
        // D227: the pip column is parallel — one entry per line, or the meters
        // drift off their rows.
        CHECK(pause_stats::stat_pips(full, eight).size() == lines.size());
    }
}

TEST_CASE("the pip meter rounds, not truncates, at the stack boundary",
          "[Game][ui][pause][stats]") {
    // D188: HULL's cap is 8, so 3 stacks is 15/8 = 1.875 pips — round says 2
    // filled, floor would say 1. The one input at this cap where they disagree.
    pause_stats::Snapshot s;
    s.upg_counts[0] = 3;
    // D227: pips live in their own parallel column now, not inside the text.
    const auto meters = pause_stats::stat_pips(s, {});
    REQUIRE_FALSE(meters.empty());
    CHECK(meters.front() == "●●○○○");
}

TEST_CASE("the repulsion device is not advertised on a key it does not have",
          "[Game][ui][item-slot]") {
    CHECK(pause_stats::active_key(0) == "[E]");     // missiles
    CHECK(pause_stats::active_key(1) == "[E]");     // laser
    CHECK(pause_stats::active_key(2) == "AUTO");    // repulsion device (D71)
    CHECK(pause_stats::active_tag(0) == "MISSILES");
    CHECK(pause_stats::active_tag(-1).empty());
}

// Integration seam (D131): Lane O owns the prestige state, Lane M owns the
// screen. They shipped disagreeing — M read `int` from "meta.prestige_level",
// O wrote `double` to "prestige.level", so the row silently never appeared.
// Neither lane's own tests could see it. This one goes through both sides.
TEST_CASE("the pause sheet reads the prestige level Lane O actually publishes",
          "[Game][ui][prestige]") {
    Blackboard bb;
    bb.set<double>(PRESTIGE_LEVEL_KEY, 3.0);   // exactly what start_run writes

    const int level = static_cast<int>(bb.get_or<double>(PRESTIGE_LEVEL_KEY, 0.0));
    CHECK(level == 3);

    pause_stats::Snapshot s;
    s.prestige = level;
    const auto lines = pause_stats::stat_lines(s, {});
    REQUIRE_FALSE(lines.empty());
    CHECK(lines.front().rfind("PRESTIGE 3", 0) == 0);
    CHECK(lines.front().find("HULL") != std::string::npos);   // the bonuses, not just the level

    pause_stats::Snapshot none;                                // no prestige -> no row
    CHECK(pause_stats::stat_lines(none, {}).front().rfind("PRESTIGE", 0) != 0);
}

// ---------------------------------------------------------------------------
// D192 — the gameplay screen's new gauges (#8 boss bar, #9 battery). These are
// resolved by NAME through "ui.widget_id.<name>", so a rename in GameData.json
// makes them silently never draw; that is exactly the failure this pins.
// ---------------------------------------------------------------------------
TEST_CASE("the D192 HUD gauges exist, resolve a style and stay on-canvas",
          "[Game][ui][d192]") {
    LoadedWorld w;
    auto table = w.bb.get_or<std::shared_ptr<StyleTable>>("ui_styles", nullptr);
    REQUIRE(table != nullptr);

    for (const char* name : {"hud_bat_bg", "hud_bat_fill",
                             "hud_boss_bg", "hud_boss_fill", "hud_boss_label",
                             // The ability row: the dash button and the boss-item
                             // slot beside it, resolved by the same name lookup.
                             "hud_dash_frame", "hud_dash_key",
                             "item_slot_frame", "item_slot_name", "item_slot_key"}) {
        INFO("gameplay widget: " << name);
        const Entity e = w.by_name(name);
        REQUIRE(e != 0);
        auto el = w.cs.get_component<UIElement>(e);
        REQUIRE(el.has_value());
        REQUIRE(table->contains(el->get().style_id));
        const UIRect& r = el->get().rect;
        REQUIRE(r.x >= 0.0f);
        REQUIRE(r.y >= 0.0f);
        REQUIRE(r.x + r.w <= UI_DESIGN_WIDTH);
        REQUIRE(r.y + r.h <= UI_DESIGN_HEIGHT);
    }

    // GameHUDSystem authors the battery fill at the same BAR_FULL_W = 240 it
    // restores the hull and shield bars to; a mismatch here is a bar that jumps
    // wider than its frame on the first refresh.
    auto bat = w.cs.get_component<UIElement>(w.by_name("hud_bat_fill"));
    REQUIRE(bat.has_value());
    CHECK(bat->get().rect.w == 240.0f);

    // The boss fill must sit inside its own frame, not over it.
    auto bg = w.cs.get_component<UIElement>(w.by_name("hud_boss_bg"));
    auto fill = w.cs.get_component<UIElement>(w.by_name("hud_boss_fill"));
    REQUIRE(bg.has_value());
    REQUIRE(fill.has_value());
    CHECK(fill->get().rect.w == bg->get().rect.w - 4.0f);
    CHECK(fill->get().z_order > bg->get().z_order);

    // The ability row (playtest #2/#11/#12): the boss-item slot and the dash
    // button are one row, so the two boxes must be the same size and share a
    // baseline — that sameness IS the feedback item. The dash button's face is a
    // pair of SPRITES parked over hud_dash_frame's live rect, so the box must
    // stay square: a non-square rect would stretch the booster and turn the
    // circular cooldown dial into an ellipse.
    auto slot = w.cs.get_component<UIElement>(w.by_name("item_slot_frame"));
    auto dash = w.cs.get_component<UIElement>(w.by_name("hud_dash_frame"));
    REQUIRE(slot.has_value());
    REQUIRE(dash.has_value());
    CHECK(slot->get().rect.w == dash->get().rect.w);
    CHECK(slot->get().rect.h == dash->get().rect.h);
    CHECK(slot->get().rect.y == dash->get().rect.y);
    CHECK(slot->get().rect.x + slot->get().rect.w <= dash->get().rect.x);  // no overlap
    CHECK(dash->get().rect.w == dash->get().rect.h);
}

// ---------------------------------------------------------------------------
// Playtest #11 — the dash button's circular cooldown dial. The sweep atlas is
// authored at DASH_SWEEP_FRAMES even progress steps in make_sprites.py, and
// nothing but this pins the picker to that count: an off-by-one reads a frame
// past the end of the strip, which SDL happily draws as blank.
// ---------------------------------------------------------------------------
TEST_CASE("dash_sweep_frame stays in the strip and rises with the recharge",
          "[Game][ui][dash]") {
    CHECK(dash_sweep_frame(0.0f) == 0);                          // just dashed: all grey
    CHECK(dash_sweep_frame(1.0f) == DASH_SWEEP_FRAMES - 1);      // ready (parked anyway)
    CHECK(dash_sweep_frame(-5.0f) == 0);                         // total on garbage
    CHECK(dash_sweep_frame(9.0f) == DASH_SWEEP_FRAMES - 1);
    CHECK(dash_sweep_frame(0.5f) == DASH_SWEEP_FRAMES / 2);      // half way round

    int prev = -1;
    for (int i = 0; i <= 100; ++i) {
        const int f = dash_sweep_frame(static_cast<float>(i) / 100.0f);
        INFO("frac " << i << "%");
        REQUIRE(f >= 0);
        REQUIRE(f < DASH_SWEEP_FRAMES);
        REQUIRE(f >= prev);            // monotone: the dial never runs backwards
        prev = f;
    }
}
