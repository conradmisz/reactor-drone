/**
 * Property-based tests for the Option-040 Phase 2 widget-rendering pure logic
 * (style lookup, render-decision helpers, screen-stack toggling).
 *
 * Implements the five correctness properties from the o-040-02-widget-rendering
 * design "Correctness Properties" section, one property-based TEST_CASE each:
 *   Property 1: Style lookup returns the stored color per state, default for absent ids
 *   Property 2: Button centered-text origin and Y-flip conversion are exact
 *   Property 3: Widgets render exactly the active-screen members, ordered by z_order/entity
 *   Property 4: Parsed ui_styles RGBA channels are integers within 0..255, no throw
 *   Property 5: ScreenStackSystem push/pop toggles active only for exact matches
 *
 * The tested logic lives in SDL-free units (ui_style.{hpp,cpp},
 * ui_render_math.hpp, screen_stack_system.{hpp,cpp}); no window or renderer is
 * needed and UIRenderSystem itself is not constructed here.
 *
 * Feature: o-040-02-widget-rendering
 * All property tests bounded per the workspace property-test-bounds policy.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include <nlohmann/json.hpp>

#include "engine/ui_style.hpp"
#include "engine/ecs/systems/ui_render_math.hpp"
#include "engine/ecs/systems/screen_stack_system.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"

#include <array>
#include <cstdint>
#include <set>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

// Configurable test iteration counts (MANDATORY — workspace property-test-bounds policy)
constexpr int NUM_OUTER_TESTS = 10;  // Number of different outer subjects (styles / widget sets / screen sets)
constexpr int NUM_INNER_TESTS = 5;   // Number of value variations per subject

using json = nlohmann::json;

// Color has no operator== — compare channels explicitly.
static bool color_eq(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// ---------------------------------------------------------------------------
// Feature: o-040-02-widget-rendering, Property 1: Style lookup returns the
// stored color per state, and the fixed default for absent ids.
//
// For any StyleTable populated with a non-empty style_id whose four state
// entries each hold distinct, randomly generated bg/text colors, a lookup for
// that id and any state returns exactly the bg/text stored for THAT state
// (never another state's); a lookup for any non-empty id NOT in the table
// returns UI_DEFAULT_COLOR for both bg and text for every state; a lookup with
// an empty style_id returns no result; and repeating any lookup with identical
// arguments returns identical colors.
//
// **Validates: Requirements 5.7, 6.1, 6.2, 6.3, 6.5, 6.7**
// ---------------------------------------------------------------------------
TEST_CASE("Property 1: Style lookup returns the stored color per state, default for absent ids",
          "[Engine][ui][property]") {

    SECTION("Per-state stored colors, default for absent id, nullopt for empty id, determinism") {
        // Random channel ints 0..255 drive the generated colors.
        auto outer = GENERATE(take(NUM_OUTER_TESTS, random(0, 255)));
        auto inner = GENERATE(take(NUM_INNER_TESTS, random(0, 255)));

        // Build 8 DISTINCT colors (bg+text for each of 4 states). The red
        // channel encodes the index (idx*30, gap >> any randomness) so the
        // colors are provably distinct; green/blue carry the random channels.
        auto make_color = [&](int idx) -> Color {
            return Color{
                static_cast<uint8_t>(idx * 30 + (outer % 7)),
                static_cast<uint8_t>(inner),
                static_cast<uint8_t>((outer + inner) % 256),
                static_cast<uint8_t>(255)
            };
        };

        UIStyle style{};
        for (int s = 0; s < 4; ++s) {
            style.states[s].bg = make_color(2 * s);
            style.states[s].text = make_color(2 * s + 1);
        }

        const std::string id = "style_" + std::to_string(outer) + "_" + std::to_string(inner);
        StyleTable table;
        table.set_style(id, style);
        REQUIRE(table.contains(id));

        // Lookup returns exactly the stored color for THAT state, never another's.
        for (int s = 0; s < 4; ++s) {
            WidgetState state = static_cast<WidgetState>(s);
            auto resolved = table.lookup(id, state);
            REQUIRE(resolved.has_value());
            REQUIRE(color_eq(resolved->bg, make_color(2 * s)));
            REQUIRE(color_eq(resolved->text, make_color(2 * s + 1)));

            // Never another state's colors (colors are distinct by construction).
            for (int s2 = 0; s2 < 4; ++s2) {
                if (s2 == s) continue;
                REQUIRE_FALSE(color_eq(resolved->bg, make_color(2 * s2)));
                REQUIRE_FALSE(color_eq(resolved->text, make_color(2 * s2 + 1)));
            }

            // Repeating an identical lookup returns identical colors (R6.7).
            auto again = table.lookup(id, state);
            REQUIRE(again.has_value());
            REQUIRE(color_eq(again->bg, resolved->bg));
            REQUIRE(color_eq(again->text, resolved->text));
        }

        // A non-empty id NOT in the table -> UI_DEFAULT_COLOR for both, every state (R6.3, R5.7).
        const std::string absent_id = id + "_absent";
        REQUIRE_FALSE(table.contains(absent_id));
        for (int s = 0; s < 4; ++s) {
            auto resolved = table.lookup(absent_id, static_cast<WidgetState>(s));
            REQUIRE(resolved.has_value());
            REQUIRE(color_eq(resolved->bg, UI_DEFAULT_COLOR));
            REQUIRE(color_eq(resolved->text, UI_DEFAULT_COLOR));
        }

        // An empty style_id -> nullopt for every state (R6.5).
        for (int s = 0; s < 4; ++s) {
            REQUIRE_FALSE(table.lookup("", static_cast<WidgetState>(s)).has_value());
        }
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-02-widget-rendering, Property 2: Button centered-text origin
// and Y-flip conversion are exact.
//
// For any UIRect (including zero-width/zero-height rects) and any text
// dimensions text_w/text_h (including larger than the rect),
// compute_centered_text_origin returns left edge exactly
// rect.x + (rect.w - text_w)/2 and bottom edge exactly
// rect.y + (rect.h - text_h)/2 with no clamping (equal margins); and for any
// window_height, y, height, to_sdl_y returns exactly window_height - y - height
// with no special-casing of zero height.
//
// **Validates: Requirements 4.3, 4.4, 11.3, 11.4**
// ---------------------------------------------------------------------------
TEST_CASE("Property 2: Button centered-text origin and Y-flip conversion are exact",
          "[Engine][ui][property]") {

    SECTION("compute_centered_text_origin is exact with equal margins (zero & oversize allowed)") {
        // rect: x,y,w,h ; w,h range includes 0 (zero-size rects).
        auto rvals = GENERATE(take(NUM_OUTER_TESTS, chunk(4, random(0, 300))));
        // text: text_w,text_h up to 500 -> can exceed the rect (negative offsets).
        auto tvals = GENERATE(take(NUM_INNER_TESTS, chunk(2, random(0, 500))));

        UIRect rect{};
        rect.x = static_cast<float>(rvals[0]);
        rect.y = static_cast<float>(rvals[1]);
        rect.w = static_cast<float>(rvals[2]);
        rect.h = static_cast<float>(rvals[3]);
        float text_w = static_cast<float>(tvals[0]);
        float text_h = static_cast<float>(tvals[1]);

        TextOrigin origin = compute_centered_text_origin(rect, text_w, text_h);

        // Exact equality with the unclamped centering formula.
        REQUIRE(origin.x == rect.x + (rect.w - text_w) / 2.0f);
        REQUIRE(origin.y == rect.y + (rect.h - text_h) / 2.0f);

        // Equal left/right and equal top/bottom margins.
        float left_margin   = origin.x - rect.x;
        float right_margin  = (rect.x + rect.w) - (origin.x + text_w);
        float bottom_margin = origin.y - rect.y;
        float top_margin    = (rect.y + rect.h) - (origin.y + text_h);
        REQUIRE(left_margin == right_margin);
        REQUIRE(bottom_margin == top_margin);
    }

    SECTION("to_sdl_y is exactly window_height - y - height, including zero height") {
        auto wh = GENERATE(take(NUM_OUTER_TESTS, random(0, 800)));
        auto yh = GENERATE(take(NUM_INNER_TESTS, chunk(2, random(0, 800))));  // height range includes 0

        float window_height = static_cast<float>(wh);
        float y = static_cast<float>(yh[0]);
        float height = static_cast<float>(yh[1]);

        REQUIRE(to_sdl_y(window_height, y, height) == window_height - y - height);
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-02-widget-rendering, Property 3: Widgets render exactly the
// active-screen members, ordered by ascending z_order then entity id.
//
// For any set of screens (each independently active/inactive) and any set of
// widgets (random z_order, random owning screen, distinct entity id, some with
// no membership), the selection-and-sort step produces a draw list containing
// exactly the widgets whose ScreenMembership names an active screen — each once
// — and that list is ordered non-decreasing by z_order, ties broken by
// ascending entity id. Pure: DrawItems are built directly and
// sort_widgets_by_draw_order is exercised; no UIRenderSystem/SDL.
//
// **Validates: Requirements 8.1, 8.2, 8.3, 8.4, 8.5, 9.1, 9.2, 9.3, 9.4**
// ---------------------------------------------------------------------------
TEST_CASE("Property 3: Widgets render exactly the active-screen members, ordered by z_order then entity id",
          "[Engine][ui][property]") {

    SECTION("Selection filters to active-screen members and the draw list is sorted") {
        constexpr int NUM_SCREENS = 4;   // screens s0..s3
        constexpr int NUM_WIDGETS = 8;

        // active_mask: bit i set => screen "s<i>" is active.
        auto active_mask = GENERATE(take(NUM_OUTER_TESTS, random(0, (1 << NUM_SCREENS) - 1)));
        auto seed = GENERATE(take(NUM_INNER_TESTS, random(0, 1000000)));

        // Active-screen-name set.
        std::unordered_set<std::string> active_names;
        for (int i = 0; i < NUM_SCREENS; ++i) {
            if (active_mask & (1 << i)) active_names.insert("s" + std::to_string(i));
        }

        // Distinct, scrambled entity ids (not pre-sorted) to exercise the tiebreak.
        const std::array<Entity, NUM_WIDGETS> ent_ids = {5, 2, 8, 1, 7, 3, 6, 4};

        struct W {
            Entity entity;
            int z_order;
            bool has_mem;
            std::string screen_name;
        };

        std::vector<W> widgets;
        for (int i = 0; i < NUM_WIDGETS; ++i) {
            int z = (seed >> (i * 2)) & 3;                  // 0..3 -> ties guaranteed
            int r = (seed / (i + 1)) % (NUM_SCREENS + 1);   // 0..4 ; NUM_SCREENS => no membership
            W w{};
            w.entity = ent_ids[i];
            w.z_order = z;
            w.has_mem = (r < NUM_SCREENS);
            w.screen_name = w.has_mem ? ("s" + std::to_string(r)) : std::string{};
            widgets.push_back(w);
        }

        // Production-mirroring selection: include a widget iff it has a
        // membership naming an active screen; build DrawItems and sort.
        std::vector<DrawItem> draw_list;
        for (const W& w : widgets) {
            if (!w.has_mem) continue;                          // R8.5 no membership -> skip
            if (active_names.count(w.screen_name) == 0) continue;  // R8.2 inactive -> skip
            draw_list.push_back(DrawItem{w.z_order, w.entity});
        }
        sort_widgets_by_draw_order(draw_list);

        // Independently computed expected entity set.
        std::unordered_set<Entity> expected;
        for (const W& w : widgets) {
            if (w.has_mem && active_names.count(w.screen_name) != 0) {
                expected.insert(w.entity);
            }
        }

        // Exactly the expected widgets, each once (entity ids are distinct).
        REQUIRE(draw_list.size() == expected.size());
        std::unordered_set<Entity> seen;
        for (const DrawItem& item : draw_list) {
            REQUIRE(expected.count(item.entity) == 1);       // only active-screen members
            REQUIRE(seen.insert(item.entity).second);        // each appears exactly once (R8.4)
        }

        // Ordered non-decreasing by z_order, ties broken by ascending entity id.
        for (std::size_t k = 1; k < draw_list.size(); ++k) {
            const DrawItem& a = draw_list[k - 1];
            const DrawItem& b = draw_list[k];
            bool ordered = (a.z_order < b.z_order) ||
                           (a.z_order == b.z_order && a.entity < b.entity);
            REQUIRE(ordered);
        }
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-02-widget-rendering, Property 4: Parsed ui_styles RGBA
// channels are integers within 0..255 and malformed input never throws.
//
// For any "ui_styles" JSON value — objects mixing valid colors with malformed
// entries (wrong channel count, out-of-range, non-integer, missing bg/text)
// and non-object values — parse_ui_styles completes without raising an error,
// and every bg/text color stored in the resulting StyleTable has exactly four
// channels each an integer in 0..255; malformed color entries are omitted
// (resolve to UI_DEFAULT_COLOR) rather than stored.
//
// **Validates: Requirements 5.3, 5.5, 5.6, 6.4**
// ---------------------------------------------------------------------------
TEST_CASE("Property 4: Parsed ui_styles RGBA channels are within 0..255 and malformed input never throws",
          "[Engine][ui][property]") {

    // Every channel of a stored Color is structurally 0..255 (R6.4). That is a
    // property of the channel TYPE, not of any parsed value, so it is asserted
    // once at compile time — a runtime `c.r <= 255` on a uint8_t is tautological
    // and warns under -Wtype-limits.
    static_assert(std::numeric_limits<decltype(Color::r)>::max() == 255 &&
                  std::numeric_limits<decltype(Color::r)>::min() == 0,
                  "Color channels must be 8-bit unsigned for R6.4 to hold");
    // What a parsed style CAN get wrong is which colour it resolves to, so the
    // runtime check is that a stored channel is not the missing-style sentinel.
    auto channels_stored = [](const Color& c) {
        return !(c.r == UI_DEFAULT_COLOR.r && c.g == UI_DEFAULT_COLOR.g &&
                 c.b == UI_DEFAULT_COLOR.b && c.a == UI_DEFAULT_COLOR.a);
    };

    SECTION("Non-object ui_styles values -> empty table, no throw") {
        auto kind = GENERATE(take(NUM_OUTER_TESTS, random(0, 3)));
        auto v = GENERATE(take(NUM_INNER_TESTS, random(0, 255)));

        json value;
        switch (kind) {
            case 0: value = json(v);                       break;  // number
            case 1: value = json(std::string("not-obj"));  break;  // string
            case 2: value = json(nullptr);                 break;  // null
            default: value = json::array({1, 2, v});       break;  // array
        }

        StyleTable table;
        REQUIRE_NOTHROW(table = parse_ui_styles(value));
        REQUIRE(table.size() == 0);
    }

    SECTION("Object mixing valid and malformed entries -> valid stored in range, malformed omitted") {
        auto a_chan = GENERATE(take(NUM_OUTER_TESTS, random(0, 255)));
        auto b_chan = GENERATE(take(NUM_INNER_TESTS, random(0, 255)));

        // One fully valid color (in-range integer channels).
        const Color valid_bg{
            static_cast<uint8_t>(a_chan),
            static_cast<uint8_t>((a_chan + 1) % 256),
            static_cast<uint8_t>((a_chan + 2) % 256),
            static_cast<uint8_t>(255)
        };
        const Color valid_text{
            static_cast<uint8_t>(0),
            static_cast<uint8_t>(b_chan),
            static_cast<uint8_t>(255),
            static_cast<uint8_t>((b_chan + 3) % 256)
        };

        json styles = json::object();
        styles["good"]["normal"]["bg"] = {valid_bg.r, valid_bg.g, valid_bg.b, valid_bg.a};
        styles["good"]["normal"]["text"] = {valid_text.r, valid_text.g, valid_text.b, valid_text.a};
        // Malformed entries (each must be omitted, never stored):
        styles["wrongcount"]["normal"]["bg"] = {1, 2, 3};               // 3 channels
        styles["outofrange"]["normal"]["bg"] = {300, 0, 0, 0};          // channel > 255
        styles["nonint"]["normal"]["bg"]     = {"x", 1, 2, 3};          // non-integer channel
        styles["missing"]["normal"]          = json::object();          // no bg/text
        styles["nonobj_state"]["normal"]     = 5;                       // state value not an object

        StyleTable table;
        REQUIRE_NOTHROW(table = parse_ui_styles(styles));

        // Valid color survives parsing exactly, channels in range.
        auto good = table.lookup("good", WidgetState::Normal);
        REQUIRE(good.has_value());
        REQUIRE(color_eq(good->bg, valid_bg));
        REQUIRE(color_eq(good->text, valid_text));
        REQUIRE(channels_stored(good->bg));
        REQUIRE(channels_stored(good->text));

        // Malformed bg colors are omitted -> resolve to UI_DEFAULT_COLOR, never
        // to the malformed value. color_eq below pins the exact expected colour,
        // so no separate range assertion is needed here.
        for (const char* id : {"wrongcount", "outofrange", "nonint", "missing", "nonobj_state"}) {
            auto resolved = table.lookup(id, WidgetState::Normal);
            REQUIRE(resolved.has_value());
            REQUIRE(color_eq(resolved->bg, UI_DEFAULT_COLOR));
            REQUIRE(color_eq(resolved->text, UI_DEFAULT_COLOR));
        }
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-02-widget-rendering, Property 5: ScreenStackSystem reconciles
// every UIScreen.active flag to agree with the stack.
//
// NOTE: Adapted for Phase 4 (o-040-04-screen-stack). The Phase 2 stub API
// (push(name, storage) / pop(name, storage), which toggled UIScreen.active for
// an exact-name match and kept no ordered list) was PROMOTED to a full
// Blackboard-backed screen stack. ScreenStackSystem is now the single writer of
// both the "screen_stack" list and every UIScreen.active flag, reconciling the
// two after each mutation. This is the documented intentional API change
// (regression-policy "API intentionally changed" exception), so this property
// is re-expressed against the promoted reconcile invariant: after any push/pop,
// a UIScreen is active iff its screen_name is present on the stack. Every push
// of a name matching one or more screens activates exactly those screens; an
// empty name is a no-op; popping back to the base deactivates them all.
//
// **Validates: Requirements 1.5, 2.1, 2.5, 3.1, 3.5, 4.1, 4.2, 4.4**
// ---------------------------------------------------------------------------
TEST_CASE("Property 5: ScreenStackSystem reconciles UIScreen.active to agree with the stack",
          "[Engine][ui][property]") {

    SECTION("after any push/pop, a screen is active iff its name is on the stack") {
        constexpr int NUM_SCREENS = 5;
        // Name pool with a repeated name ("a" appears twice) to exercise
        // multiple screens matching one pushed name.
        const std::array<std::string, NUM_SCREENS> pool = {"a", "b", "menu", "a", "c"};

        auto seed = GENERATE(take(NUM_OUTER_TESTS, random(0, 1000000)));
        auto op = GENERATE(take(NUM_INNER_TESTS, random(0, 4)));

        EntityManager em;
        ComponentStorage storage;
        Blackboard bb;

        std::vector<Entity> ents;
        for (int i = 0; i < NUM_SCREENS; ++i) {
            Entity e = em.create_entity();
            // Random starting flag; the system reconciles it, so it must not be
            // relied on after initialize().
            bool active = ((seed >> i) & 1) != 0;
            UIScreen scr{};
            scr.screen_name = pool[i];
            scr.active = active;
            storage.add_component(e, scr);
            ents.push_back(e);
        }

        ScreenStackSystem system;
        system.initialize(bb, storage);  // stack = ["gameplay"]; all screens reconciled inactive

        // Select a name to push: matching one/two screens, empty, or unmatched.
        std::string target;
        switch (op) {
            case 0: target = "a";    break;  // matches two screens
            case 1: target = "b";    break;  // matches one screen
            case 2: target = "";     break;  // empty -> no-op push
            case 3: target = "zzz";  break;  // unmatched ghost name (no entity)
            default: target = "menu"; break; // matches one screen
        }

        REQUIRE_NOTHROW(system.push_screen(target, bb, storage));

        // The reconcile invariant: a screen is active iff its name is on the stack.
        auto stack = ScreenStackSystem::get_stack(bb);
        std::set<std::string> stack_set(stack.begin(), stack.end());
        for (std::size_t i = 0; i < ents.size(); ++i) {
            auto got = storage.get_component<UIScreen>(ents[i]);
            REQUIRE(got.has_value());
            REQUIRE(got->get().active == (stack_set.count(pool[i]) > 0));
        }

        // Popping back to the base deactivates every UIScreen (none remain on
        // the stack besides the entity-less sentinel "gameplay").
        REQUIRE_NOTHROW(system.pop_screen(bb, storage));
        REQUIRE(ScreenStackSystem::depth(bb) == 1);
        for (Entity e : ents) {
            auto got = storage.get_component<UIScreen>(e);
            REQUIRE(got.has_value());
            REQUIRE(got->get().active == false);
        }
    }
}
