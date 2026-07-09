#ifndef DEBUG_HUD_SYSTEM_HPP
#define DEBUG_HUD_SYSTEM_HPP

#include <SDL3/SDL.h>
#include "engine/ecs/blackboard.hpp"
#include <cstddef>

// Forward declaration — no #include needed in the header
class ResourceManager;

/**
 * DebugHUDSystem class
 *
 * Renders a debug overlay on top of the game HUD. Displays pause state
 * and step instructions when the debug HUD is visible and the game is paused.
 * Layout is programmatic (NOT loaded from GameData.json).
 *
 * Key design decisions:
 * - Constructor DI pattern (same as HUDSystem / RenderSystem)
 * - Takes debug_hud_visible and debug_paused as direct parameters
 *   rather than reading from Blackboard (these are main.cpp-local state)
 * - Does NOT use ECS entities/components — debug HUD is a developer tool
 * - Uses ResourceManager for font loading, SDL3_ttf for text rendering
 * - Y-axis flip: sdl_y = window_height - game_y - text_height
 */
class DebugHUDSystem {
public:
    /**
     * Constructor — dependencies injected, not global/singleton.
     *
     * @param renderer        SDL renderer for drawing operations
     * @param rm              ResourceManager for loading fonts
     * @param window_width    Window width in pixels
     * @param window_height   Window height in pixels (for Y-axis flip)
     * @param overlay_alpha   Alpha for the black background overlay (0=transparent, 255=opaque)
     */
    DebugHUDSystem(SDL_Renderer* renderer, ResourceManager& rm,
                   int window_width, int window_height,
                   uint8_t overlay_alpha = 128);

    /**
     * Render the debug HUD overlay.
     *
     * When debug_hud_visible is false, returns immediately (nothing rendered).
     * When visible: always renders the frame counter; when also paused,
     * renders "PAUSED (F1: Resume, F2: Step)" above the frame counter.
     *
     * @param debug_hud_visible Whether the debug HUD overlay is shown
     * @param debug_paused      Whether the game is currently paused
     * @param frame_count       Current simulation frame count from Timer
     */
    void render(bool debug_hud_visible, bool debug_paused, uint64_t frame_count,
                const Blackboard& blackboard, size_t entity_count);

private:
    /**
     * Render a single line of yellow text at the given bottom-left game coords.
     * Applies Y-axis flip internally. Returns the rendered text height (for stacking).
     */
    float render_text_line(const char* text, float game_x, float game_y);
    SDL_Renderer* renderer_;
    ResourceManager& resource_manager_;
    int window_width_;
    int window_height_;
    uint8_t overlay_alpha_;
};

#endif // DEBUG_HUD_SYSTEM_HPP
