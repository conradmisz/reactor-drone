#include "shop_system.hpp"
#include "upgrade_visuals.hpp"   // Lane N (D123): the drone's upgrade look
#include "enemy_components.hpp"    // Health
#include "item_system.hpp"         // items::item_id_for / consumable_id_for
#include "engine/ecs/systems/screen_stack_system.hpp"
#include "engine/ecs/systems/ui_render_math.hpp"   // ui_canvas_transform
#include "engine/ecs/systems/ui_system.hpp"        // UI_CLICK_KEY
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

/// Player entity, or false if there is none (game-over frame).
bool find_player(ComponentStorage& storage, Entity& out) {
    for (Entity p : storage.entities_with_component<PlayerTag>()) { out = p; return true; }
    return false;
}

std::string num(float v) {
    // Whole numbers read as "25", not "25.0"; fractions keep two decimals.
    if (std::fabs(v - std::round(v)) < 0.005f) return std::to_string(static_cast<int>(std::round(v)));
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
    return std::string(buf);
}

}  // namespace

int ShopSystem::price_for(int index, int already_bought) const {
    if (!cfg_ || index < 0 || index >= static_cast<int>(cfg_->upgrades.size())) return 0;
    float p = static_cast<float>(cfg_->upgrades[static_cast<size_t>(index)].price);
    for (int i = 0; i < already_bought; ++i) p *= cfg_->price_growth;
    return static_cast<int>(std::round(p));
}

void ShopSystem::open(ComponentStorage& storage, EntityManager& entity_manager,
                      const Blackboard& blackboard) {
    if (open_ || cfg_ == nullptr) return;
    const float win_w = static_cast<float>(blackboard.get_or<int>("window_width", 980));
    const float win_h = static_cast<float>(blackboard.get_or<int>("window_height", 660));

    const SDL_Color white  = {235, 235, 245, 255};
    const SDL_Color yellow = {255, 220, 90, 255};
    const SDL_Color cyan   = {120, 225, 255, 255};

    auto make = [&](float x, float y, float size, SDL_Color color) {
        Entity e = entity_manager.create_entity();
        storage.add_component<ScreenPosition>(e, ScreenPosition{x, y});
        storage.add_component<Text>(e, Text{std::string(), "default.ttf", size, color});
        rows_.push_back(e);
        return e;
    };

    // ponytail: fixed pixel layout, same as GameHUDSystem's hardcoded offsets.
    // Measured text centring lands with the clickable cards in Phase 6.
    const float x = win_w * 0.5f - 300.0f;
    float y = win_h - 110.0f;
    make(x, y, 34.0f, cyan);            // title
    y -= 40.0f;
    make(x, y, 26.0f, yellow);          // credits/keys
    y -= 30.0f;
    make(x, y, 22.0f, white);           // equipped slots
    y -= 40.0f;
    // Enough rows for the *widest* page, so TAB only rewrites text — no
    // destroy/respawn churn, and unused rows just render empty.
    for (size_t i = 0; i < page_rows(); ++i) {
        make(x, y, 24.0f, white);
        y -= 32.0f;
    }
    y -= 14.0f;
    make(x, y, 22.0f, cyan);            // footer

    page_ = 0;
    open_ = true;
}

size_t ShopSystem::page_rows() const {
    if (cfg_ == nullptr) return 0;
    return std::max(cfg_->upgrades.size(), cfg_->items.size() + cfg_->consumables.size());
}

void ShopSystem::close(ComponentStorage& storage) {
    for (Entity e : rows_) storage.add_component<DestroyRequest>(e, DestroyRequest{});
    rows_.clear();
    open_ = false;
}

