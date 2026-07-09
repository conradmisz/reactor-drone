#ifndef COMPONENTS_HPP
#define COMPONENTS_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <SDL3/SDL.h>

/**
 * Entity type alias
 * 
 * In ECS architecture, an entity is simply a unique identifier (ID) that represents
 * a game object. Entities themselves have no data or behavior - they're just IDs
 * that components can be attached to.
 * 
 * Using uint32_t allows for over 4 billion unique entities, which is more than
 * sufficient for educational purposes and most games.
 */
using Entity = uint32_t;

/**
 * Position component
 * 
 * Represents the 2D location of an entity in pixel coordinates.
 * This is pure data with no behavior - the component only stores where
 * something is, not how it moves or what it does.
 * 
 * In ECS, components are just data structures. Systems (like RenderSystem)
 * read this data to perform operations.
 */
struct Position {
    float x;  // X coordinate in pixels (horizontal position)
    float y;  // Y coordinate in pixels (vertical position)
};

/**
 * Size component
 * 
 * Represents the 2D dimensions of an entity in pixels.
 * Used by the render system to determine how large to draw an entity.
 * 
 * Like all components, this is pure data - it describes what size something is,
 * not how it scales or resizes.
 */
struct Size {
    float width;   // Width in pixels
    float height;  // Height in pixels
};

/**
 * Color component
 * 
 * Represents the RGBA color of an entity.
 * Each channel (red, green, blue, alpha) ranges from 0-255.
 * 
 * - r, g, b: Color channels (0 = none of that color, 255 = full intensity)
 * - a: Alpha channel for transparency (0 = fully transparent, 255 = fully opaque)
 * 
 * Using uint8_t matches SDL3's color representation, avoiding conversions
 * when passing colors to SDL rendering functions.
 */
struct Color {
    uint8_t r;  // Red channel (0-255)
    uint8_t g;  // Green channel (0-255)
    uint8_t b;  // Blue channel (0-255)
    uint8_t a;  // Alpha channel (0-255, 255 = fully opaque)
};

/**
 * Tint component (engine; v2 neon overhaul)
 *
 * Optional per-entity colour/alpha modulation applied by the RenderSystem on top
 * of whatever the entity draws (texture or colour rect). When `additive` is set,
 * the entity is drawn with additive blending (SDL_BLENDMODE_ADD) for glow effects.
 * Default {255,255,255,255,false} is identity — no visual change.
 *
 * The RenderSystem MUST reset the shared/cached texture's mod state after every
 * textured draw, since textures are cache-shared across entities.
 */
struct Tint {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;
    bool additive = false;
};

/**
 * Modulate a base colour by a tint, per channel (base * tint / 255), including
 * alpha. Pure helper — identity when the tint is {255,255,255,255}, and every
 * output channel stays within [0,255]. Models the colour-rect render path and is
 * the unit-testable core of Tint handling.
 */
inline Color modulate_color(const Color& c, const Tint& t) {
    auto mul = [](uint8_t a, uint8_t b) -> uint8_t {
        return static_cast<uint8_t>((static_cast<int>(a) * static_cast<int>(b)) / 255);
    };
    return Color{mul(c.r, t.r), mul(c.g, t.g), mul(c.b, t.b), mul(c.a, t.a)};
}

/**
 * Input component
 * 
 * Tracks keyboard input state for arrow keys.
 * This component stores boolean flags indicating which arrow keys are currently pressed.
 * 
 * In ECS, input state is stored as a component rather than global variables,
 * which makes it testable, supports multiple players, and follows ECS principles.
 * 
 * The InputSystem updates these flags based on SDL keyboard events.
 */
struct Input {
    bool up = false;     // True when UP arrow key is pressed
    bool down = false;   // True when DOWN arrow key is pressed
    bool left = false;   // True when LEFT arrow key is pressed
    bool right = false;  // True when RIGHT arrow key is pressed
    bool fire = false;   // True when SPACE key is pressed
};

/**
 * Velocity component
 * 
 * Represents movement speed in pixels per second.
 * This component stores the rate of change of position, not the position itself.
 * 
 * Units are pixels/second to enable frame-rate independent movement.
 * The MovementSystem uses velocity and delta_time to update position:
 *   position += velocity * delta_time
 * 
 * Positive dx moves right, negative dx moves left.
 * Positive dy moves up (bottom-left origin), negative dy moves down.
 */
