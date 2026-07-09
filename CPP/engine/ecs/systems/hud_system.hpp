#ifndef HUD_SYSTEM_HPP
#define HUD_SYSTEM_HPP

#include <SDL3/SDL.h>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

// Forward declaration — no #include needed in the header
class ResourceManager;

/**
 * HUDSystem class
 *
 * Renders text overlays for HUD entities that have both Text and ScreenPosition
 * components. Reads hud_visible from the Blackboard to control visibility.
 * Uses the ResourceManager to load fonts and SDL3_ttf to render text.
 *
 * The HUDSystem renders AFTER the RenderSystem in the game loop, producing
 * an overlay on top of the game world.
 *
 * Key design decisions:
 * - Constructor DI pattern (same as RenderSystem)
 * - ResourceManager forward-declared in header, included only in .cpp
 * - Text textures created and destroyed per-frame (dynamic content)
 * - Fonts cached by ResourceManager
 * - Y-axis flip: sdl_y = window_height - game_y - text_height
 */
class HUDSystem {
public:
    /**
     * Constructor — dependencies injected, not global/singleton.
     *
     * @param renderer      SDL renderer for drawing operations
     * @param rm            ResourceManager for loading fonts (spec 1)
     * @param window_width  Window width in pixels
     * @param window_height Window height in pixels (for Y-axis flip)
     */
    HUDSystem(SDL_Renderer* renderer, ResourceManager& rm,
              int window_width, int window_height);

    /**
     * Render all HUD entities.
     *
     * Reads hud_visible from Blackboard (defaults to true if missing).
     * Iterates entities with both Text and ScreenPosition components.
     * For each, loads font via ResourceManager, renders text to surface,
     * converts to texture, applies Y-flip, draws, and cleans up.
     *
     * @param storage    ComponentStorage to query for Text + ScreenPosition entities
     * @param blackboard Blackboard to read hud_visible
     */
    void render(const ComponentStorage& storage, Blackboard& blackboard);

private:
    SDL_Renderer* renderer_;
    ResourceManager& resource_manager_;
    [[maybe_unused]] int window_width_;  // reserved for future horizontal clipping
    int window_height_;
};

#endif // HUD_SYSTEM_HPP
