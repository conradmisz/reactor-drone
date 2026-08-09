#ifndef UI_RENDER_SYSTEM_HPP
#define UI_RENDER_SYSTEM_HPP

#include <SDL3/SDL.h>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

// Forward declaration — no #include needed in the header
class ResourceManager;

/**
 * UIRenderSystem class
 *
 * Draws three non-interactive widget kinds — panels (filled rect + 1px border),
 * labels (text via a ResourceManager font), and buttons (filled rect + centered
 * label text). It resolves each widget's WidgetState (always Normal in this
 * phase), looks up colors in the StyleTable, filters widgets to those on active
 * screens, and draws them in ascending z_order (entity-id tiebreak).
 *
 * The UIRenderSystem renders AFTER RenderSystem::render and BEFORE
 * HUDSystem::render in the game loop, compositing widgets on top of the game
 * world and below the gameplay HUD text.
 *
 * Key design decisions:
 * - Constructor DI pattern (mirrors HUDSystem)
 * - ResourceManager forward-declared in header, included only in .cpp
 * - Bottom-left origin preserved: Y-flip uses the shared
 *   sdl_y = window_height - y - height formula via ui_render_math helpers
 */
class UIRenderSystem {
public:
    /**
     * Constructor — dependencies injected, not global/singleton.
     *
     * @param renderer      SDL renderer for drawing operations
     * @param rm            ResourceManager for loading fonts
     * @param window_width  Window width in pixels
     * @param window_height Window height in pixels (for Y-axis flip)
     */
    UIRenderSystem(SDL_Renderer* renderer, ResourceManager& rm,
                   int window_width, int window_height);

    /**
     * Render all widgets that belong to an active screen.
     *
     * Collects active screen names from UIScreen entities, builds a draw list
     * from UIElement entities whose ScreenMembership names an active screen,
     * sorts by z_order (entity-id tiebreak), resolves colors via the StyleTable
     * stored on the Blackboard, and draws each panel/label/button.
     *
     * @param storage    ComponentStorage to query for widget/screen entities
     * @param blackboard Blackboard to read the StyleTable ("ui_styles")
     */
    void render(const ComponentStorage& storage, Blackboard& blackboard);

private:
    SDL_Renderer* renderer_;
    ResourceManager& resource_manager_;
    int window_width_;
    int window_height_;
    // v2: seconds accumulated from delta_time, driving UIElement::pulse_hz.
    // Render-only state — deliberately not on the Blackboard, so no game system
    // can read it and no replay can diverge on it.
    float elapsed_ = 0.0f;
};

#endif // UI_RENDER_SYSTEM_HPP
