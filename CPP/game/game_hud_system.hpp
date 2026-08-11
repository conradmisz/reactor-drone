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

/**
 * Is the arena HUD (gauges, readouts, minimap) furniture the player should be
 * looking at in this phase?
 *
 * The "gameplay" screen is ScreenStackSystem's base sentinel: it is on the stack
 * in every phase and is never modal, so its widgets used to render on the title
 * screen and underneath the shop panel. Visibility and simulation are separate
 * questions (HANDOFF trap 7) and this answers only the first: the HUD belongs to
 * the two phases that actually fly the drone — PHASE_PLAYING and
 * PHASE_INTERMISSION — and to nothing else. The intermission keeps it because
 * the drone is still collecting loot under that prompt, and its panel is centered
 * clear of the top-left gauges.
 *
 * Takes the raw Blackboard "phase" int rather than the enum, which lives in
 * main.cpp; the values are pinned by a unit test.
 */
inline bool hud_visible_in_phase(int phase) {
    constexpr int kPlaying = 1;        // Phase::PHASE_PLAYING
    constexpr int kIntermission = 5;   // Phase::PHASE_INTERMISSION
    return phase == kPlaying || phase == kIntermission;
}

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
    void resolve_bars(ComponentStorage& cs, const Blackboard& blackboard);

    /// Set a bar widget's fill width to `frac` of its full width, and optionally
    /// restyle it. A widget that failed to resolve is skipped.
    void set_bar(ComponentStorage& cs, Entity bar, float full_w, float frac,
                 const char* style_id = nullptr);

    /// Show/hide every widget on the "gameplay" screen by restoring or collapsing
    /// its rect. UIElement has no visibility flag (D61) and adding one would be an
    /// engine change; a zero-size rect is how this codebase already hides a widget.
    void set_widgets_visible(ComponentStorage& cs, bool visible);

    // D192: +2 for the battery gauge (#9) and +3 for the boss bar (#8). The boss
    // widgets ride this array so the phase gate collapses them for free; the
    // "no boss" case then collapses them a second time, which is idempotent.
    // +2 for the dash button of the ability row, for the same free phase gate:
    // its frame and the SPACE prompt. The button's booster glyph and circular
    // cooldown dial are sprites, not widgets (UIElement has no texture path), so
    // main.cpp parks those two itself.
    static constexpr int GAUGE_WIDGETS = 13;
    Entity gauge_[GAUGE_WIDGETS] = {0};
    UIRect gauge_rect_[GAUGE_WIDGETS] = {};   // authored geometry, cached once
    Entity hp_chip_ = 0, hp_fill_ = 0, sh_fill_ = 0, bat_fill_ = 0;
    Entity boss_bg_ = 0, boss_fill_ = 0, boss_label_ = 0;
    UIRect boss_bg_rect_{}, boss_label_rect_{};
    bool   bars_resolved_ = false;

    // The chip bar trails the real hull value, so a hit reads as a red block
    // draining away rather than as a bar that is silently shorter than last frame.
    float  hp_chip_frac_ = 1.0f;
};

#endif // GAME_HUD_SYSTEM_HPP