void ShopSystem::refresh_rows(ComponentStorage& storage, const ShipState& ship) {
    if (rows_.size() < page_rows() + 4) return;

    auto set_text = [&](size_t row, const std::string& s) {
        if (auto t = storage.get_component<Text>(rows_[row]); t.has_value()) t->get().content = s;
    };
    auto blank_from = [&](size_t first) {
        for (size_t i = first; i < page_rows(); ++i) set_text(i + 3, std::string());
    };

    set_text(0, page_ == 0 ? "REACTOR SHOP - UPGRADES" : "REACTOR SHOP - GEAR");
    set_text(1, "Credits: " + std::to_string(ship.currency) +
                 "    Keys: " + std::to_string(ship.keys));
    auto slot_name = [&](const std::vector<ShopUpgradeDef>& rows, int id, bool is_item) {
        if (id < 0) return std::string("-");
        for (const auto& d : rows) {
            const int rid = is_item ? items::item_id_for(d.effect)
                                    : items::consumable_id_for(d.effect);
            if (rid == id) return d.name;
        }
        return std::string("-");
    };
    set_text(2, "Item: " + slot_name(cfg_->items, ship.item_id, true) +
                 "    Consumable: " + slot_name(cfg_->consumables, ship.consumable_id, false));

    if (page_ == 0) {
        for (size_t i = 0; i < cfg_->upgrades.size(); ++i) {
            const ShopUpgradeDef& d = cfg_->upgrades[i];
            const int owned = ship.upg_counts[i];
            const bool maxed = d.max_stacks > 0 && owned >= d.max_stacks;
            std::string line = "[" + std::to_string(i + 1) + "] " + d.name +
                               "  +" + num(d.amount);
            if (owned > 0) line += "  (x" + std::to_string(owned) + ")";
            line += maxed ? "   MAX"
                          : "   " + std::to_string(price_for(static_cast<int>(i), owned)) + " cr";
            set_text(i + 3, line);
        }
        blank_from(cfg_->upgrades.size());
        set_text(page_rows() + 3, "1-" + std::to_string(cfg_->upgrades.size()) +
                                  " buy    TAB gear    B launch");
    } else {
        const size_t n_items = cfg_->items.size();
        for (size_t i = 0; i < n_items + cfg_->consumables.size(); ++i) {
            const bool is_item = i < n_items;
            const ShopUpgradeDef& d = is_item ? cfg_->items[i] : cfg_->consumables[i - n_items];
            const int id = is_item ? items::item_id_for(d.effect)
                                   : items::consumable_id_for(d.effect);
            const bool held = is_item ? (ship.item_id == id) : (ship.consumable_id == id);
            set_text(i + 3, "[" + std::to_string(i + 1) + "] " +
                            (is_item ? "ITEM " : "USE  ") + d.name +
                            (held ? "   EQUIPPED" : "   " + std::to_string(d.price) + " cr"));
        }
        blank_from(n_items + cfg_->consumables.size());
        set_text(page_rows() + 3,
                 "1-" + std::to_string(n_items + cfg_->consumables.size()) +
                 " equip (replaces the slot)    TAB upgrades    B launch");
    }
}

bool ShopSystem::update(ComponentStorage& storage, Blackboard& blackboard,
                        int digit, bool leave, bool toggle_page) {
    if (!open_ || cfg_ == nullptr) return true;

    Entity player = 0;
    if (!find_player(storage, player)) return true;
    auto ship_opt = storage.get_component<ShipState>(player);
    if (!ship_opt.has_value()) return true;
    ShipState& ship = ship_opt->get();

    // A page flip consumes the frame: TAB and a digit can't both land anyway,
    // and this keeps "the row you saw is the row you bought" true. TAB still
    // walks only the two original pages; the LEVELS page is menu/keyboard-3.
    if (toggle_page) {
        page_ = (page_ == 0) ? 1 : 0;
    } else if (page_ == 1) {
        buy_gear(digit - 1, player, storage, blackboard, ship);
    } else if (page_ == 2) {
        if (digit > 0) upgrade_gear(digit - 1, storage, blackboard, ship);
    } else {
        buy_upgrade(digit - 1, player, storage, blackboard, ship);
    }

    refresh_rows(storage, ship);
    return leave;
}

void ShopSystem::buy_upgrade(int index, Entity player, ComponentStorage& storage,
                             Blackboard& blackboard, ShipState& ship) {
    if (index < 0 || index >= static_cast<int>(cfg_->upgrades.size())) return;
    const ShopUpgradeDef& d = cfg_->upgrades[static_cast<size_t>(index)];
    const int owned = ship.upg_counts[index];
    const int cost = price_for(index, owned);
    if (d.max_stacks > 0 && owned >= d.max_stacks) {
        blackboard.set<std::string>("hud_message", d.name + " is maxed out");
    } else if (ship.currency < cost) {
        blackboard.set<std::string>("hud_message",
            "Not enough credits (" + std::to_string(cost) + ")");
    } else {
        ship.currency -= cost;
        ship.upg_counts[index] = owned + 1;
        apply(d, player, storage, blackboard);
        blackboard.set<std::string>("hud_message", d.name + " installed");
    }
    blackboard.set<float>("hud_message_timer", 2.0f);
}

void ShopSystem::buy_gear(int index, Entity player, ComponentStorage& storage,
                          Blackboard& blackboard, ShipState& ship) {
    const int n_items = static_cast<int>(cfg_->items.size());
    const int total = n_items + static_cast<int>(cfg_->consumables.size());
    if (index < 0 || index >= total) return;

    const bool is_item = index < n_items;
    const ShopUpgradeDef& d = is_item ? cfg_->items[static_cast<size_t>(index)]
                                      : cfg_->consumables[static_cast<size_t>(index - n_items)];
    const int id = is_item ? items::item_id_for(d.effect) : items::consumable_id_for(d.effect);

    if (id < 0) {
        blackboard.set<std::string>("hud_message", d.name + ": unknown effect");
    } else if (is_item && ship.item_id == id) {
        blackboard.set<std::string>("hud_message", d.name + " is already fitted");
    } else if (!is_item && ship.consumable_id == id) {
        blackboard.set<std::string>("hud_message", d.name + " is already loaded");
    } else if (ship.currency < d.price) {
        blackboard.set<std::string>("hud_message",
            "Not enough credits (" + std::to_string(d.price) + ")");
    } else {
        ship.currency -= d.price;
        equip(d, is_item, player, storage, blackboard);
        blackboard.set<std::string>("hud_message", d.name + (is_item ? " fitted" : " loaded"));
    }
    blackboard.set<float>("hud_message_timer", 2.0f);
}

