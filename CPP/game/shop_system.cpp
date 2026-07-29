#include "shop_system.hpp"
#include "enemy_components.hpp"    // Health
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
    y -= 44.0f;
    for (size_t i = 0; i < cfg_->upgrades.size(); ++i) {
        make(x, y, 24.0f, white);
        y -= 32.0f;
    }
    y -= 14.0f;
    make(x, y, 22.0f, cyan);            // footer

    open_ = true;
}

void ShopSystem::close(ComponentStorage& storage) {
    for (Entity e : rows_) storage.add_component<DestroyRequest>(e, DestroyRequest{});
    rows_.clear();
    open_ = false;
}

void ShopSystem::refresh_rows(ComponentStorage& storage, const ShipState& ship) {
    if (rows_.size() < cfg_->upgrades.size() + 3) return;

    auto set_text = [&](size_t row, const std::string& s) {
        if (auto t = storage.get_component<Text>(rows_[row]); t.has_value()) t->get().content = s;
    };

    set_text(0, "REACTOR SHOP");
    set_text(1, "Credits: " + std::to_string(ship.currency) +
                 "    Keys: " + std::to_string(ship.keys));

    for (size_t i = 0; i < cfg_->upgrades.size(); ++i) {
        const ShopUpgradeDef& d = cfg_->upgrades[i];
        const int owned = ship.upg_counts[i];
        const bool maxed = d.max_stacks > 0 && owned >= d.max_stacks;
        std::string line = "[" + std::to_string(i + 1) + "] " + d.name +
                           "  +" + num(d.amount);
        if (owned > 0) line += "  (x" + std::to_string(owned) + ")";
        line += maxed ? "   MAX"
                      : "   " + std::to_string(price_for(static_cast<int>(i), owned)) + " cr";
        set_text(i + 2, line);
    }

    set_text(cfg_->upgrades.size() + 2, "Press 1-" + std::to_string(cfg_->upgrades.size()) +
                                        " to buy    B to launch");
}

bool ShopSystem::update(ComponentStorage& storage, Blackboard& blackboard,
                        int digit, bool leave) {
    if (!open_ || cfg_ == nullptr) return true;

    Entity player = 0;
    if (!find_player(storage, player)) return true;
    auto ship_opt = storage.get_component<ShipState>(player);
    if (!ship_opt.has_value()) return true;
    ShipState& ship = ship_opt->get();

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

    refresh_rows(storage, ship);
    return leave;
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
        // proportionally, so a full recharge always takes ~5 s of not being hit.
        s.shield_regen = s.shield_max * 0.2f;
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
