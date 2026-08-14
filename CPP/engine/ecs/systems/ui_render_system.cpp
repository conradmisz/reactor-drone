/**
 * UIRenderSystem implementation
 *
 * Draws three non-interactive widget kinds — panels (filled rect + 1px border),
 * labels (text via a ResourceManager font), and buttons (filled rect + centered
 * label text). It resolves each widget's WidgetState (always Normal in this
 * phase, since nothing mutates UIState), looks up colors in the StyleTable
 * stored on the Blackboard, filters widgets to those on active screens, and
 * draws them in ascending z_order (entity-id tiebreak).
 *
 * This is a thin SDL shell: every input-varying decision (style lookup,
 * widget-state resolution, centered-text origin, z-order sort, the Y-flip)
 * lives in the pure helpers (ui_style.hpp / ui_render_math.hpp) that the unit
 * and property tests exercise directly. SDL draw calls here are verified by the
 * integration build and code review.
 *
 * Bottom-left origin is preserved: all widget Y conversions go through the
 * shared to_sdl_y() formula (sdl_y = window_height - y - height), the same
 * formula RenderSystem::draw_entity() uses. No Y-flip is performed anywhere
 * else and no UIState is mutated (Phase 2 is non-interactive).
 *
 * Added in Phase 2 (o-040-02-widget-rendering).
 */

#include "engine/ecs/systems/ui_render_system.hpp"
#include "engine/ecs/systems/ui_render_math.hpp"
#include "engine/ui_style.hpp"
#include "engine/resource_manager.hpp"

#include <SDL3_ttf/SDL_ttf.h>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

// UIElement carries no font field, so UIRenderSystem uses a fixed default font
// obtained through ResourceManager. "default.ttf" ships in assets/fonts/ and is
// the same default the Text component / HUDSystem rely on.
constexpr char  UI_FONT_NAME[] = "default.ttf";
constexpr float UI_FONT_SIZE   = 24.0f;

// Inner padding, in DESIGN-canvas units, kept between a widget's edge and its
// text. A caption that touches its own border reads as broken even when it
// technically fits, which is half of what the "too big for the box" report was.
constexpr float LABEL_PAD  = 0.0f;   // labels are their own box; no border to crowd
constexpr float BUTTON_PAD = 10.0f;

// Convert the engine-wide Color (uint8_t RGBA) to SDL_Color for TTF rendering.
inline SDL_Color to_sdl_color(const Color& c) {
    return SDL_Color{c.r, c.g, c.b, c.a};
}

} // namespace

UIRenderSystem::UIRenderSystem(SDL_Renderer* renderer, ResourceManager& rm,
                               int window_width, int window_height)
    : renderer_(renderer), resource_manager_(rm),
      window_width_(window_width), window_height_(window_height) {
}