void ShopSystem::equip(const ShopUpgradeDef& def, bool is_item, Entity player,
                       ComponentStorage& storage, Blackboard& blackboard) {
    auto s = storage.get_component<ShipState>(player);
    if (!s.has_value()) return;
    ShipState& ship = s->get();

    if (is_item) {
        // No refund on a replacement — §3 of Phase 3's handoff: no sell, no refund.
        ship.item_id = items::item_id_for(def.effect);
        // One number for the one equipped item, read by whichever system owns it
        // (PickupSystem's salvage multiplier, PlayerDamageSystem's reflect
        // damage, repulse_enemies' push speed). Same pattern as ship.extra_shots.
        blackboard.set<float>("ship.item_amount", def.amount);
        blackboard.set<std::string>("ship.item_name", def.name);
    } else {
        ship.consumable_id = items::consumable_id_for(def.effect);
        blackboard.set<std::string>("ship.consumable_name", def.name);
    }
}

void ShopSystem::apply(const ShopUpgradeDef& def, Entity player,
                       ComponentStorage& storage, Blackboard& blackboard) {
    auto ship = storage.get_component<ShipState>(player);
    if (!ship.has_value()) return;
    ShipState& s = ship->get();

    if (def.effect == "hull") {
        if (auto h = storage.get_component<Health>(player); h.has_value()) {
            h->get().max_hp += def.amount;
            h->get().current += def.amount;   // a repair, not just a bigger empty bar
        }
    } else if (def.effect == "shield") {
        s.shield_max += def.amount;
        // Regen is derived, not a second catalogue number: a bigger bank refills
        // proportionally, so a full recharge always takes the same wall time. That
        // rate is now data (D54) — 0.08 is ~12 s, where the old hardcoded 0.2 was 5 s.
        s.shield_regen = s.shield_max * (cfg_ ? cfg_->shield_regen_frac : 0.08f);
        s.shield = s.shield_max;              // the purchase arrives charged
    } else if (def.effect == "speed") {
        s.speed_mult += def.amount;
    } else if (def.effect == "fire_rate") {
        if (auto w = storage.get_component<WeaponStats>(player); w.has_value())
            w->get().fire_rate += def.amount;
    } else if (def.effect == "damage") {
        if (auto w = storage.get_component<WeaponStats>(player); w.has_value())
            w->get().damage += def.amount;
    } else if (def.effect == "range") {
        // D97: range is speed x lifetime, and only lifetime is moved. Raising
        // projectile_speed would also change lead, aim feel and how dodgeable a
        // shot is; lifetime changes nothing but where the shot expires.
        if (auto w = storage.get_component<WeaponStats>(player); w.has_value())
            w->get().projectile_lifetime += def.amount;
    } else if (def.effect == "bounce") {
        // Same Blackboard route as extra_shot: PlayerFireSystem stamps the count
        // onto each shot, so no catalogue index leaks into a system (D26).
        blackboard.set<int>("ship.bounces",
            blackboard.get_or<int>("ship.bounces", 0) + static_cast<int>(def.amount));
    } else if (def.effect == "extra_shot") {
        // PlayerFireSystem reads this rather than a catalogue index, so the barrel
        // count survives re-ordering the JSON rows.
        blackboard.set<int>("ship.extra_shots",
            blackboard.get_or<int>("ship.extra_shots", 0) + static_cast<int>(def.amount));
    }
}

// ===========================================================================
// Lane C (D61-D65) — gear levels and the clickable menu
// ===========================================================================

