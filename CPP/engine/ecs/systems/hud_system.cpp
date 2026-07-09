/**
 * HUDSystem implementation
 *
 * Renders text overlays for entities with both Text and ScreenPosition components.
 * Checks hud_visible from Blackboard before rendering. Uses ResourceManager to
 * load fonts and SDL3_ttf to render text. Applies Y-axis flip from bottom-left
 * game coords to top-left SDL coords.
 */

#include "engine/ecs/systems/hud_system.hpp"
#include "engine/resource_manager.hpp"
#include <SDL3_ttf/SDL_ttf.h>

HUDSystem::HUDSystem(SDL_Renderer* renderer, ResourceManager& rm,
                     int window_width, int window_height)
    : renderer_(renderer), resource_manager_(rm),
      window_width_(window_width), window_height_(window_height) {
}

void HUDSystem::render(const ComponentStorage& storage, Blackboard& blackboard) {
    // Check visibility — default to true if key missing
    bool visible = blackboard.get_or<bool>("hud_visible", true);
    if (!visible) return;

    // Iterate all entities with Text component
    auto entities = storage.entities_with_component<Text>();

    for (Entity entity : entities) {
        // Must also have ScreenPosition — skip if missing
        if (!storage.has_component<ScreenPosition>(entity)) continue;

        auto text_opt = storage.get_component<Text>(entity);
        auto pos_opt = storage.get_component<ScreenPosition>(entity);
        if (!text_opt.has_value() || !pos_opt.has_value()) continue;

        const auto& text = text_opt->get();
        const auto& screen_pos = pos_opt->get();

        // Load font via ResourceManager (cached after first call)
        TTF_Font* font = resource_manager_.load_font(text.font_name, text.font_size);
        if (!font) continue;  // Skip if font can't be loaded

        // Render text to surface
        SDL_Surface* surface = TTF_RenderText_Blended(font, text.content.c_str(), 0, text.color);
        if (!surface) continue;

        // Convert surface to texture
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_DestroySurface(surface);  // Free surface immediately
        if (!texture) continue;

        // Get rendered text dimensions
        float text_width, text_height;
        SDL_GetTextureSize(texture, &text_width, &text_height);

        // Compute destination rectangle with Y-axis flip
        // Formula: sdl_y = window_height - game_y - text_height
        SDL_FRect dest;
        dest.x = screen_pos.x;
        dest.y = static_cast<float>(window_height_) - screen_pos.y - text_height;
        dest.w = text_width;
        dest.h = text_height;

        // Draw and clean up
        SDL_RenderTexture(renderer_, texture, nullptr, &dest);
        SDL_DestroyTexture(texture);  // Per-frame texture — content changes each frame
    }
}