void UIRenderSystem::render(const ComponentStorage& storage, Blackboard& blackboard) {
    // v2: render-local clock for pulse_hz widgets. Accumulated here rather than
    // read from a Blackboard key because it is purely presentational — the sim
    // must not be able to observe it, or a replay could diverge on it.
    // Advanced BEFORE the no-active-screen early-out so the phase does not
    // freeze while menus are closed.
    elapsed_ += static_cast<float>(blackboard.get_or<double>("delta_time", 0.0));

    // Every fill below is alpha-composited, so the renderer's draw blend mode
    // must be BLEND. It is set HERE and not inherited: SDL's default is NONE,
    // under which an alpha-0 fill writes SOLID BLACK instead of nothing — a
    // "rim, no fill" widget then paints over whatever it frames. Until v3 Tier
    // 9 this happened to work only because additive PARTICLES reset the mode to
    // BLEND on their way past (render_system draw_entity's colour path); the
    // frame a particle failed to draw, the dash button's icon vanished behind
    // its own frame. Don't rely on another system's leftovers.
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    // 1. Collect active screen names. (R8.1)
    std::unordered_set<std::string> active_screens;
    for (Entity screen_entity : storage.entities_with_component<UIScreen>()) {
        auto screen_opt = storage.get_component<UIScreen>(screen_entity);
        if (!screen_opt.has_value()) continue;
        const UIScreen& screen = screen_opt->get();
        if (screen.active) {
            active_screens.insert(screen.screen_name);
        }
    }

    // If no screen is active, complete the frame without drawing. (R1.5)
    if (active_screens.empty()) {
        return;
    }

    // 2. Build the draw list — iterate EVERY UIElement entity (R1.4), keeping
    //    only those whose ScreenMembership names an active screen.
    std::vector<DrawItem> items;
    for (Entity entity : storage.entities_with_component<UIElement>()) {
        // No membership -> never rendered. (R8.5)
        if (!storage.has_component<ScreenMembership>(entity)) continue;
        auto membership_opt = storage.get_component<ScreenMembership>(entity);
        if (!membership_opt.has_value()) continue;
        const ScreenMembership& membership = membership_opt->get();

        // Membership's screen must be active. (R8.2, R8.3, R8.4)
        if (active_screens.find(membership.screen_name) == active_screens.end()) {
            continue;
        }

        auto element_opt = storage.get_component<UIElement>(entity);
        if (!element_opt.has_value()) continue;
        const UIElement& element = element_opt->get();

        items.push_back(DrawItem{element.z_order, entity});
    }

    // 3. Sort: ascending z_order, ties broken by ascending entity id. (R9)
    sort_widgets_by_draw_order(items);

    // 4. Resolve the StyleTable from the Blackboard. A missing key or null
    //    pointer is treated as an empty table -> all default colors. (R5.7)
    auto table = blackboard.get_or<std::shared_ptr<StyleTable>>("ui_styles", nullptr);

    // 4b. Window-size independence: map the fixed design canvas to the live
    //     window (uniform fit + center). Every widget rect and the slider knob /
    //     font size are scaled by this so menus look identical at any window size.
    const UICanvasTransform xform =
        ui_canvas_transform(static_cast<float>(window_width_),
                            static_cast<float>(window_height_));

    // 5. Draw each widget by element_type.
    for (const DrawItem& item : items) {
        auto element_opt = storage.get_component<UIElement>(item.entity);
        if (!element_opt.has_value()) continue;
        const UIElement& element = element_opt->get();

        // Resolve the interaction state. UIState is never mutated this phase,
        // so this is Normal in practice; precedence is implemented for reuse.
        WidgetState state = WidgetState::Normal;
        float       value = 0.0f;  // Phase 5: slider/checkbox read UIState.value.
        bool        focused = false;  // Phase 7: keyboard focus indicator.
        if (storage.has_component<UIState>(item.entity)) {
            auto ui_state_opt = storage.get_component<UIState>(item.entity);
            if (ui_state_opt.has_value()) {
                state = resolve_widget_state(ui_state_opt->get());
                value = ui_state_opt->get().value;  // R10.2, R11.2
                focused = ui_state_opt->get().focused;
            }
        } else {
            state = resolve_widget_state();
        }

        // Resolve colors. nullopt (empty id, missing table, or absent id) ->
        // the fixed default sentinel for both bg and text. (R2.2, R5.7, R6.3)
        Color bg_color   = UI_DEFAULT_COLOR;
        Color text_color = UI_DEFAULT_COLOR;
        if (table) {
            std::optional<ResolvedStyle> resolved = table->lookup(element.style_id, state);
            if (resolved.has_value()) {
                bg_color   = resolved->bg;
                text_color = resolved->text;
            }
        }

        // v2: a widget with pulse_hz > 0 breathes its alpha. Applied to the
        // resolved colours only — never to `rect` — so the drawn area and the
        // hit-test area stay identical. Widgets that never opted in (the default
        // pulse_hz == 0) get an exact 1.0 multiplier and are byte-unchanged.
        if (element.pulse_hz > 0.0f) {
            const float s = pulse_alpha_scale(element.pulse_hz, elapsed_);
            bg_color   = apply_alpha_scale(bg_color, s);
            text_color = apply_alpha_scale(text_color, s);
        }

        // Widget rect mapped from the design canvas to window space; everything
        // below draws against this so the layout scales/centers with the window.
        const UIRect rect = ui_apply_transform(xform, element.rect);
        // Font and slider-knob sizes scale with the canvas too.
        const float ui_font_size = UI_FONT_SIZE * xform.scale;

        if (element.element_type == "panel") {
            // Panel: filled rect (bg), then a 1px border outline (text). (R2)
            float sdl_y = to_sdl_y(static_cast<float>(window_height_), rect.y, rect.h);
            SDL_FRect frect{rect.x, sdl_y, rect.w, rect.h};

            // Fill first. (R2.1, R2.2)
            SDL_SetRenderDrawColor(renderer_, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
            SDL_RenderFillRect(renderer_, &frect);

            // Border second so it composites over the fill. (R2.3, R2.4)
            SDL_SetRenderDrawColor(renderer_, text_color.r, text_color.g,
                                   text_color.b, text_color.a);
            SDL_RenderRect(renderer_, &frect);

        } else if (element.element_type == "label") {
            // Label: text in the text color, left edge at rect.x, bottom edge
            // at rect.y. Font and colors are always resolved; glyphs are only
            // drawn when the font loads and the text is non-empty. (R3)
            TTF_Font* font = resource_manager_.load_font(UI_FONT_NAME, ui_font_size);
            if (!font) continue;                 // No font -> no glyphs, no error. (R3.6)
            if (element.label_text.empty()) continue;  // Empty text -> no glyphs. (R3.4)

            SDL_Surface* surface = TTF_RenderText_Blended(
                font, element.label_text.c_str(), 0, to_sdl_color(text_color));
            if (!surface) continue;

            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            SDL_DestroySurface(surface);
            if (!texture) continue;

            float text_w = 0.0f, text_h = 0.0f;
            SDL_GetTextureSize(texture, &text_w, &text_h);

            // v2: measured against the widget rect and shrunk to fit, so a label
            // can never draw outside its own box (or off the screen). Left-aligned,
            // vertically centered. (R3.5 amended)
            const TextFit fit = fit_text_in_rect(rect, text_w, text_h,
                                                 TextAlign::Left, LABEL_PAD * xform.scale);
            if (fit.visible) {
                float sdl_y = to_sdl_y(static_cast<float>(window_height_), fit.y, fit.h);
                SDL_FRect dest{fit.x, sdl_y, fit.w, fit.h};
                SDL_RenderTexture(renderer_, texture, nullptr, &dest);
            }

            SDL_DestroyTexture(texture);  // Per-frame texture.

        } else if (element.element_type == "button") {
            // Button: filled rect (bg), then centered label text (text). (R4)
            float rect_sdl_y = to_sdl_y(static_cast<float>(window_height_), rect.y, rect.h);
            SDL_FRect frect{rect.x, rect_sdl_y, rect.w, rect.h};

            // Fill the button body. (R4.1, R4.5)
            SDL_SetRenderDrawColor(renderer_, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
            SDL_RenderFillRect(renderer_, &frect);

            // Centered text only when the label is non-empty and the font loads.
            if (element.label_text.empty()) continue;  // (R4.6)
            TTF_Font* font = resource_manager_.load_font(UI_FONT_NAME, ui_font_size);
            if (!font) continue;

            SDL_Surface* surface = TTF_RenderText_Blended(
                font, element.label_text.c_str(), 0, to_sdl_color(text_color));
            if (!surface) continue;

            SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
            SDL_DestroySurface(surface);
            if (!texture) continue;

            float text_w = 0.0f, text_h = 0.0f;
            SDL_GetTextureSize(texture, &text_w, &text_h);

            // v2: centered AND fitted — a caption wider than its button used to
            // spill over both edges (compute_centered_text_origin explicitly did
            // not clamp). It is still exactly centered when it fits. (R4.3, R4.4)
            const TextFit fit = fit_text_in_rect(rect, text_w, text_h,
                                                 TextAlign::Center, BUTTON_PAD * xform.scale);
            if (!fit.visible) { SDL_DestroyTexture(texture); continue; }
            float text_sdl_y = to_sdl_y(static_cast<float>(window_height_), fit.y, fit.h);
            SDL_FRect dest{fit.x, text_sdl_y, fit.w, fit.h};
            SDL_RenderTexture(renderer_, texture, nullptr, &dest);

            SDL_DestroyTexture(texture);  // Per-frame texture.

        } else if (element.element_type == "slider") {
            // Slider: track (filled rect bg + 1px border text), same idiom as
            // panel/button, then a knob (filled rect text) positioned from the
            // UIState value via the pure helper. (R10.1, R10.2, R10.3, R10.5)
            float track_sdl_y = to_sdl_y(static_cast<float>(window_height_), rect.y, rect.h);
            SDL_FRect track{rect.x, track_sdl_y, rect.w, rect.h};

            // Track fill then border. (R10.1, R10.3)
            SDL_SetRenderDrawColor(renderer_, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
            SDL_RenderFillRect(renderer_, &track);
            SDL_SetRenderDrawColor(renderer_, text_color.r, text_color.g,
                                   text_color.b, text_color.a);
            SDL_RenderRect(renderer_, &track);

            // Knob: full track height, centered at the clamped value position.
            // No UIState mutation, no drag/click input. (R10.2, R10.4, R10.5)
            const float knob_w = 16.0f * xform.scale;  // scales with the canvas
            float cx = slider_knob_center_x(rect, value, knob_w);
            SDL_FRect knob{cx - knob_w * 0.5f, track_sdl_y,
                           knob_w, rect.h};
            SDL_RenderFillRect(renderer_, &knob);  // text color already set

        } else if (element.element_type == "checkbox") {
            // Checkbox: box (filled rect bg + 1px border text), then an inset
            // check (filled rect text) only when the value is non-zero. No
            // UIState mutation, no toggle/click input. (R11.1, R11.4, R11.5)
            float box_sdl_y = to_sdl_y(static_cast<float>(window_height_), rect.y, rect.h);
            SDL_FRect box{rect.x, box_sdl_y, rect.w, rect.h};

            // Box fill then border. (R11.1, R11.4)
            SDL_SetRenderDrawColor(renderer_, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
            SDL_RenderFillRect(renderer_, &box);
            SDL_SetRenderDrawColor(renderer_, text_color.r, text_color.g,
                                   text_color.b, text_color.a);
            SDL_RenderRect(renderer_, &box);

            // Check shown only when value != 0, inset 25% on each axis. (R11.2, R11.3)
            if (checkbox_is_checked(value)) {
                float inset = rect.w * 0.25f;
                float ih = rect.h * 0.25f;
                float check_sdl_y = to_sdl_y(static_cast<float>(window_height_),
                                             rect.y + ih, rect.h - 2.0f * ih);
                SDL_FRect check{rect.x + inset, check_sdl_y,
                                rect.w - 2.0f * inset, rect.h - 2.0f * ih};
                SDL_RenderFillRect(renderer_, &check);  // text color already set
            }
        }
        // Unknown element_type -> drawn nothing, no error.

        // Phase 7: keyboard focus indicator — an amber border outset 2px around
        // the widget rect, drawn last so it frames the widget body. Screen-space
        // via the shared to_sdl_y formula; no new Y-flip. (R: keyboard nav)
        if (focused) {
            constexpr float FOCUS_OUTSET = 2.0f;
            float focus_sdl_y = to_sdl_y(static_cast<float>(window_height_),
                                         rect.y, rect.h) - FOCUS_OUTSET;
            SDL_FRect focus_rect{rect.x - FOCUS_OUTSET, focus_sdl_y,
                                 rect.w + 2.0f * FOCUS_OUTSET,
                                 rect.h + 2.0f * FOCUS_OUTSET};
            SDL_SetRenderDrawColor(renderer_, 255, 215, 0, 255);  // amber
            SDL_RenderRect(renderer_, &focus_rect);
        }
    }
}