namespace {

// ponytail: one flat step per level rather than a per-row curve in the
// catalogue. The brief was explicit that the catalogue data does not change, and
// a second growth number nobody has playtested is a knob with no reader. Promote
// to `shop.gear_amount_step` in GameData.json the moment a playtest wants to
// tune it separately from the price.
constexpr float GEAR_AMOUNT_STEP = 0.25f;

// The drone preview, authored in the same 800x600 design canvas as the widgets
// (see ui-context.md). The glow disc is the aura; the hull sits inside it.
constexpr UIRect PREVIEW_SHIP{552.0f, 272.0f, 116.0f, 116.0f};
constexpr UIRect PREVIEW_GLOW{510.0f, 230.0f, 200.0f, 200.0f};

// Detail-pane geometry (D89): ONE fixed slot in the right column, under the
// drone preview. It used to track the hovered card's y, which meant the pane
// jumped on every hover and, over the 44..486 range, slid straight through the
// preview art it shares the column with. A pane the eye can find without
// re-locating it is worth more than proximity to the row.
constexpr float TIP_X = 448.0f, TIP_Y = 116.0f, TIP_W = 328.0f, TIP_H = 108.0f;

// D189: seconds a card must be held to complete a purchase.
constexpr float HOLD_TO_BUY_S = 1.0f;

/// Aura colour per equipped item. Mirrors the live item aura in main.cpp so the
/// preview and the flying drone agree; -1 (nothing fitted) draws no aura.
bool aura_color(int item_id, Tint& out) {
    switch (item_id) {
        case item_ids::MAGNET_CORE:      out = Tint{255, 210, 90,  150, true}; return true;
        case item_ids::REPULSOR_FIELD:   out = Tint{120, 220, 255, 150, true}; return true;
        case item_ids::REACTIVE_PLATING: out = Tint{255, 130, 60,  150, true}; return true;
        case item_ids::SALVAGER:         out = Tint{255, 235, 150, 150, true}; return true;
        default: return false;
    }
}

void set_label(ComponentStorage& storage, Entity e, const std::string& s) {
    if (e == 0) return;
    if (auto el = storage.get_component<UIElement>(e); el.has_value())
        el->get().label_text = s;
}

void set_rect(ComponentStorage& storage, Entity e, const UIRect& r) {
    if (e == 0) return;
    if (auto el = storage.get_component<UIElement>(e); el.has_value()) el->get().rect = r;
}

void set_disabled(ComponentStorage& storage, Entity e, bool d) {
    if (e == 0) return;
    if (auto st = storage.get_component<UIState>(e); st.has_value()) st->get().disabled = d;
}

/// Move a world entity so it lands on a fixed window-space point. RenderSystem
/// only draws entities that carry a Position (ScreenPosition alone is never
/// iterated), and CameraSystem overwrites ScreenPosition every frame — so the
/// only screen-locked recipe that needs no engine change is to invert the camera
/// transform here and let CameraSystem re-derive the screen point (D63).
void place_on_screen(ComponentStorage& storage, const Blackboard& blackboard,
                     Entity e, const UIRect& design_rect) {
    if (e == 0) return;
    const float win_w = static_cast<float>(blackboard.get_or<int>("window_width", 980));
    const float win_h = static_cast<float>(blackboard.get_or<int>("window_height", 660));
    const UIRect r = ui_apply_transform(ui_canvas_transform(win_w, win_h), design_rect);

    float zoom = blackboard.get_or<float>("camera.zoom", 1.0f);
    if (zoom < 0.01f) zoom = 0.01f;
    const float cam_left   = blackboard.get_or<float>("camera.lookat.x", 0.0f) - win_w / zoom * 0.5f;
    const float cam_bottom = blackboard.get_or<float>("camera.lookat.y", 0.0f) - win_h / zoom * 0.5f;

    if (auto p = storage.get_component<Position>(e); p.has_value()) {
        p->get().x = cam_left   + r.x / zoom;
        p->get().y = cam_bottom + r.y / zoom;
    }
    if (auto s = storage.get_component<Size>(e); s.has_value()) {
        s->get().width  = r.w / zoom;
        s->get().height = r.h / zoom;
    }
}

}  // namespace

int ShopSystem::gear_price(int index, int level) const {
    if (!cfg_ || index < 0 || index >= static_cast<int>(cfg_->items.size())) return 0;
    float p = static_cast<float>(cfg_->items[static_cast<size_t>(index)].price);
    for (int i = 0; i < level; ++i) p *= cfg_->price_growth;
    return static_cast<int>(std::round(p));
}

bool ShopSystem::owns_gear(const ShipState& ship, int index) const {
    if (!cfg_ || index < 0 || index >= static_cast<int>(cfg_->items.size())) return false;
    return ship.item_id ==
           items::item_id_for(cfg_->items[static_cast<size_t>(index)].effect);
}

