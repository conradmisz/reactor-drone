/**
 * test_shop_gear.cpp — gear levels (#11) and the hover -> tooltip seam (Lane C).
 *
 * The price curve and the "only fitted gear levels" rule are pure ShipState
 * arithmetic and are tested against a hand-built catalogue. The tooltip test is
 * deliberately NOT a fake: it loads the shipped GameData.json so the widget
 * lookups, the card ordering and the tooltip labels are exercised through the
 * same "ui.widget_id.<name>" path the game uses.
 */

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/systems/ui_system.hpp"
#include "engine/gamedata_loader.hpp"
#include "engine/project_paths.hpp"
#include "game/enemy_components.hpp"
#include "game/player_components.hpp"
#include "game/shop_system.hpp"

namespace {

ShopConfig make_catalogue() {
    ShopConfig cfg;
    cfg.price_growth = 1.5f;
    cfg.upgrades = {
        ShopUpgradeDef{"Hull Plating",    "hull",   50, 25.0f, 8, 0.0f},
        ShopUpgradeDef{"Heavy Rounds",    "damage", 60,  8.0f, 8, 0.0f},
    };
    cfg.items = {
        ShopUpgradeDef{"Magnet Core",     "magnet",   120,  0.0f, 0, 0.0f},
        ShopUpgradeDef{"Repulsor Field",  "repulsor", 120, 35.0f, 0, 0.0f},
    };
    cfg.consumables = {
        ShopUpgradeDef{"Repair Kit",      "repair",    45, 60.0f, 0, 0.0f},
    };
    return cfg;
}

/// The shipped world plus a player, so ShopSystem has everything it reads.
struct ShopWorld {
    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    Entity player = 0;

    ShopWorld() {
        load_game_data(project_paths::assets_dir() + "/GameData.json", em, cs, bb);
        player = em.create_entity();
        cs.add_component<PlayerTag>(player, PlayerTag{});
        cs.add_component<ShipState>(player, ShipState{});
    }

    ShipState& ship() { return cs.get_component<ShipState>(player)->get(); }

    Entity widget(const std::string& name) const {
        const double v = bb.get_or<double>("ui.widget_id." + name, -1.0);
        return v < 0.0 ? 0 : static_cast<Entity>(v);
    }

    void hover(const std::string& name, bool on) {
        Entity e = widget(name);
        REQUIRE(e != 0);
        cs.get_component<UIState>(e)->get().hovered = on;
    }

    std::string label(const std::string& name) const {
        Entity e = widget(name);
        REQUIRE(e != 0);
        return cs.get_component<UIElement>(e)->get().label_text;
    }
};

}  // namespace

TEST_CASE("Gear level price is base * price_growth^level", "[Game][shop][gear]") {
    ShopConfig cfg = make_catalogue();
    ShopSystem shop;
    shop.set_config(&cfg);

    // 120 * 1.5^0 / ^1 / ^2 -> 120, 180, 270. Same curve price_for() uses, so a
    // levelled item escalates exactly like a stacked upgrade.
    REQUIRE(shop.gear_price(1, 0) == 120);
    REQUIRE(shop.gear_price(1, 1) == 180);
    REQUIRE(shop.gear_price(1, 2) == 270);

    // Out-of-range rows price at 0 rather than reading past the catalogue.
    REQUIRE(shop.gear_price(-1, 0) == 0);
    REQUIRE(shop.gear_price(99, 0) == 0);
}

