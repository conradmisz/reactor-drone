/**
 * Unit tests for the widget style data model and parsing (ui_style.hpp).
 *
 * Verifies the pure data + pure logic style system that backs UIRenderSystem,
 * with NO SDL or rendering dependency:
 *
 * - StyleTable::set_style + lookup round-trip: a style whose four states each
 *   carry DISTINCT bg/text colors resolves to exactly the stored colors,
 *   per state (R12.1).
 * - lookup on an empty style_id returns std::nullopt (R6.5).
 * - lookup of a non-empty id absent from the table returns
 *   ResolvedStyle{UI_DEFAULT_COLOR, UI_DEFAULT_COLOR} for every state (R6.3).
 * - parse_widget_state maps the four exact, case-sensitive names and rejects
 *   anything else (R6.6).
 * - parse_ui_styles on a non-object json (array / null) yields an empty table
 *   (R5.4, R5.5); on a valid object it yields the expected styles/colors.
 * - A style present but missing one state's bg resolves that bg to
 *   UI_DEFAULT_COLOR (R5.7).
 *
 * Color has uint8_t r,g,b,a and no operator==, so every color assertion
 * compares the four channels individually for exact equality.
 *
 * Requirements tested: 12.1, 6.3, 6.5, 6.6, 5.4, 5.5, 5.7
 */

#include <catch2/catch_test_macros.hpp>

#include "engine/ui_style.hpp"
#include "engine/ecs/components.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Assert exact channel-for-channel equality of two Colors. Color has no
// operator==, so we compare r, g, b, a directly (uint8_t each).
static void require_color_eq(const Color& got, const Color& expected) {
    REQUIRE(got.r == expected.r);
    REQUIRE(got.g == expected.g);
    REQUIRE(got.b == expected.b);
    REQUIRE(got.a == expected.a);
}

// Build a StateColors carrying a concrete bg and text color.
static UIStyle::StateColors make_state(Color bg, Color text) {
    UIStyle::StateColors sc;
    sc.bg = bg;
    sc.text = text;
    return sc;
}

// ---------------------------------------------------------------------------
// StyleTable lookup round-trip with distinct per-state colors (R12.1)
// ---------------------------------------------------------------------------

/**
 * Test: a style whose four states each hold DISTINCT bg/text colors resolves
 * to exactly the stored colors, asserted per state.
 *
 * Each of the four WidgetState entries is given its own bg and text color with
 * no overlap, so a per-state lookup must return precisely the color stored for
 * that state (and not bleed in from another state). (R12.1, R6.1, R6.2)
 */
TEST_CASE("StyleTable: lookup returns the exact stored bg/text per state",
          "[Engine][ui][unit]") {
    // Eight distinct colors, one (bg,text) pair per state.
    const Color normal_bg{10, 20, 30, 40};
    const Color normal_text{11, 21, 31, 41};
    const Color hovered_bg{50, 60, 70, 80};
    const Color hovered_text{51, 61, 71, 81};
    const Color pressed_bg{90, 100, 110, 120};
    const Color pressed_text{91, 101, 111, 121};
    const Color disabled_bg{130, 140, 150, 160};
    const Color disabled_text{131, 141, 151, 161};

    UIStyle style{};
    style.states[static_cast<int>(WidgetState::Normal)]   = make_state(normal_bg, normal_text);
    style.states[static_cast<int>(WidgetState::Hovered)]  = make_state(hovered_bg, hovered_text);
    style.states[static_cast<int>(WidgetState::Pressed)]  = make_state(pressed_bg, pressed_text);
    style.states[static_cast<int>(WidgetState::Disabled)] = make_state(disabled_bg, disabled_text);

    StyleTable table;
    table.set_style("primary", std::move(style));

    REQUIRE(table.contains("primary"));
    REQUIRE(table.size() == 1);

    SECTION("Normal state resolves to its stored bg and text") {
        auto resolved = table.lookup("primary", WidgetState::Normal);
        REQUIRE(resolved.has_value());
        require_color_eq(resolved->bg, normal_bg);
        require_color_eq(resolved->text, normal_text);
    }

    SECTION("Hovered state resolves to its stored bg and text") {
        auto resolved = table.lookup("primary", WidgetState::Hovered);
        REQUIRE(resolved.has_value());
        require_color_eq(resolved->bg, hovered_bg);
        require_color_eq(resolved->text, hovered_text);
    }

    SECTION("Pressed state resolves to its stored bg and text") {
        auto resolved = table.lookup("primary", WidgetState::Pressed);
        REQUIRE(resolved.has_value());
        require_color_eq(resolved->bg, pressed_bg);
        require_color_eq(resolved->text, pressed_text);
    }

    SECTION("Disabled state resolves to its stored bg and text") {
        auto resolved = table.lookup("primary", WidgetState::Disabled);
        REQUIRE(resolved.has_value());
        require_color_eq(resolved->bg, disabled_bg);
        require_color_eq(resolved->text, disabled_text);
    }
}