bool ShopSystem::upgrade_gear(int index, ComponentStorage& storage,
                              Blackboard& blackboard, ShipState& ship) {
    (void)storage;   // levels are pure ShipState + Blackboard; no component touched
    blackboard.set<float>("hud_message_timer", 2.0f);
    if (!cfg_ || index < 0 || index >= static_cast<int>(cfg_->items.size())) {
        blackboard.set<std::string>("hud_message", "No gear to upgrade");
        return false;
    }
    const ShopUpgradeDef& d = cfg_->items[static_cast<size_t>(index)];
    if (!owns_gear(ship, index)) {
        // The whole point of the LEVELS page: it upgrades what is fitted, it does
        // not sell. Buying the item first is the GEAR page's job.
        blackboard.set<std::string>("hud_message", d.name + " is not fitted");
        return false;
    }
    if (d.amount <= 0.0f) {
        // Magnet Core is a boolean effect (amount 0) — a level would scale zero.
        blackboard.set<std::string>("hud_message", d.name + " has nothing to scale");
        return false;
    }
    const int level = ship.gear_levels[index];
    const int cost = gear_price(index, level);
    if (ship.currency < cost) {
        blackboard.set<std::string>("hud_message",
            "Not enough credits (" + std::to_string(cost) + ")");
        return false;
    }
    ship.currency -= cost;
    ship.gear_levels[index] = level + 1;
    // ship.item_amount is the one number every item consumer already reads
    // (repulsor push, reactive reflect, salvage multiplier) — D28/D41.
    blackboard.set<float>("ship.item_amount",
        d.amount * (1.0f + GEAR_AMOUNT_STEP * static_cast<float>(level + 1)));
    blackboard.set<std::string>("hud_message",
        d.name + " Lv" + std::to_string(level + 1));
    return true;
}

bool ShopSystem::menu_tick(ComponentStorage& storage, EntityManager& entity_manager,
                           Blackboard& blackboard) {
    if (!open_ || cfg_ == nullptr) { menu_teardown(storage, blackboard); return false; }
    if (menu_absent_) return false;
    if (!menu_built_ && !menu_build(storage, entity_manager, blackboard)) return false;

    Entity player = 0;
    if (!find_player(storage, player)) return false;
    auto ship_opt = storage.get_component<ShipState>(player);
    if (!ship_opt.has_value()) return false;
    ShipState& ship = ship_opt->get();

    // The card list the player was LOOKING at when the click was confirmed —
    // rebuilt before the click is routed, so "the row you saw is the row you
    // bought" survives a purchase that changes the list (fitting an item).
    rebuild_visible(ship);

    bool leave = false;
    const std::string click =
        blackboard.get_or<std::string>(UISystem::UI_CLICK_KEY, std::string());
    if (click.rfind("on_shop_", 0) == 0) {
        // Consume it: UISystem never clears the key, and an unconsumed click
        // re-fires every frame.
        blackboard.remove(UISystem::UI_CLICK_KEY);
        if (click == "on_shop_leave") {
            leave = true;
        } else if (click.rfind("on_shop_page_", 0) == 0) {
            const int p = click.back() - '0';
            if (p >= 0 && p <= 2) page_ = p;
        } else if (click.rfind("on_shop_card_", 0) == 0) {
            // D189: a click no longer buys — purchasing is press-and-HOLD
            // (below), so a stray click can't spend 200 credits. The click is
            // still consumed so it cannot re-fire. The 1-8 digit path keeps
            // instant purchase for headless scripts and tests.
        }
    }

    if (leave) {
        // Tear down here, not in the caller: the caller only knows to call
        // close(), and the preview entities and the pushed screen are ours.
        menu_teardown(storage, blackboard);
        return true;
    }

    const int card = hovered_card(storage);

    // D189: press-and-hold to buy. UIState.pressed is already sticky for the
    // whole press (UISystem sets it on down-inside, clears on release), so the
    // hold needs no engine change: accumulate delta_time while the same card
    // stays hovered and pressed, buy at the full second, and show progress as
    // a fill strip along the card's bottom edge.
    {
        const float dt = static_cast<float>(blackboard.get_or<double>("delta_time", 0.0));
        bool held = false;
        if (card >= 0 && card < static_cast<int>(visible_.size())) {
            if (auto st = storage.get_component<UIState>(card_[card]); st.has_value())
                held = st->get().pressed;
        }
        if (held && card == hold_card_) {
            hold_t_ += dt;
            if (hold_t_ >= HOLD_TO_BUY_S) {
                buy_visible_row(card, player, storage, blackboard, ship);
                hold_t_ = 0.0f;   // a kept hold buys again after another full hold
            }
        } else {
            hold_card_ = card;
            hold_t_ = held ? dt : 0.0f;
        }
        UIRect bar{0.0f, 0.0f, 0.0f, 0.0f};
        if (held && card >= 0) {
            bar = card_rect_[card];
            bar.w *= std::min(1.0f, hold_t_ / HOLD_TO_BUY_S);
            bar.h = 6.0f;   // a strip under the text, not a curtain over it
        }
        set_rect(storage, hold_bar_, bar);
    }

    refresh_cards(storage, ship);
    refresh_tooltip(storage, ship, card);
    refresh_preview(storage, blackboard, ship, card);
    return false;
}