struct Velocity {
    float dx = 0.0f;  // X velocity in pixels per second
    float dy = 0.0f;  // Y velocity in pixels per second
};

/**
 * Images component
 * 
 * Associates an entity with one or more texture files for rendering.
 * Filenames are resolved by the ResourceManager relative to the assets directory.
 * The active_index selects which texture is currently displayed.
 * When present, the RenderSystem renders a texture instead of a colored rectangle.
 */
struct Images {
    std::vector<std::string> filenames;  // Texture filenames, e.g. {"explorer.png"}
    size_t active_index = 0;             // Index into filenames for the current texture

    const std::string& active_filename() const {
        return filenames.at(active_index);
    }
};

/**
 * Text component
 *
 * Stores text rendering parameters for HUD entities.
 * When paired with a ScreenPosition component, the HUDSystem renders
 * this text at the specified screen-space location.
 */
struct Text {
    std::string content;                              // Text string to render
    std::string font_name = "default.ttf";            // Font filename resolved by ResourceManager
    float font_size = 24.0f;                          // Font point size (SDL3_ttf uses float)
    SDL_Color color = {255, 255, 255, 255};           // RGBA color (default: white, fully opaque)
};

/**
 * ScreenPosition component
 *
 * Stores screen-space coordinates using the bottom-left origin.
 * Distinct from Position (world-space) — ScreenPosition is for HUD elements
 * that are fixed to the screen regardless of camera movement.
 *
 * y=0 is the bottom of the screen, y increases upward.
 */
struct ScreenPosition {
    float x;  // Horizontal screen-space coordinate
    float y;  // Vertical screen-space coordinate (bottom-left origin)
};

/**
 * Rotation component
 *
 * Stores orientation and rotational speed for an entity.
 * The angle is measured in radians and angular_velocity in radians per second.
 * A RotationSystem (added in a later spec) will update angle each frame:
 *   angle += angular_velocity * delta_time
 */
struct Rotation {
    float angle = 0.0f;            // Orientation in radians
    float angular_velocity = 0.0f; // Rotational speed in radians/second
    // When false, the renderer skips the "face-left → horizontal-flip" heuristic
    // and applies pure rotation. v2's symmetric, right-facing top-down art sets
    // this false; v1 entities keep the default true so their behaviour is unchanged.
    bool flip_when_left = true;
};

/**
 * Collider component
 *
 * Stores axis-aligned bounding box dimensions and collision filtering data.
 * The width and height define the AABB in world units.
 * The layer field identifies which collision group this entity belongs to,
 * and the mask field identifies which layers this entity can collide with.
 *
 * Collision filtering uses bitwise comparison: two entities can collide
 * when (a.layer & b.mask) != 0 && (b.layer & a.mask) != 0.
 */
struct Collider {
    float width;          // AABB width in world units
    float height;         // AABB height in world units
    uint8_t layer = 0;   // Collision layer this entity belongs to
    uint8_t mask = 0;    // Collision layers this entity can collide with
};

/**
 * CircleCollider component
 *
 * Stores a collision circle for narrow phase detection. The circle center
 * in world space is (Position.x + offset_x + radius, Position.y + offset_y + radius)
 * — the center of the entity's bounding square.
 *
 * Entities with CircleCollider MUST also have a Collider component for
 * layer/mask data. The broad phase AABB for circle entities is a square
 * with bottom-left at (Position.x + offset_x, Position.y + offset_y)
 * and side length 2 * radius.
 */
