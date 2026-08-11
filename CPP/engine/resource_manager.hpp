#ifndef RESOURCE_MANAGER_HPP
#define RESOURCE_MANAGER_HPP

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstddef>

class ResourceManager {
public:
    // --- Construction / Destruction ---
    ResourceManager(SDL_Renderer* renderer, const std::string& assets_dir);
    ~ResourceManager();

    // Non-copyable, non-movable (owns SDL resources)
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;

    // --- Texture Loading ---
    // Resolves path: assets_dir/images/<name>
    // Returns cached texture on hit, loads from disk on miss.
    // Returns missing_texture_ if file cannot be loaded.
    SDL_Texture* load_texture(const std::string& name);

    /**
     * Probe for an optional texture (v3 Tier 2: the `_glow` emissive siblings).
     * Returns nullptr — NOT the magenta missing-texture — when the file does
     * not exist, and caches the miss so the probe never hits the disk (or the
     * log) twice for the same name.
     */
    SDL_Texture* try_load_texture(const std::string& name);

    // --- Font Loading ---
    // Resolves path: assets_dir/fonts/<name>
    // Cache key: resolved_path + ":" + to_string(int(size))
    // Returns cached font on hit, loads from disk on miss.
    // Returns nullptr if file cannot be loaded.
    TTF_Font* load_font(const std::string& name, float size);

    // --- Text Texture Rendering ---
    // Cache key: font_path + ":" + size + ":" + text + ":" + r,g,b,a
    // Renders text via TTF_RenderText_Blended(font, text, 0, color),
    // converts surface to texture, caches result.
    // Returns nullptr if font cannot be loaded.
    SDL_Texture* render_text(const std::string& font_name, float font_size,
                             const std::string& text, SDL_Color color);

    // --- Introspection (for testing) ---
    size_t texture_count() const;
    size_t font_count() const;
    size_t text_cache_count() const;
    SDL_Texture* get_missing_texture() const;

private:
    SDL_Renderer* renderer_;
    std::string assets_directory_;

    std::unordered_map<std::string, SDL_Texture*> texture_cache_;
    std::unordered_set<std::string> missing_names_;   // cached try_load_texture misses
    std::unordered_map<std::string, TTF_Font*> font_cache_;
    std::unordered_map<std::string, SDL_Texture*> font_text_cache_;

    SDL_Texture* missing_texture_;

    // Generate the 64x64 magenta/black checkerboard at construction
    SDL_Texture* create_checkerboard_texture();
};

#endif // RESOURCE_MANAGER_HPP
