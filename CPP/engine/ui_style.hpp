/**
 * ui_style.hpp — Widget style data model and parsing for UIRenderSystem.
 *
 * Pure data + pure logic, with NO SDL and no rendering dependency, so the
 * style system is trivially testable without a window. Defines the four
 * widget interaction states, one named style's per-state bg/text colors,
 * the StyleTable that holds all loaded styles, the single fixed default
 * color used for every missing lookup, and the GameData parsing entry point.
 *
 * Colors reuse the engine-wide `Color` struct (uint8_t RGBA), so every
 * channel is structurally 0..255 and parsing only admits integer channels
 * in range.
 *
 * Added in Phase 2 (o-040-02-widget-rendering). The .cpp (task 1.2)
 * implements parse_widget_state, StyleTable, and parse_ui_styles.
 */

#pragma once

#include "engine/ecs/components.hpp"   // reuse Color

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>

// Forward declaration of nlohmann::json so the header stays free of the full
// json include; the implementation (.cpp) includes <nlohmann/json.hpp>.
#include <nlohmann/json_fwd.hpp>

/**
 * The four interaction states a widget's colors can be selected by.
 * Indexed numerically via static_cast<int>(WidgetState) into UIStyle::states.
 */
enum class WidgetState {
    Normal,    // 0
    Hovered,   // 1
    Pressed,   // 2
    Disabled   // 3
};

/**
 * Parse a widget-state name.
 *
 * Returns the matching WidgetState for an exact, case-sensitive match of
 * "normal", "hovered", "pressed", or "disabled"; returns std::nullopt for
 * anything else. (R6.6)
 */
std::optional<WidgetState> parse_widget_state(const std::string& name);

/**
 * One resolved (bg, text) color pair returned by a successful style lookup.
 */
struct ResolvedStyle {
    Color bg;
    Color text;
};

/**
 * The single fixed default color used for EVERY missing lookup. (R5.7, R6.3)
 * Magenta is an intentional "missing style" sentinel — highly visible in-game
 * so authoring mistakes are obvious.
 */
inline constexpr Color UI_DEFAULT_COLOR{255, 0, 255, 255};

/**
 * One named style: an independently-optional bg and text color per state.
 * A missing (omitted) color is std::nullopt and resolves to UI_DEFAULT_COLOR
 * at lookup time.
 */
struct UIStyle {
    struct StateColors {
        std::optional<Color> bg;    // omitted/malformed -> nullopt (R5.6)
        std::optional<Color> text;  // omitted/malformed -> nullopt (R5.6)
    };
    // Indexed by static_cast<int>(WidgetState): [Normal, Hovered, Pressed, Disabled].
    std::array<StateColors, 4> states;
};

/**
 * The in-memory collection of all loaded UIStyle entries, keyed by style id.
 */
class StyleTable {
public:
    // Insert or replace the style stored under `id`.
    void set_style(const std::string& id, UIStyle style);

    // True if a style is stored under `id`.
    bool contains(const std::string& id) const;

    // Number of stored styles.
    std::size_t size() const;

    // Style lookup (R6). Contract:
    //  - empty style_id            -> std::nullopt                      (R6.5)
    //  - non-empty id absent       -> ResolvedStyle{default, default}   (R6.3)
    //  - present id, color stored  -> the stored color                 (R6.1, R6.2)
    //  - present id, color missing -> UI_DEFAULT_COLOR for that channel (R5.7)
    // Deterministic: identical arguments always yield identical results. (R6.7)
    std::optional<ResolvedStyle> lookup(const std::string& style_id,
                                        WidgetState state) const;

private:
    std::unordered_map<std::string, UIStyle> styles_;
};

/**
 * Build a StyleTable from the "ui_styles" JSON value. (R5.1–R5.6)
 *
 *  - value not an object (or null) -> empty table, no throw           (R5.4, R5.5)
 *  - per style, per state: parse bg & text as exactly four integer
 *    channels (red, green, blue, alpha) in the range 0..255
 *  - any malformed channel set (missing, wrong count, non-integer, or
 *    out-of-range) -> omit that one color entry (leave nullopt), no throw (R5.6)
 */
StyleTable parse_ui_styles(const nlohmann::json& ui_styles_value);
