#include "shop_system.hpp"
#include "enemy_components.hpp"    // Health
#include "item_system.hpp"         // items::item_id_for / consumable_id_for
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
    // and this keeps "the row you saw is the row you bought" true.
    if (toggle_page) {
        page_ = 1 - page_;
    } else if (page_ == 1) {
        buy_gear(digit - 1, player, storage, blackboard, ship);
    } else {
        const int index = digit - 1;
        if (index >= 0 && index < static_cast<int>(cfg_->upgrades.size())) {
            const ShopUpgradeDef& d = cfg_->upgrades[static_cast<size_t>(index)];
            const int owned = ship.upg_counts[index];
            const int cost = price_for(index, owned);
            if (d.max_stacks > 0 && owned >= d.max_stacks) {
                blackboard.set<std::string>("hud_message", d.name + " is maxed out");
                blackboard.set<float>("hud_message_timer", 2.0f);
            } else if (ship.currency < cost) {
                blackboard.set<std::string>("hud_message",
                    "Not enough credits (" + std::to_string(cost) + ")");
                blackboard.set<float>("hud_message_timer", 2.0f);
            } else {
                ship.currency -= cost;
                ship.upg_counts[index] = owned + 1;
                apply(d, player, storage, blackboard);
                blackboard.set<std::string>("hud_message", d.name + " installed");
                blackboard.set<float>("hud_message_timer", 2.0f);
            }
        }
    }

    refresh_rows(storage, ship);
    return leave;
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
    } else if (def.effect == "extra_shot") {
        // PlayerFireSystem reads this rather than a catalogue index, so the barrel
        // count survives re-ordering the JSON rows.
        blackboard.set<int>("ship.extra_shots",
            blackboard.get_or<int>("ship.extra_shots", 0) + static_cast<int>(def.amount));
    }
}