bool ShopSystem::menu_build(ComponentStorage& storage, EntityManager& entity_manager,
                            Blackboard& blackboard) {
    // Same name -> entity resolution GameHUDSystem uses: the loader publishes
    // every named widget as a double under "ui.widget_id.<name>".
    auto id = [&](const std::string& name) -> Entity {
        const double v = blackboard.get_or<double>("ui.widget_id." + name, -1.0);
        return v < 0.0 ? 0 : static_cast<Entity>(v);
    };

    for (int i = 0; i < MENU_CARDS; ++i) card_[i] = id("shop_card_" + std::to_string(i));
    if (card_[0] == 0) {
        // No `shop` screen in this data file: stay on the legacy text list for
        // good, rather than retrying the lookup every frame.
        menu_absent_ = true;
        return false;
    }
    for (int i = 0; i < MENU_CARDS; ++i) {
        if (auto el = storage.get_component<UIElement>(card_[i]); el.has_value())
            card_rect_[i] = el->get().rect;
    }
    for (int i = 0; i < 3; ++i) tab_[i] = id("shop_tab_" + std::to_string(i));
    title_     = id("shop_title");
    credits_   = id("shop_credits");
    leave_     = id("shop_leave");
    tip_panel_ = id("shop_tip_panel");
    tip_name_  = id("shop_tip_name");
    tip_desc_  = id("shop_tip_desc");

    // The numbered Text rows were the thing the player complained about; with a
    // real menu up they would only overdraw it. open_ stays true — this drops the
    // rows, not the shop (refresh_rows() no-ops on an empty rows_).
    for (Entity e : rows_) storage.add_component<DestroyRequest>(e, DestroyRequest{});
    rows_.clear();

    blackboard.set<std::string>(ScreenStackSystem::CMD_PUSH, std::string(SCREEN_NAME));
    menu_pushed_ = true;

    // Preview: two plain world entities on a high RenderLayer, drawn under the UI
    // in the panel-free right half of the canvas. Position is rewritten every
    // frame by place_on_screen (D63).
    auto make_preview = [&](int layer) {
        Entity e = entity_manager.create_entity();
        storage.add_component<Position>(e, Position{0.0f, 0.0f});
        storage.add_component<Size>(e, Size{1.0f, 1.0f});
        storage.add_component<RenderLayer>(e, RenderLayer{layer});
        return e;
    };
    preview_glow_ = make_preview(6);
    storage.add_component<Images>(preview_glow_,
        // ResourceManager::load_texture prepends "<assets>/images/", so an
        // Images name is relative to assets/images — NOT to assets. (Sidecar
        // paths, e.g. PlayerConfig::sidecar, *are* relative to assets and do
        // carry the "images/" prefix; the two are not interchangeable.)
        Images{{"v2/glow_disc_128.png"}, 0});
    storage.add_component<Tint>(preview_glow_, Tint{255, 255, 255, 0, true});

    preview_ship_ = make_preview(7);
    // Borrow whatever art the live drone is wearing, so the preview follows the
    // selected ship (Lane F) without this system knowing anything about ships.
    for (Entity p : storage.entities_with_component<PlayerTag>()) {
        if (auto ss = storage.get_component<SpriteSheet>(p); ss.has_value())
            storage.add_component<SpriteSheet>(preview_ship_, ss->get());
        else if (auto im = storage.get_component<Images>(p); im.has_value())
            storage.add_component<Images>(preview_ship_, im->get());
        break;
    }

    // D189: the hold-to-buy fill bar. A code-made pooled widget (the
    // pause_stats recipe), parked at zero size until a card is held. Disabled
    // so it can never swallow the hover/press it is reporting on.
    hold_bar_ = entity_manager.create_entity();
    {
        UIElement el;
        el.element_type = "panel";
        el.rect = UIRect{0.0f, 0.0f, 0.0f, 0.0f};
        el.style_id = "hud_hp_ok";
        el.z_order = 45;
        storage.add_component<UIElement>(hold_bar_, el);
        storage.add_component<UIState>(hold_bar_, UIState{false, false, true, 0.0f, false});
        storage.add_component<ScreenMembership>(hold_bar_,
                                                ScreenMembership{std::string(SCREEN_NAME)});
    }
    hold_t_ = 0.0f;
    hold_card_ = -1;

    page_ = 0;
    menu_built_ = true;
    return true;
}

void ShopSystem::menu_teardown(ComponentStorage& storage, Blackboard& blackboard) {
    if (menu_pushed_) {
        blackboard.set<bool>(ScreenStackSystem::CMD_POP, true);
        menu_pushed_ = false;
    }
    for (Entity* e : {&preview_glow_, &preview_ship_, &hold_bar_}) {
        if (*e != 0) storage.add_component<DestroyRequest>(*e, DestroyRequest{});
        *e = 0;
    }
    hold_t_ = 0.0f;
    hold_card_ = -1;
    menu_built_ = false;
    visible_.clear();
    tip_name_text_.clear();
    tip_detail_text_.clear();
}