// ---------------------------------------------------------------------------
// Empty style_id -> std::nullopt (R6.5)
// ---------------------------------------------------------------------------

/**
 * Test: lookup with an empty style_id returns std::nullopt for every state.
 *
 * An empty style_id means "this widget declares no style"; the caller supplies
 * its own fallback, so lookup returns no result regardless of state. (R6.5)
 */
TEST_CASE("StyleTable: empty style_id returns nullopt", "[Engine][ui][unit]") {
    StyleTable table;

    // Even with a populated table, an empty id yields nullopt.
    UIStyle style{};
    style.states[static_cast<int>(WidgetState::Normal)] =
        make_state(Color{1, 2, 3, 4}, Color{5, 6, 7, 8});
    table.set_style("primary", std::move(style));

    REQUIRE_FALSE(table.lookup("", WidgetState::Normal).has_value());
    REQUIRE_FALSE(table.lookup("", WidgetState::Hovered).has_value());
    REQUIRE_FALSE(table.lookup("", WidgetState::Pressed).has_value());
    REQUIRE_FALSE(table.lookup("", WidgetState::Disabled).has_value());
}

// ---------------------------------------------------------------------------
// Non-empty id absent from table -> default sentinel for every state (R6.3)
// ---------------------------------------------------------------------------

/**
 * Test: a non-empty id not present in the table resolves to
 * ResolvedStyle{UI_DEFAULT_COLOR, UI_DEFAULT_COLOR} for every state.
 *
 * A missing style is a data error the renderer survives by drawing with the
 * visible default sentinel (magenta) for both bg and text. (R6.3)
 */
TEST_CASE("StyleTable: absent non-empty id resolves to UI_DEFAULT_COLOR for every state",
          "[Engine][ui][unit]") {
    StyleTable table;  // empty table

    const WidgetState states[] = {
        WidgetState::Normal, WidgetState::Hovered,
        WidgetState::Pressed, WidgetState::Disabled};

    for (WidgetState state : states) {
        auto resolved = table.lookup("does_not_exist", state);
        REQUIRE(resolved.has_value());
        require_color_eq(resolved->bg, UI_DEFAULT_COLOR);
        require_color_eq(resolved->text, UI_DEFAULT_COLOR);
    }
}

// ---------------------------------------------------------------------------
// parse_widget_state (R6.6)
// ---------------------------------------------------------------------------

/**
 * Test: parse_widget_state maps the four exact, case-sensitive names and
 * rejects unknown names and the empty string.
 *
 * "normal"/"hovered"/"pressed"/"disabled" map to their enum; anything else
 * (e.g. "bogus", "") yields std::nullopt. (R6.6)
 */
TEST_CASE("parse_widget_state: valid names map, invalid names yield nullopt",
          "[Engine][ui][unit]") {
    SECTION("valid state names map to the matching enum") {
        auto normal = parse_widget_state("normal");
        REQUIRE(normal.has_value());
        REQUIRE(*normal == WidgetState::Normal);

        auto hovered = parse_widget_state("hovered");
        REQUIRE(hovered.has_value());
        REQUIRE(*hovered == WidgetState::Hovered);

        auto pressed = parse_widget_state("pressed");
        REQUIRE(pressed.has_value());
        REQUIRE(*pressed == WidgetState::Pressed);

        auto disabled = parse_widget_state("disabled");
        REQUIRE(disabled.has_value());
        REQUIRE(*disabled == WidgetState::Disabled);
    }

    SECTION("invalid names yield nullopt") {
        REQUIRE_FALSE(parse_widget_state("bogus").has_value());
        REQUIRE_FALSE(parse_widget_state("").has_value());
    }
}

