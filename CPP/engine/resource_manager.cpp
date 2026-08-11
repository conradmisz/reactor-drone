#include "resource_manager.hpp"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

// --- Construction / Destruction ---

ResourceManager::ResourceManager(SDL_Renderer* renderer, const std::string& assets_dir)
    : renderer_(renderer)
    , assets_directory_(assets_dir)
    , missing_texture_(create_checkerboard_texture())
{
}

ResourceManager::~ResourceManager() {
    for (auto& [key, texture] : texture_cache_) {
        SDL_DestroyTexture(texture);
    }
    for (auto& [key, font] : font_cache_) {
        TTF_CloseFont(font);
    }
    for (auto& [key, texture] : font_text_cache_) {
        SDL_DestroyTexture(texture);
    }
    if (missing_texture_) {
        SDL_DestroyTexture(missing_texture_);
    }
}

// --- Private: Checkerboard Generation ---

SDL_Texture* ResourceManager::create_checkerboard_texture() {
    constexpr int size = 64;
    constexpr int cell = 8;

    SDL_Surface* surface = SDL_CreateSurface(size, size, SDL_PIXELFORMAT_RGBA8888);
    if (!surface) return nullptr;

    auto* pixels = static_cast<uint32_t*>(surface->pixels);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            bool is_magenta = ((x / cell) + (y / cell)) % 2 == 0;
            pixels[y * size + x] = is_magenta
                ? 0xFF00FFFF   // Magenta (RGBA8888: R=FF, G=00, B=FF, A=FF)
                : 0x000000FF;  // Black   (RGBA8888: R=00, G=00, B=00, A=FF)
        }
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_DestroySurface(surface);
    return texture;
}

// --- Texture Loading ---

SDL_Texture* ResourceManager::load_texture(const std::string& name) {
    std::string full_path = assets_directory_ + "/images/" + name;

    auto it = texture_cache_.find(full_path);
    if (it != texture_cache_.end()) {
        return it->second;
    }

    SDL_Texture* texture = IMG_LoadTexture(renderer_, full_path.c_str());
    if (!texture) {
        SDL_Log("Warning: Failed to load texture '%s': %s", full_path.c_str(), SDL_GetError());
        return missing_texture_;
    }

    // Enable alpha blending so PNG transparency is rendered correctly
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    texture_cache_[full_path] = texture;
    return texture;
}

SDL_Texture* ResourceManager::try_load_texture(const std::string& name) {
    std::string full_path = assets_directory_ + "/images/" + name;
    auto it = texture_cache_.find(full_path);
    if (it != texture_cache_.end()) return it->second;
    if (missing_names_.count(full_path)) return nullptr;

    SDL_Texture* texture = IMG_LoadTexture(renderer_, full_path.c_str());
    if (!texture) {
        // A miss is an expected answer here, not an error: cache it silently.
        missing_names_.insert(full_path);
        return nullptr;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    texture_cache_[full_path] = texture;
    return texture;
}

// --- Font Loading ---

TTF_Font* ResourceManager::load_font(const std::string& name, float size) {
    std::string full_path = assets_directory_ + "/fonts/" + name;
    std::string key = full_path + ":" + std::to_string(static_cast<int>(size));

    auto it = font_cache_.find(key);
    if (it != font_cache_.end()) {
        return it->second;
    }

    TTF_Font* font = TTF_OpenFont(full_path.c_str(), size);
    if (!font) {
        SDL_Log("Warning: Failed to load font '%s': %s", full_path.c_str(), SDL_GetError());
        return nullptr;
    }

    font_cache_[key] = font;
    return font;
}

// --- Text Texture Rendering ---

SDL_Texture* ResourceManager::render_text(const std::string& font_name, float font_size,
                                           const std::string& text, SDL_Color color) {
    std::string full_path = assets_directory_ + "/fonts/" + font_name;
    std::string key = full_path + ":" + std::to_string(static_cast<int>(font_size))
        + ":" + text
        + ":" + std::to_string(color.r) + "," + std::to_string(color.g)
        + "," + std::to_string(color.b) + "," + std::to_string(color.a);

    auto it = font_text_cache_.find(key);
    if (it != font_text_cache_.end()) {
        return it->second;
    }

    TTF_Font* font = load_font(font_name, font_size);
    if (!font) {
        return nullptr;
    }

    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), 0, color);
    if (!surface) {
        SDL_Log("Warning: Failed to render text '%s': %s", text.c_str(), SDL_GetError());
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_DestroySurface(surface);

    if (!texture) {
        SDL_Log("Warning: Failed to create texture from rendered text '%s': %s",
                text.c_str(), SDL_GetError());
        return nullptr;
    }

    font_text_cache_[key] = texture;
    return texture;
}

// --- Introspection ---

size_t ResourceManager::texture_count() const {
    return texture_cache_.size();
}

size_t ResourceManager::font_count() const {
    return font_cache_.size();
}

size_t ResourceManager::text_cache_count() const {
    return font_text_cache_.size();
}

SDL_Texture* ResourceManager::get_missing_texture() const {
    return missing_texture_;
}