void ShopSystem::rebuild_visible(const ShipState& ship) {
    visible_.clear();
    if (cfg_ == nullptr) return;
    const int cap = MENU_CARDS;
    if (page_ == 0) {
        for (int i = 0; i < static_cast<int>(cfg_->upgrades.size()) && i < cap; ++i)
            visible_.push_back(i);
    } else if (page_ == 1) {
        const int total = static_cast<int>(cfg_->items.size() + cfg_->consumables.size());
        for (int i = 0; i < total && i < cap; ++i) visible_.push_back(i);
    } else {
        for (int i = 0; i < static_cast<int>(cfg_->items.size()) && i < cap; ++i)
            if (owns_gear(ship, i)) visible_.push_back(i);
    }
}

void ShopSystem::refresh_cards(ComponentStorage& storage, const ShipState& ship) {
    rebuild_visible(ship);

    const int page = (page_ < 0 || page_ > 2) ? 0 : page_;
    // The title is the SHOP; the tab strip already says which page you are on, so
    // repeating the page name in the heading was a third copy of the same word.
    set_label(storage, title_, "REACTOR SHOP");
    // One line of "what am I looking at" per page — the LEVELS page in particular
    // was a list of prices with no statement of what a level buys.
    static const char* kPageHint[3] = {
        "Permanent stat stacks. Prices rise as you buy.",
        "Fit one item and one [Q] consumable.",
        "Level the fitted item: +25% effect per level."
    };
    page_hint_ = kPageHint[page];
    set_label(storage, credits_, "Credits: " + std::to_string(ship.currency) +
                                 "    Keys: " + std::to_string(ship.keys));
    for (int i = 0; i < 3; ++i) set_disabled(storage, tab_[i], i == page_);

    const int n_items = static_cast<int>(cfg_->items.size());
    for (int c = 0; c < MENU_CARDS; ++c) {
        if (c >= static_cast<int>(visible_.size())) {
            // An empty card must not draw its button body: UIElement has no
            // visibility flag, so a zero-width rect is the hide (and disabled
            // keeps it out of hit-testing and keyboard focus).
            set_rect(storage, card_[c], UIRect{card_rect_[c].x, card_rect_[c].y, 0.0f, 0.0f});
            set_label(storage, card_[c], std::string());
            set_disabled(storage, card_[c], true);
            continue;
        }
        set_rect(storage, card_[c], card_rect_[c]);
        set_disabled(storage, card_[c], false);

        const int idx = visible_[static_cast<size_t>(c)];
        std::string line;
        if (page_ == 0) {
            const ShopUpgradeDef& d = cfg_->upgrades[static_cast<size_t>(idx)];
            const int owned = ship.upg_counts[idx];
            const bool maxed = d.max_stacks > 0 && owned >= d.max_stacks;
            line = d.name + "  +" + num(d.amount);
            if (owned > 0) line += " x" + std::to_string(owned);
            line += maxed ? "   MAX"
                          : "   " + std::to_string(price_for(idx, owned)) + " cr";
        } else if (page_ == 1) {
            const bool is_item = idx < n_items;
            const ShopUpgradeDef& d = is_item
                ? cfg_->items[static_cast<size_t>(idx)]
                : cfg_->consumables[static_cast<size_t>(idx - n_items)];
            const int gid = is_item ? items::item_id_for(d.effect)
                                    : items::consumable_id_for(d.effect);
            const bool held = is_item ? (ship.item_id == gid) : (ship.consumable_id == gid);
            line = (is_item ? "ITEM " : "USE  ") + d.name +
                   (held ? "   EQUIPPED" : "   " + std::to_string(d.price) + " cr");
        } else {
            // LEVELS reads as a transition, not a state: what you have, what the
            // click gives you, what it costs — in that order, left to right.
            const ShopUpgradeDef& d = cfg_->items[static_cast<size_t>(idx)];
            const int level = ship.gear_levels[idx];
            line = d.name + "  LV" + std::to_string(level);
            line += (d.amount <= 0.0f)
                ? "  -  no scaling"
                : " > LV" + std::to_string(level + 1) + "   " +
                  std::to_string(gear_price(idx, level)) + " cr";
        }
        set_label(storage, card_[c], line);
    }

    // The LEVELS page with nothing fitted is a dead-end unless it says why.
    if (page_ == 2 && visible_.empty()) {
        set_rect(storage, card_[0], card_rect_[0]);
        set_label(storage, card_[0], "Nothing fitted - buy an item on GEAR first");
        set_disabled(storage, card_[0], true);
    }
}

int ShopSystem::hovered_card(const ComponentStorage& storage) const {
    for (int c = 0; c < MENU_CARDS; ++c) {
        if (card_[c] == 0) continue;
        auto st = storage.get_component<UIState>(card_[c]);
        if (st.has_value() && st->get().hovered && !st->get().disabled) return c;
    }
    return -1;
}