TEST_CASE("You cannot upgrade gear you do not own", "[Game][shop][gear]") {
    ShopConfig cfg = make_catalogue();
    ShopSystem shop;
    shop.set_config(&cfg);

    EntityManager em;
    ComponentStorage cs;
    Blackboard bb;
    ShipState ship;
    ship.currency = 1000;
    ship.item_id = -1;                     // nothing fitted

    REQUIRE_FALSE(shop.owns_gear(ship, 1));
    REQUIRE_FALSE(shop.upgrade_gear(1, cs, bb, ship));
    REQUIRE(ship.currency == 1000);        // no charge for a refused purchase
    REQUIRE(ship.gear_levels[1] == 0);
    REQUIRE(bb.get_or<std::string>("hud_message", std::string()).find("not fitted")
            != std::string::npos);

    // Fit it, and the same call now succeeds and charges the level-0 price.
    ship.item_id = item_ids::REPULSOR_FIELD;
    REQUIRE(shop.owns_gear(ship, 1));
    REQUIRE(shop.upgrade_gear(1, cs, bb, ship));
    REQUIRE(ship.currency == 1000 - 120);
    REQUIRE(ship.gear_levels[1] == 1);
    // The level lands on the one number every item consumer already reads.
    REQUIRE(bb.get_or<float>("ship.item_amount", 0.0f) > 35.0f);

    // The second level costs the escalated price.
    REQUIRE(shop.upgrade_gear(1, cs, bb, ship));
    REQUIRE(ship.currency == 1000 - 120 - 180);

    // ...and stops when the credits run out.
    ship.currency = 10;
    REQUIRE_FALSE(shop.upgrade_gear(1, cs, bb, ship));
    REQUIRE(ship.currency == 10);
    REQUIRE(ship.gear_levels[1] == 2);
}

TEST_CASE("A boolean item has no level to buy", "[Game][shop][gear]") {
    ShopConfig cfg = make_catalogue();
    ShopSystem shop;
    shop.set_config(&cfg);

    ComponentStorage cs;
    Blackboard bb;
    ShipState ship;
    ship.currency = 1000;
    ship.item_id = item_ids::MAGNET_CORE;   // catalogue amount 0.0

    REQUIRE(shop.owns_gear(ship, 0));
    REQUIRE_FALSE(shop.upgrade_gear(0, cs, bb, ship));
    REQUIRE(ship.currency == 1000);
}

TEST_CASE("The detail pane fills on hover and idles on the page hint",
          "[Game][ui][shop]") {
    ShopConfig cfg = make_catalogue();
    ShopWorld w;
    ShopSystem shop;
    shop.set_config(&cfg);

    shop.open(w.cs, w.em, w.bb);
    REQUIRE(shop.menu_tick(w.cs, w.em, w.bb) == false);   // builds the menu

    // Nothing hovered: the pane holds the page hint rather than collapsing (D89
    // supersedes D64's cursor-following tooltip — it jumped, and it slid through
    // the drone preview it shares the column with).
    REQUIRE(shop.tooltip_name().empty());
    REQUIRE(w.label("shop_tip_name").empty());
    REQUIRE_FALSE(w.label("shop_tip_desc").empty());
    REQUIRE(w.cs.get_component<UIElement>(w.widget("shop_tip_panel"))->get().rect.w > 0.0f);

    // Hover card 1 -> the second UPGRADES row.
    w.hover("shop_card_1", true);
    shop.menu_tick(w.cs, w.em, w.bb);
    REQUIRE(shop.tooltip_name() == "Heavy Rounds");
    REQUIRE(w.label("shop_tip_name") == "Heavy Rounds");
    REQUIRE(shop.tooltip_detail().find("damage") != std::string::npos);

    // The pane sits in the right column, clear of the card panel — never over
    // the row it describes.
    const UIRect tip = w.cs.get_component<UIElement>(w.widget("shop_tip_panel"))->get().rect;
    const UIRect card = w.cs.get_component<UIElement>(w.widget("shop_card_1"))->get().rect;
    REQUIRE(tip.w > 0.0f);
    REQUIRE(tip.x >= card.x + card.w);

    // Move off -> back to the idle hint.
    w.hover("shop_card_1", false);
    shop.menu_tick(w.cs, w.em, w.bb);
    REQUIRE(shop.tooltip_name().empty());
    REQUIRE_FALSE(w.label("shop_tip_desc").empty());
}

