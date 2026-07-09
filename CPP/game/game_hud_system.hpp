#ifndef GAME_HUD_SYSTEM_HPP
#define GAME_HUD_SYSTEM_HPP

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * GameHUDSystem — the arena HUD: score, wave, health, level, a large status
 * banner (title / game-over / victory), and the transient level-up message.
 *
 * init() spawns the HUD Text entities once; update() refreshes their content
 * each frame from the Blackboard ("score", "wave"/"total_waves", "level",
 * "phase", "upgrade_message") and the player's Health component.
 */
class GameHUDSystem {
public:
    void init(ComponentStorage& component_storage,
              EntityManager& entity_manager,
              const Blackboard& blackboard);

    void update(ComponentStorage& component_storage, Blackboard& blackboard);

private:
    Entity score_entity_ = 0;
    Entity wave_entity_ = 0;
    Entity health_entity_ = 0;
    Entity level_entity_ = 0;
    Entity status_entity_ = 0;
    Entity message_entity_ = 0;
    bool initialized_ = false;
};

#endif // GAME_HUD_SYSTEM_HPP
