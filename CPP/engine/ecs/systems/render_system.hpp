/**
 * RenderSystem - System for rendering entities with visual components
 * 
 * This system demonstrates the "S" in ECS - Systems contain logic that operates
 * on entities with specific component combinations. The RenderSystem queries for
 * entities that have Position and Size components, then renders them as either
 * textured sprites (if Images component is present) or filled rectangles (if
 * Color component is present).
 * 
 * Rendering priority: SpriteSheet > Images > Color > skip
 * 
 * Coordinate preference: When an entity has a ScreenPosition component (written
 * by CameraSystem), the RenderSystem uses ScreenPosition for rendering coordinates.
 * Otherwise, it falls back to Position (backward compatible). Size dimensions are
 * multiplied by camera.zoom (read from Blackboard, default 1.0) to produce the
 * destination rectangle.
 * 
 * Key ECS Concepts:
 * - Systems don't store data, they operate on component data
 * - Systems query for entities with specific component combinations
 * - Multiple systems can operate on the same entities independently
 */

#ifndef RENDER_SYSTEM_HPP
#define RENDER_SYSTEM_HPP

#include <SDL3/SDL.h>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

// Forward declaration — no #include needed in the header
class ResourceManager;

/**
 * RenderSystem class
 * 
 * Responsible for rendering all entities that have the required visual components.
 * This system demonstrates how ECS separates logic (systems) from data (components).
 */
class RenderSystem {
public:
    /**
     * Constructor
     * 
     * @param renderer SDL renderer for drawing operations
     * @param resource_manager ResourceManager for loading textures (spec 1)
     */
    RenderSystem(SDL_Renderer* renderer, ResourceManager& resource_manager);

    /**
     * Clear the screen to a solid background color, or tile a background texture
     */
    void clear_background();

    /**
     * Set a background texture to tile across the window instead of solid color.
     * Pass nullptr to revert to solid blue.
     * @param texture_name Filename of the texture in the assets directory
     */
    void set_background_texture(const std::string& texture_name);

    /**
     * Render all entities with Position and Size components.
     * 
     * For each entity, prefers ScreenPosition over Position for coordinates.
     * Reads camera.zoom from the Blackboard (default 1.0f) and multiplies
     * Size dimensions by zoom to compute the destination rectangle.
     * 
     * Priority chain: SpriteSheet > Images > Color > skip
     * - If entity has SpriteSheet: render atlas frame via ResourceManager
     * - Else if entity has Images: render texture via ResourceManager
     * - Else if entity has Color: render filled rectangle
     * - Else: skip entity
     * 
     * @param storage Component storage to query for entities and components
     * @param blackboard Blackboard to read camera.zoom from
     */
    void render(const ComponentStorage& storage, const Blackboard& blackboard);

    /**
     * Present the rendered frame to the screen
     */
    void present();

    /**
     * Draw a dashed world border rectangle.
     *
     * Renders a 5-pixel-wide dashed outline around the world bounds using
     * alternating black/red/green segments. The border is drawn in screen
     * space after the camera transform, so it moves with zoom/pan.
     *
     * @param blackboard Blackboard containing world bounds and camera config
     */
    void draw_world_border(const Blackboard& blackboard);

private:
    /**
     * Draw a single entity.
     * 
     * When texture is non-null, renders the texture via SDL_RenderTexture
     * (or SDL_RenderTextureRotated if rotation_angle is non-zero).
     * When texture is null, renders a filled rectangle via SDL_RenderFillRect
     * using the provided Color (rotation_angle is ignored).
     * 
     * Both paths apply the Y-axis flip: sdl_y = window_height - y - height
     * 
     * @param x      X coordinate (screen-space, bottom-left origin)
     * @param y      Y coordinate (screen-space, bottom-left origin)
     * @param width  Zoom-scaled width in pixels
     * @param height Zoom-scaled height in pixels
     * @param color  Color component (used only when texture is nullptr)
     * @param texture Optional texture to render (nullptr = colored rectangle)
     * @param rotation_angle Rotation angle in radians (default 0.0f, used only with texture)
     * @param src_rect Optional source rectangle for atlas rendering (nullptr = full texture)
     */
    void draw_entity(float x, float y, float width, float height,
                     const Color& color, SDL_Texture* texture = nullptr,
                     float rotation_angle = 0.0f,
                     const SDL_FRect* src_rect = nullptr);

    SDL_Renderer* renderer_;
    ResourceManager& resource_manager_;
    std::string background_texture_name_;
};

#endif // RENDER_SYSTEM_HPP