TEST_CASE("A card buys on a held press, not a click, and the click key is consumed",
          "[Game][ui][shop]") {
    ShopConfig cfg = make_catalogue();
    ShopWorld w;
    ShopSystem shop;
    shop.set_config(&cfg);
    w.ship().currency = 500;

    shop.open(w.cs, w.em, w.bb);
    shop.menu_tick(w.cs, w.em, w.bb);

    // Card 0 on the UPGRADES page is Hull Plating at 50 cr.
    REQUIRE(shop.card_line(w.cs, 0).find("Hull Plating") != std::string::npos);

    // D189: a bare click SELECTS but never buys — purchasing is press-and-hold.
    w.bb.set<std::string>(UISystem::UI_CLICK_KEY, std::string("on_shop_card_0"));
    shop.menu_tick(w.cs, w.em, w.bb);
    REQUIRE(w.ship().currency == 500);
    REQUIRE(w.ship().upg_counts[0] == 0);
    // UISystem never clears the key; an unconsumed click would re-fire forever.
    REQUIRE(w.bb.get_or<std::string>(UISystem::UI_CLICK_KEY, std::string()).empty());

    // Hold the card (hovered + pressed) for a full second of frames -> it buys.
    w.hover("shop_card_0", true);
    w.cs.get_component<UIState>(w.widget("shop_card_0"))->get().pressed = true;
    w.bb.set<double>("delta_time", 1.0 / 60.0);
    for (int i = 0; i < 70 && w.ship().upg_counts[0] == 0; ++i)
        shop.menu_tick(w.cs, w.em, w.bb);
    REQUIRE(w.ship().currency == 450);
    REQUIRE(w.ship().upg_counts[0] == 1);

    // Releasing resets the hold — half a second of held frames buys nothing.
    w.cs.get_component<UIState>(w.widget("shop_card_0"))->get().pressed = false;
    shop.menu_tick(w.cs, w.em, w.bb);
    w.cs.get_component<UIState>(w.widget("shop_card_0"))->get().pressed = true;
    for (int i = 0; i < 30; ++i) shop.menu_tick(w.cs, w.em, w.bb);
    REQUIRE(w.ship().upg_counts[0] == 1);
    w.cs.get_component<UIState>(w.widget("shop_card_0"))->get().pressed = false;
    w.hover("shop_card_0", false);
    shop.menu_tick(w.cs, w.em, w.bb);

    // Tier 8 (D221/D225): the GEAR/LEVELS pages are retired — a stray page
    // click (the widgets are gone, but the callback comparison survives)
    // stays on the upgrades cards.
    w.bb.set<std::string>(UISystem::UI_CLICK_KEY, std::string("on_shop_page_1"));
    shop.menu_tick(w.cs, w.em, w.bb);
    REQUIRE(shop.card_line(w.cs, 0).find("Hull Plating") != std::string::npos);

    // LAUNCH ends the shop frame.
    w.bb.set<std::string>(UISystem::UI_CLICK_KEY, std::string("on_shop_leave"));
    REQUIRE(shop.menu_tick(w.cs, w.em, w.bb));
}

// --dev: dev_max_upgrades leaves every stacked row at its max_stacks, and the
// stats those rows write actually landed (hull raised Health, damage raised
// WeaponStats) — a maxed counter with an untouched ship would be the bug.
TEST_CASE("dev_max_upgrades maxes every upgrade row", "[shop][dev]") {
    ShopWorld w;
    w.cs.add_component<Health>(w.player, Health{100.0f, 100.0f});
    w.cs.add_component<WeaponStats>(w.player, WeaponStats{});
    ShopConfig cfg = make_catalogue();
    ShopSystem shop;
    shop.set_config(&cfg);

    const float dmg0 = w.cs.get_component<WeaponStats>(w.player)->get().damage;
    shop.dev_max_upgrades(w.player, w.cs, w.bb, w.ship());

    REQUIRE(w.ship().upg_counts[0] == cfg.upgrades[0].max_stacks);
    REQUIRE(w.ship().upg_counts[1] == cfg.upgrades[1].max_stacks);
    REQUIRE(w.cs.get_component<Health>(w.player)->get().max_hp ==
            100.0f + cfg.upgrades[0].amount * static_cast<float>(cfg.upgrades[0].max_stacks));
    REQUIRE(w.cs.get_component<WeaponStats>(w.player)->get().damage ==
            dmg0 + cfg.upgrades[1].amount * static_cast<float>(cfg.upgrades[1].max_stacks));

    // Idempotent: a second call cannot push a row past its cap.
    shop.dev_max_upgrades(w.player, w.cs, w.bb, w.ship());
    REQUIRE(w.ship().upg_counts[0] == cfg.upgrades[0].max_stacks);
}
