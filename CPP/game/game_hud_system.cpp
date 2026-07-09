#include "game_hud_system.hpp"
#include "player_components.hpp"   // PlayerTag
#include "enemy_components.hpp"    // Health
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
    level_entity_   = make(20.0f, win_h - 96.0f, 24.0f, white, "Level: 1");
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

    // Health (from the player entity)
    int hp = 0;
    for (Entity p : component_storage.entities_with_component<PlayerTag>()) {
        if (auto h = component_storage.get_component<Health>(p); h.has_value()) {
            hp = static_cast<int>(h->get().current + 0.5f);
        }
        break;
    }
    if (auto t = component_storage.get_component<Text>(health_entity_); t.has_value()) {
        t->get().content = "Health: " + int_str(hp);
    }

    // Level
    if (auto t = component_storage.get_component<Text>(level_entity_); t.has_value()) {
        t->get().content = "Level: " + int_str(blackboard.get_or<int>("level", 1));
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

    // Transient level-up message (timer ticks down in main via delta_time).
    std::string msg;
    float msg_timer = blackboard.get_or<float>("upgrade_message_timer", 0.0f);
    if (msg_timer > 0.0f) msg = blackboard.get_or<std::string>("upgrade_message", std::string());
    if (auto t = component_storage.get_component<Text>(message_entity_); t.has_value()) {
        t->get().content = msg;
    }
}
