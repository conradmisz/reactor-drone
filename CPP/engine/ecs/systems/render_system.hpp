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
#include <string>
#include <vector>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/systems/line_mesh_math.hpp"
#include "engine/ecs/systems/particle_mesh.hpp"

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
     * One tiled backdrop layer: a texture wrapped across the whole window with a
     * per-axis pixel offset (offsets come from parallax math; sign is arbitrary
     * since the tile wraps). Screen-space only — no camera transform, no Y-flip.
     *
     * `alpha` in [0,1] scales the layer's opacity, which is what the v2 Phase 5b
     * arena crossfade rides on: the outgoing arena's layers are pushed at 1.0 and
     * the incoming arena's on top at a rising alpha.
     */
    struct TiledLayer {
        std::string texture;
        float offset_x = 0.0f;
        float offset_y = 0.0f;
        float alpha = 1.0f;
    };

    /**
     * Clear to opaque black, then tile each layer (in order, back-to-front) over
     * the window with wraparound at its offset. Missing/zero-size textures are
     * skipped. Replaces clear_background when parallax backdrops are configured;
     * an empty list is just the black clear.
     */
    void render_layers(const std::vector<TiledLayer>& layers);

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
     * v3 Tier 5 (D211): one glowing polyline, immediate-mode. World-space
     * points; the camera transform and the world Y-flip are applied HERE (the
     * flip stays in this file, per the invariant). Game code pushes a list
     * each frame and calls render_glow_lines once into the scene and once
     * into the emissive target.
     */
    struct GlowLine {
        std::vector<line_mesh::P2> points;   // world space, >= 2
        float width = 6.0f;                  // world units (zoom-scaled)
        Color color{255, 255, 255, 255};
        bool core = true;                    // also draw a narrow white core
        // v3 Tier 7: per-point widths for a tapered trail. Empty = uniform
        // `width` (every pre-Tier-7 caller). Size must match `points`.
        std::vector<float> widths;
        // v3 Tier 7: ramp alpha with arc length so the tail dissolves. Uses
        // the u build_ribbon already computes — 0 at the oldest point, 1 at
        // the head — so it costs nothing extra to compute.
        bool fade_tail = false;
        // v3 Tier 10: how wide the hot core rides relative to the ribbon.
        // Narrower reads hotter — the same total light through less width.
        float core_scale = 0.35f;
    };

    /**
     * Draw glowing ribbons with SDL_RenderGeometry: each line becomes a
     * miter-joined triangle strip whose cross-section samples the 1D falloff
     * texture (v2/line_falloff.png), drawn additively. When `core` is set a
     * second strip at 0.35x width and white-lifted color rides on top — the
     * hot center every neon line needs. Lines with < 2 points are skipped.
     */
    void render_glow_lines(const std::vector<GlowLine>& lines,
                           const Blackboard& blackboard);

    /**
     * v3 Tier 9 (D215): every additive particle in ONE SDL_RenderGeometry call,
     * UV-mapped across v2/glow_disc_64.png so each one is a soft round disc.
     *
     * This exists because the alternative shapes are both wrong. Drawn with no
     * texture, a particle takes draw_entity's fill-rect path and is a hard
     * SQUARE that seeds the bloom chain and smears into a box halo (bugs/004).
     * Given a texture per particle, draw_entity makes six batch-flushing SDL
     * state calls EACH, measured at ~27x. One mesh makes six per frame.
     *
     * render_walk skips additive-tinted Color entities for exactly this reason
     * — this pass owns them. Call it wherever render()/render_emissive() are
     * called: once into the scene, once into the emissive target.
     */
    void render_particles(const ComponentStorage& storage,
                          const Blackboard& blackboard);

    /**
     * v3 Tier 2: the emissive pass. Walks the same entities in the same order
     * as render(), but draws ONLY what should feed the bloom chain, into the
     * currently bound render target (the BloomSystem's emissive target):
     *  - a sprite whose atlas/image has a `_glow` sibling texture (probed via
     *    ResourceManager::try_load_texture, misses cached) draws that sibling
     *    with identical geometry/rotation/tint;
     *  - otherwise an entity with an additive Tint draws its normal visual —
     *    its sharp copy is already in the scene, so this contributes halo only;
     *  - everything else is skipped.
     * Call between BloomSystem::begin_emissive() and resolve(); never call it
     * when bloom is inactive (the draws would land on the backbuffer).
     */
    void render_emissive(const ComponentStorage& storage, const Blackboard& blackboard);

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
     * @param tint   Optional per-entity colour/alpha/blend modulation (nullptr = none).
     *               Texture path: SetTextureColorMod/AlphaMod/BlendMode before draw,
     *               reset to 255/255/255/BLEND after (textures are cache-shared).
     *               Rect path: modulate_color(color, tint) + additive blend when set.
     * @param flip_when_left When false, skip the face-left horizontal-flip heuristic
     *               (pure rotation). v2 symmetric right-facing art passes false.
     */
    void draw_entity(float x, float y, float width, float height,
                     const Color& color, SDL_Texture* texture = nullptr,
                     float rotation_angle = 0.0f,
                     const SDL_FRect* src_rect = nullptr,
                     const Tint* tint = nullptr,
                     bool flip_when_left = true);

    /** Shared body of render()/render_emissive() — one walk, two draw policies. */
    void render_walk(const ComponentStorage& storage, const Blackboard& blackboard,
                     bool emissive);

    SDL_Renderer* renderer_;
    ResourceManager& resource_manager_;
    std::string background_texture_name_;
};

#endif // RENDER_SYSTEM_HPP