void ShopSystem::refresh_tooltip(ComponentStorage& storage, const ShipState& ship,
                                 int card) {
    tip_name_text_.clear();
    tip_detail_text_.clear();

    if (card >= 0 && card < static_cast<int>(visible_.size())) {
        const int idx = visible_[static_cast<size_t>(card)];
        const int n_items = static_cast<int>(cfg_->items.size());
        if (page_ == 0) {
            const ShopUpgradeDef& d = cfg_->upgrades[static_cast<size_t>(idx)];
            tip_name_text_ = d.name;
            tip_detail_text_ = "+" + num(d.amount) + " " + d.effect + " per stack, owned " +
                               std::to_string(ship.upg_counts[idx]) +
                               (d.max_stacks > 0 ? "/" + std::to_string(d.max_stacks)
                                                 : std::string());
        } else if (page_ == 1) {
            const bool is_item = idx < n_items;
            const ShopUpgradeDef& d = is_item
                ? cfg_->items[static_cast<size_t>(idx)]
                : cfg_->consumables[static_cast<size_t>(idx - n_items)];
            tip_name_text_ = d.name;
            tip_detail_text_ = d.effect + (is_item ? " - fills the item slot"
                                                   : " - fills the [Q] slot");
        } else {
            const ShopUpgradeDef& d = cfg_->items[static_cast<size_t>(idx)];
            const int level = ship.gear_levels[idx];
            tip_name_text_ = d.name;
            tip_detail_text_ = "Lv" + std::to_string(level) + " -> Lv" +
                               std::to_string(level + 1) + ", +" +
                               std::to_string(static_cast<int>(GEAR_AMOUNT_STEP * 100.0f)) +
                               "% " + d.effect;
        }
    }

    // D189: the pane doubles as the hold-to-buy prompt while a row is hovered.
    if (!tip_detail_text_.empty()) tip_detail_text_ += "  -  HOLD to buy";

    set_label(storage, tip_name_, tip_name_text_);
    set_label(storage, tip_desc_, tip_detail_text_);
    if (tip_name_text_.empty()) {
        // Idle state, not a hole: the pane holds the page's one-line explanation
        // until a row is hovered. An empty pane that appears and disappears is a
        // flicker; a pane that always says something is a place to look.
        set_label(storage, tip_name_, std::string());
        set_label(storage, tip_desc_, page_hint_);
    }
    set_rect(storage, tip_panel_, UIRect{TIP_X, TIP_Y, TIP_W, TIP_H});
    set_rect(storage, tip_name_,  UIRect{TIP_X + 16.0f, TIP_Y + 64.0f, TIP_W - 32.0f, 30.0f});
    // The description box is two lines tall on purpose: a long effect string now
    // shrinks a little inside a generous box instead of a lot inside a thin one.
    set_rect(storage, tip_desc_,  UIRect{TIP_X + 16.0f, TIP_Y + 14.0f, TIP_W - 32.0f, 44.0f});
}

void ShopSystem::refresh_preview(ComponentStorage& storage, const Blackboard& blackboard,
                                 const ShipState& ship, int card) {
    // Nothing hovered -> the drone as it is right now. That is the answer to
    // "what does my ship look like equipped" when the player is not shopping a
    // specific row.
    int show_item = ship.item_id;
    if (card >= 0 && card < static_cast<int>(visible_.size())) {
        const int idx = visible_[static_cast<size_t>(card)];
        const int n_items = static_cast<int>(cfg_->items.size());
        if (page_ == 2 || (page_ == 1 && idx < n_items))
            show_item = items::item_id_for(cfg_->items[static_cast<size_t>(idx)].effect);
    }

    place_on_screen(storage, blackboard, preview_ship_, PREVIEW_SHIP);
    place_on_screen(storage, blackboard, preview_glow_, PREVIEW_GLOW);

    // Lane N (D123): the glow also carries the upgrade tier, so the preview
    // changes in the frame the credits are spent — same ramp the flying drone's
    // plume uses, so the two never disagree. A fitted item still owns the hue;
    // upgrades only push it brighter.
    const upgrade_visuals::Look up = upgrade_visuals::look_for(upgrade_visuals::tier(ship));
    const int t_alpha = 40 * upgrade_visuals::tier(ship);
    Tint aura{};
    const bool lit = aura_color(show_item, aura);
    if (lit) aura.a = static_cast<uint8_t>(std::min(255, aura.a + t_alpha));
    if (auto t = storage.get_component<Tint>(preview_glow_); t.has_value())
        t->get() = lit ? aura
                       : Tint{up.start_r, up.start_g, up.start_b,
                              static_cast<uint8_t>(t_alpha), true};
}

void ShopSystem::buy_visible_row(int card, Entity player, ComponentStorage& storage,
                                 Blackboard& blackboard, ShipState& ship) {
    if (card < 0 || card >= static_cast<int>(visible_.size())) return;
    const int idx = visible_[static_cast<size_t>(card)];
    if (page_ == 0)      buy_upgrade(idx, player, storage, blackboard, ship);
    else if (page_ == 1) buy_gear(idx, player, storage, blackboard, ship);
    else                 upgrade_gear(idx, storage, blackboard, ship);
}
