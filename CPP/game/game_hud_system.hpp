#ifndef GAME_HUD_SYSTEM_HPP
#define GAME_HUD_SYSTEM_HPP

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * GameHUDSystem — the arena HUD: score, wave, health, credits/keys, a large
 * status banner (title / game-over / victory), and a transient message line.
 *
 * init() spawns the HUD Text entities once; update() refreshes their content
 * each frame from the Blackboard ("score", "wave"/"total_waves",
 * "phase", "hud_message") and the player's Health component.
 *
 * The hull and shield GAUGES are not text. They are `panel` widgets authored in
 * GameData.json on the always-active "gameplay" UI screen, and this system only
 * resizes and recolours them — reusing UIRenderSystem's styled-rect path rather
 * than teaching HUDSystem (which draws text and nothing else) to fill rects.
 * The widgets are resolved once, by name, through the "ui.widget_id.<name>" keys
 * the GameData loader publishes.
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
    Entity credits_entity_ = 0;
    Entity slots_entity_ = 0;      // Phase 4: equipped item / consumable / buff timer
    Entity status_entity_ = 0;
    Entity message_entity_ = 0;
    bool initialized_ = false;

    // --- Hull / shield gauges (UI panel widgets, resolved by name) ---
    /// Resolve the bar widgets from the Blackboard. Safe to call repeatedly; a
    /// data file with no "gameplay" screen simply leaves them unresolved and the
    /// gauge code becomes a no-op.
    void resolve_bars(const Blackboard& blackboard);

    /// Set a bar widget's fill width to `frac` of its full width, and optionally
    /// restyle it. A widget that failed to resolve is skipped.
    void set_bar(ComponentStorage& cs, Entity bar, float full_w, float frac,
                 const char* style_id = nullptr);

    Entity hp_chip_ = 0, hp_fill_ = 0, sh_fill_ = 0;
    bool   bars_resolved_ = false;

    // The chip bar trails the real hull value, so a hit reads as a red block
    // draining away rather than as a bar that is silently shorter than last frame.
    float  hp_chip_frac_ = 1.0f;
};

#endif // GAME_HUD_SYSTEM_HPP
