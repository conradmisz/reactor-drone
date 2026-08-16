#ifndef PAUSE_STATS_HPP
#define PAUSE_STATS_HPP

#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "arena_config.hpp"   // GameConfig, ShopUpgradeDef

/**
 * pause_stats — the pause screen's character sheet (#5) and the HUD's
 * active-item slot (#13), Lane M / D113-D117.
 *
 * One system for both because they are one question asked twice: "what is this
 * drone right now?". Both read the same ShipState/WeaponStats/Health off the
 * player entity in the same frame, and splitting them would mean two lookups,
 * two hooks and two places to forget the phase gate.
 *
 * The stat lines are a PURE function of a flat snapshot, so every line — the
 * upgrade arithmetic especially — is unit tested with no window and no world.
 * The system half only moves the strings into widgets.
 *
 * Why widgets and not HUD Text (D58/D63): the pause panel is drawn by
 * UIRenderSystem, which composites last, so a HUDSystem Text row would be drawn
 * *underneath* the panel. The stat lines are therefore pooled UIElement labels
 * on the "pause" screen, created once and relabelled — the mechanism the minimap
 * blips and the shop's detail pane already use.
 */
namespace pause_stats {

/// Everything the readout needs, flattened off the player entity. A struct
/// rather than eight parameters because the test builds one per case.
struct Snapshot {
    float hull = 0.0f, hull_max = 0.0f;
    float shield = 0.0f, shield_max = 0.0f;
    float base_speed = 0.0f;        // GameConfig::player move_speed, px/s
    float speed_mult = 1.0f;
    float fire_rate = 0.0f, damage = 0.0f;
    int   upg_counts[8] = {0};
    int   item_id = -1, consumable_id = -1, active_id = -1;
    std::string item_name, consumable_name, active_name;
    /// Lane O's persistent progression (#14). 0 = no prestige run yet, which is
    /// every run today — the line is simply absent. See the spec's open question.
    int   prestige = 0;
};

/// Slots the pause screen reserves for stat lines. Playtest #1 item 8 (D227)
/// split FIRE RATE and DAMAGE into their own rows (each needs its own pip) and
/// dropped the GEAR row with the gear economy (D225): 5 stats + a blank + the
/// UPGRADES header + 8 upgrade rows = 15, plus the prestige line = 16.
constexpr int MAX_LINES = 16;

/// Where line `i` is drawn, in the 800x600 design canvas (bottom-left origin).
/// Public so the screen-layout test can prove these never land on an authored
/// widget — the exact class of bug #2 turned out to be.
UIRect line_rect(int i);

/// Where line `i`'s pip meter is drawn — ONE fixed x column (playtest #1 item
/// 8, D227), so the bubbles align no matter how long the value text is.
UIRect pip_rect(int i);

/// Pip meters, row-for-row parallel to stat_lines ("" where a row has none).
std::vector<std::string> stat_pips(const Snapshot& s,
                                   const std::vector<ShopUpgradeDef>& upgrades);

/// The character sheet, top line first. Never longer than MAX_LINES.
std::vector<std::string> stat_lines(const Snapshot& s,
                                    const std::vector<ShopUpgradeDef>& upgrades);

/// Short tag and key hint for the equipped active, for the HUD slot (#13).
/// The repulsion device is not on a key at all — it auto-fires below 20% hull.
std::string active_tag(int active_id);
std::string active_key(int active_id);

}  // namespace pause_stats

/**
 * Drives both readouts. Instantiate once (a function-local static in main's
 * hook, like MinimapSystem) — the label pool survives spawn_world because it is
 * made of UIElement carriers.
 */
class PauseStatsSystem {
public:
    static constexpr const char* PAUSE_SCREEN = "pause";
    static constexpr const char* SLOT_FRAME   = "item_slot_frame";
    static constexpr const char* SLOT_NAME    = "item_slot_name";
    static constexpr const char* SLOT_KEY     = "item_slot_key";

    void update(ComponentStorage& cs, EntityManager& em, Blackboard& bb,
                const GameConfig& cfg);

private:
    void ensure_pool(ComponentStorage& cs, EntityManager& em);
    void resolve_slot(ComponentStorage& cs, const Blackboard& bb);

    std::vector<Entity> pool_;
    std::vector<Entity> pips_;   // parallel pip-column labels (D227)
    Entity slot_[3] = {0, 0, 0};      // frame, name, key
    UIRect slot_rect_[3] = {};        // authored geometry, cached before hiding
    bool   slot_resolved_ = false;
};

#endif  // PAUSE_STATS_HPP