struct CircleCollider {
    float radius;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

/**
 * OBBCollider component
 *
 * Stores an oriented bounding box defined by half-extents. The OBB's
 * rotation angle is read from the entity's Rotation component.
 *
 * OBB center in world space: (Position.x + half_width, Position.y + half_height)
 *
 * Entities with OBBCollider MUST also have a Collider component for
 * layer/mask data and a Rotation component for orientation.
 *
 * OBBCollider takes precedence over CircleCollider for broad phase
 * AABB computation and narrow phase dispatch.
 *
 * Broad phase AABB (Max-AABB): a square centered on the OBB center
 * with half-extent sqrt(half_width² + half_height²). This AABB fully
 * contains the OBB at any rotation angle.
 */
struct OBBCollider {
    float half_width;
    float half_height;
};

/**
 * Lifetime component
 *
 * Stores a countdown timer in seconds. A LifetimeSystem (added in a later spec)
 * will decrement remaining each frame and attach a DestroyRequest when it
 * reaches zero, causing the entity to be cleaned up by the destruction pipeline.
 */
struct Lifetime {
    float remaining = 0.0f;  // Seconds until destruction
};

/**
 * WrapAround tag component
 *
 * Empty marker struct. When present on an entity, a WrapAroundSystem
 * (added in a later spec) will teleport the entity to the opposite
 * screen edge when it moves beyond the boundary.
 *
 * Tag components have no data fields — their presence or absence on an
 * entity is the only information they carry.
 */
struct WrapAround {};

/**
 * DestroyRequest tag component
 *
 * Empty marker struct. Any system can attach a DestroyRequest to an entity
 * during a frame to schedule it for deferred destruction. The destruction
 * pipeline runs at the end of each frame, removes all components from
 * marked entities, and recycles their IDs via EntityManager.
 *
 * Using deferred destruction avoids iterator invalidation — systems can
 * safely mark entities for removal while iterating component maps.
 */
struct DestroyRequest {};

/**
 * CollidedWith component
 *
 * Stores the list of entities that collided with this entity during the
 * current frame. The CollisionSystem populates this component each frame
 * after running collision detection — it clears all existing CollidedWith
 * components first, then writes fresh results.
 *
 * Downstream systems query collision data via has_component<CollidedWith>()
 * and get_component<CollidedWith>() instead of reading from the Blackboard.
 */
struct CollidedWith {
    std::vector<Entity> entities;  // IDs of all entities this entity collided with
};

/**
 * Script component
 *
 * Associates a Lua script file with an entity. The filename is relative
 * to the assets directory. The initialized flag tracks whether on_init
 * has been called — ScriptSystem sets it to true after the first update.
 */
struct Script {
    std::string filename;       // Path to .lua file relative to assets dir
    bool initialized = false;   // True after on_init has been called
};

/**
 * SpriteSheet component
 *
 * Associates an entity with a texture atlas and describes the atlas grid
 * layout. The RenderSystem uses this to calculate a source rectangle for
 * rendering a single frame from the atlas.
 *
 * Frame indexing: frames are numbered left-to-right, top-to-bottom,
 * starting from 0. Frame 0 is the top-left cell of the grid.
 *
 * The Animation component (spec 080-03) will drive current_frame changes.
 * For this spec, current_frame defaults to 0 and can be set manually.
 */
struct SpriteSheet {
    std::string atlas_filename;   // Atlas image filename (resolved by ResourceManager)
    int frame_width = 0;          // Width of each frame in pixels
    int frame_height = 0;         // Height of each frame in pixels
    int columns = 1;              // Number of columns in the atlas grid
    int total_frames = 1;         // Total number of valid frames
    int current_frame = 0;        // Currently selected frame index (0-based)
};

/**
 * Animation component
 *
 * Describes a time-based frame animation sequence within a texture atlas.
 * The AnimationSystem advances current_frame based on delta_time and writes
 * it to SpriteSheet.current_frame each tick.
 *
 * Frame sequence: frames start_frame through start_frame + frame_count - 1.
 * Looping animations wrap from the last frame back to start_frame.
 * One-shot animations stop on the last frame and set finished = true.
 */
struct Animation {
    int current_frame = 0;        // Frame index currently selected for rendering
    int start_frame = 0;          // First frame index of the animation sequence
    int frame_count = 1;          // Number of frames in the sequence
    float frame_duration = 0.1f;  // Seconds each frame is displayed before advancing
    float elapsed = 0.0f;         // Time accumulated since last frame change
    bool looping = true;          // Whether to wrap back to start_frame after last frame
    bool playing = true;          // Whether the animation is currently advancing
    bool finished = false;        // True when a one-shot animation has completed
};

/**
 * RenderLayer component
 *
 * Optional draw-order hint for the RenderSystem. Entities are drawn in ascending
 * (layer, entity_id) order, so a higher layer draws on top. An entity without a
 * RenderLayer is treated as layer 0, so existing content (towers, enemies, tiles)
 * is unaffected. Use higher layers for overlays such as laser beams and health bars.
 */
struct RenderLayer {
    int layer = 0;
};

#endif // COMPONENTS_HPP
