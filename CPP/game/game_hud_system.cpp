#include "game_hud_system.hpp"
#include "player_components.hpp"   // PlayerTag, ShipState
#include "enemy_components.hpp"    // Health
#include <cstdio>
#include <string>

namespace {
std::string int_str(int v) { return std::to_string(v); }
}

void GameHUDSystem::init(ComponentStorage& component_storage,
                         EntityManager& entity_manager,
                         const Blackboard& blackboard) {
    const float win_w = static_cast<float>(blackboard.get_or<int>("window_width", 980));
    const float win_h = static_cast<float>(blackboard.get_or<int>("window_height", 660));

    auto make = [&](float x, float y, float size, SDL_Color color,
                    const std::string& content) {
        Entity e = entity_manager.create_entity();
        component_storage.add_component<ScreenPosition>(e, ScreenPosition{x, y});
        component_storage.add_component<Text>(e, Text{content, "default.ttf", size, color});
        return e;
    };

    const SDL_Color white  = {235, 235, 245, 255};
    const SDL_Color yellow = {255, 220, 90, 255};
    const SDL_Color cyan   = {120, 225, 255, 255};

    score_entity_   = make(20.0f, win_h - 36.0f, 24.0f, white, "Score: 0");
    health_entity_  = make(20.0f, win_h - 66.0f, 24.0f, white, "Health: 100");
    credits_entity_ = make(20.0f, win_h - 96.0f, 24.0f, yellow, "Credits: 0");
    slots_entity_   = make(20.0f, win_h - 124.0f, 20.0f, cyan, "");
    wave_entity_    = make(win_w - 200.0f, win_h - 36.0f, 24.0f, white, "Wave: 0/0");
    status_entity_  = make(win_w * 0.5f - 190.0f, win_h * 0.5f, 44.0f, yellow, "");
    message_entity_ = make(win_w * 0.5f - 170.0f, win_h * 0.5f - 60.0f, 28.0f, cyan, "");
    initialized_ = true;
}

void GameHUDSystem::update(ComponentStorage& component_storage, Blackboard& blackboard) {
    if (!initialized_) return;

    // Score
    if (auto t = component_storage.get_component<Text>(score_entity_); t.has_value()) {
        t->get().content = "Score: " + int_str(blackboard.get_or<int>("score", 0));
    }

    // Health + economy (from the player entity)
    int hp = 0, credits = 0, keys = 0, shield = 0;
    int item_id = -1, consumable_id = -1, buff_id = -1;
    float buff_timer = 0.0f;
    for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
        if (auto h = component_storage.get_component<Health>(p); h.has_value()) {
            hp = static_cast<int>(h->get().current + 0.5f);
        }
        if (auto s = component_storage.get_component<ShipState>(p); s.has_value()) {
            credits = s->get().currency;
            keys    = s->get().keys;
            shield  = static_cast<int>(s->get().shield + 0.5f);
            item_id = s->get().item_id;
            consumable_id = s->get().consumable_id;
            buff_id = s->get().buff_id;
            buff_timer = s->get().buff_timer;
        }
        break;
    }
    // Shields only appear once a Shield Capacitor has been bought — same rule as
    // the key count below: a permanent "Shield: 0" is noise for most of a run.
    if (auto t = component_storage.get_component<Text>(health_entity_); t.has_value()) {
        t->get().content = "Health: " + int_str(hp) +
                           (shield > 0 ? "   Shield: " + int_str(shield) : std::string());
    }

    // Credits, with the key count only once one has actually dropped (D2 keys are
    // rare enough that a permanent "Keys: 0" would just be noise).
    if (auto t = component_storage.get_component<Text>(credits_entity_); t.has_value()) {
        t->get().content = "Credits: " + int_str(credits) +
                           (keys > 0 ? "   Keys: " + int_str(keys) : std::string());
    }

    // Gameplay Phase 4: equipped gear and the live buff countdown. The names are
    // whatever ShopSystem published when the slot was filled, so the HUD needs no
    // catalogue. Empty line when nothing is equipped — same "no noise" rule as
    // the shield and key readouts.
    if (auto t = component_storage.get_component<Text>(slots_entity_); t.has_value()) {
        std::string line;
        if (item_id >= 0)
            line += blackboard.get_or<std::string>("ship.item_name", std::string("Item"));
        if (consumable_id >= 0) {
            if (!line.empty()) line += "   ";
            line += "[Q] " +
                blackboard.get_or<std::string>("ship.consumable_name", std::string("Consumable"));
        }
        if (buff_id >= 0) {
            if (!line.empty()) line += "   ";
            char buf[24];
            std::snprintf(buf, sizeof(buf), "OVERDRIVE %.1fs", static_cast<double>(buff_timer));
            line += buf;
        }
        t->get().content = line;
    }

    // Wave
    if (auto t = component_storage.get_component<Text>(wave_entity_); t.has_value()) {
        t->get().content = "Wave: " + int_str(blackboard.get_or<int>("wave", 0)) +
                           "/" + int_str(blackboard.get_or<int>("total_waves", 0));
    }

    // Status banner by phase (0=title,1=playing,2=gameover,3=victory).
    int phase = blackboard.get_or<int>("phase", 0);
    std::string status;
    if (phase == 0) status = "REACTOR DRONE - click to start";
    else if (phase == 2) status = "GAME OVER - click to retry";
    else if (phase == 3) status = "VICTORY! - click to retry";
    if (auto t = component_storage.get_component<Text>(status_entity_); t.has_value()) {
        t->get().content = status;
    }

    // Transient message channel (timer ticks down in main via delta_time).
    std::string msg;
    float msg_timer = blackboard.get_or<float>("hud_message_timer", 0.0f);
    if (msg_timer > 0.0f) msg = blackboard.get_or<std::string>("hud_message", std::string());
    if (auto t = component_storage.get_component<Text>(message_entity_); t.has_value()) {
        t->get().content = msg;
    }
}
