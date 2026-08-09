/**
 * ui_style.cpp — Implementation of the widget style data model and parsing.
 *
 * Pure data + pure logic, with NO SDL and no rendering dependency. Implements
 * parse_widget_state, the StyleTable accessors and lookup contract, and the
 * GameData "ui_styles" parser. See ui_style.hpp for the full API contract and
 * the per-requirement rationale.
 *
 * Added in Phase 2 (o-040-02-widget-rendering), task 1.2.
 */

#include "engine/ui_style.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// parse_widget_state
// ---------------------------------------------------------------------------
// Exact, case-sensitive match of the four state names. Anything else (unknown
// name, different case, empty string) yields std::nullopt. (R6.6)
std::optional<WidgetState> parse_widget_state(const std::string& name) {
    if (name == "normal")   return WidgetState::Normal;
    if (name == "hovered")  return WidgetState::Hovered;
    if (name == "pressed")  return WidgetState::Pressed;
    if (name == "disabled") return WidgetState::Disabled;
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// StyleTable — trivial map operations
// ---------------------------------------------------------------------------
void StyleTable::set_style(const std::string& id, UIStyle style) {
    styles_[id] = std::move(style);
}

bool StyleTable::contains(const std::string& id) const {
    return styles_.find(id) != styles_.end();
}

std::size_t StyleTable::size() const {
    return styles_.size();
}

// ---------------------------------------------------------------------------
// StyleTable::lookup (R6)
// ---------------------------------------------------------------------------
// Contract:
//  - empty style_id            -> std::nullopt                      (R6.5)
//  - non-empty id absent       -> ResolvedStyle{default, default}   (R6.3)
//  - present id, color stored  -> the stored color for that state   (R6.1, R6.2)
//  - present id, color missing -> UI_DEFAULT_COLOR for that channel (R5.7)
// Deterministic: identical arguments always yield identical results. (R6.7)
std::optional<ResolvedStyle> StyleTable::lookup(const std::string& style_id,
                                                WidgetState state) const {
    // An empty style_id means "this widget declares no style"; the caller
    // supplies its own fallback, so we return no result. (R6.5)
    if (style_id.empty()) {
        return std::nullopt;
    }

    // A non-empty id that is not in the table is a data error the renderer
    // must survive by drawing with the visible default sentinel. (R6.3)
    auto it = styles_.find(style_id);
    if (it == styles_.end()) {
        return ResolvedStyle{UI_DEFAULT_COLOR, UI_DEFAULT_COLOR};
    }

    // Present id: resolve the stored colors for the requested state, falling
    // back to UI_DEFAULT_COLOR for any color that was omitted. (R6.1, R6.2, R5.7)
    const UIStyle::StateColors& colors = it->second.states[static_cast<int>(state)];
    ResolvedStyle resolved;
    resolved.bg   = colors.bg.value_or(UI_DEFAULT_COLOR);
    resolved.text = colors.text.value_or(UI_DEFAULT_COLOR);
    return resolved;
}

// ---------------------------------------------------------------------------
// parse_ui_styles helpers
// ---------------------------------------------------------------------------
namespace {

// Parse a single color value into a Color. A color is accepted ONLY if it is a
// JSON array of exactly four elements, each an integer in [0, 255]. Anything
// else (wrong type, wrong channel count, non-integer channel, out-of-range
// channel) yields std::nullopt — never clamped, never throwing. (R5.6)
std::optional<Color> parse_color(const json& value) {
    if (!value.is_array() || value.size() != 4) {
        return std::nullopt;
    }
    Color color{};
    for (std::size_t i = 0; i < 4; ++i) {
        const json& channel = value[i];
        // Reject non-integer channels (floats, strings, bools, null, etc.).
        if (!channel.is_number_integer()) {
            return std::nullopt;
        }
        // Use a signed 64-bit read so negative values are detected before the
        // range check rather than wrapping through an unsigned conversion.
        std::int64_t raw = channel.get<std::int64_t>();
        if (raw < 0 || raw > 255) {
            return std::nullopt;
        }
        std::uint8_t component = static_cast<std::uint8_t>(raw);
        switch (i) {
            case 0: color.r = component; break;
            case 1: color.g = component; break;
            case 2: color.b = component; break;
            case 3: color.a = component; break;
        }
    }
    return color;
}

// Parse one named style's per-state color entries into a UIStyle. For each of
// the four state names present in the style object, parse its "bg" and "text"
// colors; unknown state keys are ignored. A missing or malformed color is left
// as std::nullopt so a later lookup applies the default color. (R5.2, R5.3, R5.6)
UIStyle parse_style(const json& style_obj) {
    UIStyle style;
    for (auto& [state_name, state_value] : style_obj.items()) {
        std::optional<WidgetState> state = parse_widget_state(state_name);
        if (!state.has_value()) {
            // Ignore state-name keys that are not one of the four valid states.
            continue;
        }
        if (!state_value.is_object()) {
            // A state entry that is not an object carries no usable colors.
            continue;
        }
        UIStyle::StateColors& colors = style.states[static_cast<int>(*state)];
        if (state_value.contains("bg")) {
            colors.bg = parse_color(state_value["bg"]);
        }
        if (state_value.contains("text")) {
            colors.text = parse_color(state_value["text"]);
        }
    }
    return style;
}

}  // namespace

// ---------------------------------------------------------------------------
// parse_ui_styles (R5.1–R5.6)
// ---------------------------------------------------------------------------
// Build a StyleTable from the "ui_styles" JSON value:
//  - value not an object (or null) -> empty table, no throw  (R5.4, R5.5)
//  - each key names one UIStyle; each style's present states are parsed
//  - malformed colors are omitted (left nullopt), never clamped, never thrown
StyleTable parse_ui_styles(const json& ui_styles_value) {
    StyleTable table;

    // A non-object value (including null) yields an empty table. (R5.4, R5.5)
    if (!ui_styles_value.is_object()) {
        return table;
    }

    for (auto& [style_id, style_value] : ui_styles_value.items()) {
        // A style entry that is not an object contributes an empty UIStyle:
        // every state resolves to the default color at lookup time.
        if (!style_value.is_object()) {
            table.set_style(style_id, UIStyle{});
            continue;
        }
        table.set_style(style_id, parse_style(style_value));
    }

    return table;
}
