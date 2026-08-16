/**
 * test_cosmetics.cpp — paints (gameplay pack v2.3 tier 7, D221): ownership
 * derivation, per-item slot resolution, catalogue integrity, and the two
 * overlay screens' contract (the intermission-test pattern).
 */
#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <string>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/gamedata_loader.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"
#include "game/meta_save.hpp"

namespace {
GameConfig cfg() { return load_arena_config(project_paths::assets_dir() + "/GameData.json"); }
}

TEST_CASE("paint ownership derives from ships; shop paints from purchases", "[cosmetics]") {
    const GameConfig c = cfg();
    MetaSave m;
    const int cyan = find_color(c.cosmetic_colors, "Cyan");
    const int violet = find_color(c.cosmetic_colors, "Violet");
    const int gold = find_color(c.cosmetic_colors, "Gold");
    REQUIRE(cyan >= 0); REQUIRE(violet >= 0); REQUIRE(gold >= 0);
    CHECK(color_owned(m, c.ships, c.cosmetic_colors[cyan]));        // Falcon's, free
    CHECK_FALSE(color_owned(m, c.ships, c.cosmetic_colors[violet])); // Owl not bought
    CHECK_FALSE(color_owned(m, c.ships, c.cosmetic_colors[gold]));   // not purchased
    m.owned_ships.push_back("Owl");
    m.owned_cosmetics.push_back("Gold");
    CHECK(color_owned(m, c.ships, c.cosmetic_colors[violet]));
    CHECK(color_owned(m, c.ships, c.cosmetic_colors[gold]));
}

TEST_CASE("an equipped slot resolves only while the paint stays owned", "[cosmetics]") {
    const GameConfig c = cfg();
    MetaSave m;
    m.ship_colors["Falcon"] = "Gold";
    // Equipped but NOT owned: falls back to the item's own paint.
    CHECK(equipped_color(m, m.ship_colors, c.ships, c.cosmetic_colors, "Falcon") < 0);
    m.owned_cosmetics.push_back("Gold");
    const int i = equipped_color(m, m.ship_colors, c.ships, c.cosmetic_colors, "Falcon");
    REQUIRE(i >= 0);
    CHECK(c.cosmetic_colors[static_cast<size_t>(i)].name == "Gold");
    // Unknown item / empty slot: default.
    CHECK(equipped_color(m, m.ship_colors, c.ships, c.cosmetic_colors, "Owl") < 0);
}

TEST_CASE("the paint catalogue is coherent and its atlases exist", "[cosmetics][data]") {
    const GameConfig c = cfg();
    REQUIRE(c.cosmetic_colors.size() >= 6);
    int granted = 0, purchasable = 0;
    for (const CosmeticColorDef& col : c.cosmetic_colors) {
        INFO("paint: " << col.name);
        REQUIRE_FALSE(col.sidecar.empty());
        std::ifstream f(project_paths::assets_dir() + "/" + col.sidecar);
        CHECK(f.is_open());   // a paint with no atlas would strip the drone bare
        if (col.granted_by.empty()) {
            ++purchasable;
            CHECK(col.price > 0);
        } else {
            ++granted;
            bool ship_exists = false;
            for (const ShipDef& s : c.ships) ship_exists |= (s.name == col.granted_by);
            CHECK(ship_exists);
        }
    }
    CHECK(granted >= 3);
    CHECK(purchasable >= 3);
}

TEST_CASE("the cosmetic shop and inventory screens route their clicks", "[cosmetics][ui]") {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    load_game_data(project_paths::assets_dir() + "/GameData.json", em, cs, bb);
    auto by_name = [&](const std::string& n) -> Entity {
        const double v = bb.get_or<double>("ui.widget_id." + n, -1.0);
        return v < 0.0 ? 0 : static_cast<Entity>(v);
    };
    auto fn = [&](Entity e) { return cs.get_component<UIElement>(e)->get().on_click_fn; };
    for (const char* screen : {"cosmetic_shop", "inventory"}) {
        bool found = false;
        for (Entity e : cs.entities_with_component<UIScreen>()) {
            auto sc = cs.get_component<UIScreen>(e);
            if (!sc.has_value() || sc->get().screen_name != screen) continue;
            found = true;
            REQUIRE_FALSE(sc->get().active);   // must not cover the title at boot
        }
        INFO("screen: " << screen);
        REQUIRE(found);
    }
    for (int i = 0; i < 6; ++i) {
        Entity e = by_name("cs_row_" + std::to_string(i));
        REQUIRE(e != 0);
        CHECK(fn(e) == "on_cosmetic_buy_" + std::to_string(i));
    }
    CHECK(fn(by_name("inv_ship_color")) == "on_inv_ship_color");
    CHECK(fn(by_name("inv_trail_color")) == "on_inv_trail_color");
    CHECK(fn(by_name("inv_proj_color")) == "on_inv_proj_color");
    CHECK(fn(by_name("cs_back")) == "on_overlay_back");
    CHECK(fn(by_name("inv_back")) == "on_overlay_back");
    // The hangar doors into both overlays.
    CHECK(fn(by_name("menu_cosmetics")) == "on_open_cosmetics");
    CHECK(fn(by_name("menu_inventory")) == "on_open_inventory");
}