// ---------------------------------------------------------------------------
// parse_ui_styles on a non-object json -> empty table (R5.4, R5.5)
// ---------------------------------------------------------------------------

/**
 * Test: parse_ui_styles on a non-object value (array or null) yields an empty
 * table with no throw.
 *
 * A "ui_styles" value that is not a JSON object carries no styles. (R5.4, R5.5)
 */
TEST_CASE("parse_ui_styles: non-object json yields an empty table",
          "[Engine][ui][unit]") {
    SECTION("json array -> empty table") {
        json arr = json::array({1, 2, 3});
        StyleTable table = parse_ui_styles(arr);
        REQUIRE(table.size() == 0);
    }

    SECTION("json null -> empty table") {
        json null_value = nullptr;
        StyleTable table = parse_ui_styles(null_value);
        REQUIRE(table.size() == 0);
    }
}

// ---------------------------------------------------------------------------
// parse_ui_styles on a valid object -> expected styles with correct colors
// ---------------------------------------------------------------------------

/**
 * Test: parse_ui_styles on a valid small object produces the expected styles,
 * and the parsed colors resolve to exactly the authored channel values.
 *
 * Two styles, each declaring states with four-integer-channel bg/text colors,
 * are parsed and looked up; lookup returns the exact authored colors. (R5.1-R5.3)
 */
TEST_CASE("parse_ui_styles: valid object yields expected styles with correct colors",
          "[Engine][ui][unit]") {
    json ui_styles = {
        {"primary", {
            {"normal",  {{"bg", {10, 20, 30, 255}}, {"text", {200, 210, 220, 255}}}},
            {"hovered", {{"bg", {40, 50, 60, 255}}, {"text", {201, 211, 221, 255}}}}
        }},
        {"secondary", {
            {"pressed", {{"bg", {70, 80, 90, 128}}, {"text", {15, 25, 35, 45}}}}
        }}
    };

    StyleTable table = parse_ui_styles(ui_styles);

    REQUIRE(table.size() == 2);
    REQUIRE(table.contains("primary"));
    REQUIRE(table.contains("secondary"));

    SECTION("primary normal colors parse exactly") {
        auto resolved = table.lookup("primary", WidgetState::Normal);
        REQUIRE(resolved.has_value());
        require_color_eq(resolved->bg, Color{10, 20, 30, 255});
        require_color_eq(resolved->text, Color{200, 210, 220, 255});
    }

    SECTION("primary hovered colors parse exactly") {
        auto resolved = table.lookup("primary", WidgetState::Hovered);
        REQUIRE(resolved.has_value());
        require_color_eq(resolved->bg, Color{40, 50, 60, 255});
        require_color_eq(resolved->text, Color{201, 211, 221, 255});
    }

    SECTION("secondary pressed colors parse exactly") {
        auto resolved = table.lookup("secondary", WidgetState::Pressed);
        REQUIRE(resolved.has_value());
        require_color_eq(resolved->bg, Color{70, 80, 90, 128});
        require_color_eq(resolved->text, Color{15, 25, 35, 45});
    }
}

// ---------------------------------------------------------------------------
// Present style missing a state's bg -> UI_DEFAULT_COLOR for that bg (R5.7)
// ---------------------------------------------------------------------------

/**
 * Test: a present style whose Normal state declares only "text" (no "bg")
 * resolves that state's bg to UI_DEFAULT_COLOR while text keeps its value.
 *
 * A missing/omitted color stays std::nullopt during parsing and is filled with
 * the default sentinel at lookup time, independently of the other color. (R5.7)
 */
TEST_CASE("parse_ui_styles: missing bg resolves to UI_DEFAULT_COLOR at lookup",
          "[Engine][ui][unit]") {
    json ui_styles = {
        {"partial", {
            {"normal", {{"text", {12, 34, 56, 78}}}}  // bg intentionally omitted
        }}
    };

    StyleTable table = parse_ui_styles(ui_styles);
    REQUIRE(table.contains("partial"));

    auto resolved = table.lookup("partial", WidgetState::Normal);
    REQUIRE(resolved.has_value());

    // bg was omitted -> default sentinel; text was present -> exact value.
    require_color_eq(resolved->bg, UI_DEFAULT_COLOR);
    require_color_eq(resolved->text, Color{12, 34, 56, 78});
}
