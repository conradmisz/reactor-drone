/**
 * RenderSystem implementation
 * 
 * Renders entities using a priority chain: SpriteSheet > Images > Color > skip.
 * Textured entities use SDL_RenderTexture; colored entities use SDL_RenderFillRect.
 * Both paths apply the Y-axis flip from bottom-left game coords to top-left SDL coords.
 * 
 * Coordinate preference: ScreenPosition (camera-aware) > Position (backward compatible).
 * Size dimensions are multiplied by camera.zoom (from Blackboard) for the destination rect.
 */

#include "engine/ecs/systems/render_system.hpp"
#include "engine/ecs/sprite_sheet_math.hpp"
#include "engine/resource_manager.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace {
/**
 * The coordinate space everything is drawn in. When a logical presentation is
 * active (see main.cpp) that is the logical size — SDL scales it to the window
 * itself, so the Y-flip and the tiling loops must use the logical dimensions,
 * NOT pixels. SDL_GetCurrentRenderOutputSize is deliberately not used here: with
 * LETTERBOX it returns the scaled content area in pixels (e.g. 1484x1000 for a
 * 980x660 logical surface in a 1600x1000 window), which flips Y about the wrong
 * axis. With no logical presentation set it falls back to the real output size,
 * so callers that never enable one are unaffected.
 */
void draw_surface_size(SDL_Renderer* renderer, int* w, int* h) {
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    if (SDL_GetRenderLogicalPresentation(renderer, w, h, &mode) &&
        mode != SDL_LOGICAL_PRESENTATION_DISABLED && *w > 0 && *h > 0) {
        return;
    }
    SDL_GetCurrentRenderOutputSize(renderer, w, h);
}
}  // namespace

RenderSystem::RenderSystem(SDL_Renderer* renderer, ResourceManager& resource_manager)
    : renderer_(renderer), resource_manager_(resource_manager) {
}

void RenderSystem::set_background_texture(const std::string& texture_name) {
    background_texture_name_ = texture_name;
}

void RenderSystem::clear_background() {
    if (!background_texture_name_.empty()) {
        SDL_Texture* bg = resource_manager_.load_texture(background_texture_name_);
        if (bg) {
            int win_w, win_h;
            draw_surface_size(renderer_, &win_w, &win_h);
            float tex_w, tex_h;
            SDL_GetTextureSize(bg, &tex_w, &tex_h);

            // Tile the texture across the window
            for (float y = 0; y < static_cast<float>(win_h); y += tex_h) {
                for (float x = 0; x < static_cast<float>(win_w); x += tex_w) {
                    SDL_FRect dst = {x, y, tex_w, tex_h};
                    SDL_RenderTexture(renderer_, bg, nullptr, &dst);
                }
            }
            return;
        }
    }
    // Fallback: solid blue
    SDL_SetRenderDrawColor(renderer_, 0, 0, 255, 255);
    SDL_RenderClear(renderer_);
}

void RenderSystem::render_layers(const std::vector<TiledLayer>& layers) {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    int win_w, win_h;
    draw_surface_size(renderer_, &win_w, &win_h);

    for (const auto& layer : layers) {
        SDL_Texture* tex = resource_manager_.load_texture(layer.texture);
        if (!tex) continue;
        float tw, th;
        SDL_GetTextureSize(tex, &tw, &th);
        if (tw <= 0.0f || th <= 0.0f) continue;

        // Textures are cached and shared by the ResourceManager, so an alpha mod
        // left behind here would leak into every later draw of the same texture.
        // Set it, tile, put it back.
        float a = layer.alpha < 0.0f ? 0.0f : (layer.alpha > 1.0f ? 1.0f : layer.alpha);
        SDL_SetTextureAlphaMod(tex, static_cast<Uint8>(a * 255.0f + 0.5f));

        // Wrap the offset into [0,tw)/[0,th) and tile from just off-screen so the
        // window is fully covered regardless of the offset's sign.
        float sx = std::fmod(layer.offset_x, tw); if (sx < 0.0f) sx += tw;
        float sy = std::fmod(layer.offset_y, th); if (sy < 0.0f) sy += th;
        for (float y = -sy; y < static_cast<float>(win_h); y += th) {
            for (float x = -sx; x < static_cast<float>(win_w); x += tw) {
                SDL_FRect dst = {x, y, tw, th};
                SDL_RenderTexture(renderer_, tex, nullptr, &dst);
            }
        }
        SDL_SetTextureAlphaMod(tex, 255);
    }
}

void RenderSystem::render(const ComponentStorage& storage, const Blackboard& blackboard) {
    render_walk(storage, blackboard, false);
}

void RenderSystem::render_emissive(const ComponentStorage& storage,
                                   const Blackboard& blackboard) {
    render_walk(storage, blackboard, true);
}

namespace {
/// "v2/enemy_spark.png" -> "v2/enemy_spark_glow.png"; no extension -> append.
std::string glow_name(const std::string& name) {
    auto dot = name.rfind('.');
    if (dot == std::string::npos) return name + "_glow";
    return name.substr(0, dot) + "_glow" + name.substr(dot);
}
}  // namespace

void RenderSystem::render_walk(const ComponentStorage& storage,
                               const Blackboard& blackboard, bool emissive) {
    float zoom = blackboard.get_or<float>("camera.zoom", 1.0f);

    auto all = storage.entities_with_component<Position>();

    // Draw order is by RenderLayer (entities without one are layer 0, so existing
    // content is unchanged; higher layers draw on top — e.g. laser beams, health bars).
    // Rather than an O(n log n) comparison sort, bucket the entities by layer in a
    // single O(n) pass and emit the buckets in ascending layer order (std::map keeps
    // the small, fixed set of layer keys ordered). Within a layer, entities keep their
    // storage order. draw_entity is then called over the layered sequence.
    std::map<int, std::vector<Entity>> buckets;
    for (Entity e : all) {
        int layer = 0;
        if (storage.has_component<RenderLayer>(e)) {
            auto rl = storage.get_component<RenderLayer>(e);
            if (rl.has_value()) layer = rl->get().layer;
        }
        buckets[layer].push_back(e);
    }

    std::vector<Entity> entities;
    entities.reserve(all.size());
    for (auto& [layer, bucket] : buckets) {
        (void)layer;
        for (Entity e : bucket) entities.push_back(e);
    }

    for (Entity entity : entities) {
        if (!storage.has_component<Size>(entity)) {
            continue;  // Position without Size — nothing to render
        }

        auto size_opt = storage.get_component<Size>(entity);
        if (!size_opt.has_value()) {
            continue;
        }
        const auto& size = size_opt->get();

        // Prefer ScreenPosition over Position for coordinates
        float x, y;
        if (storage.has_component<ScreenPosition>(entity)) {
            auto sp = storage.get_component<ScreenPosition>(entity);
            x = sp->get().x;
            y = sp->get().y;
        } else {
            auto pos = storage.get_component<Position>(entity);
            if (!pos.has_value()) continue;
            x = pos->get().x;
            y = pos->get().y;
        }

        // Zoom-scale dimensions
        float render_width = size.width * zoom;
        float render_height = size.height * zoom;

        // Hoist rotation + flip control + optional tint once — shared by all paths.
        float rotation_angle = 0.0f;
        bool flip_when_left = true;
        if (storage.has_component<Rotation>(entity)) {
            const auto& rot = storage.get_component<Rotation>(entity)->get();
            rotation_angle = rot.angle;
            flip_when_left = rot.flip_when_left;
        }
        Tint tint_val{};
        const Tint* tint = nullptr;
        if (storage.has_component<Tint>(entity)) {
            auto t = storage.get_component<Tint>(entity);
            if (t.has_value()) { tint_val = t->get(); tint = &tint_val; }
        }

        // Priority: SpriteSheet > Images > Color > skip
        if (storage.has_component<SpriteSheet>(entity)) {
            auto ss_opt = storage.get_component<SpriteSheet>(entity);
            if (ss_opt.has_value()) {
                const auto& ss = ss_opt->get();
                SDL_Texture* texture;
                if (emissive) {
                    texture = resource_manager_.try_load_texture(glow_name(ss.atlas_filename));
                    if (!texture) {
                        if (!(tint && tint->additive)) continue;
                        texture = resource_manager_.load_texture(ss.atlas_filename);
                    }
                } else {
                    texture = resource_manager_.load_texture(ss.atlas_filename);
                }
                SDL_FRect src_rect = compute_source_rect(
                    ss.current_frame, ss.columns, ss.frame_width, ss.frame_height);
                draw_entity(x, y, render_width, render_height,
                            Color{0, 0, 0, 255}, texture, rotation_angle, &src_rect,
                            tint, flip_when_left);
            }
        } else if (storage.has_component<Images>(entity)) {
            auto img_opt = storage.get_component<Images>(entity);
            if (img_opt.has_value()) {
                SDL_Texture* texture;
                if (emissive) {
                    texture = resource_manager_.try_load_texture(
                        glow_name(img_opt->get().active_filename()));
                    if (!texture) {
                        if (!(tint && tint->additive)) continue;
                        texture = resource_manager_.load_texture(
                            img_opt->get().active_filename());
                    }
                } else {
                    texture = resource_manager_.load_texture(
                        img_opt->get().active_filename());
                }
                draw_entity(x, y, render_width, render_height, Color{0, 0, 0, 255},
                            texture, rotation_angle, nullptr, tint, flip_when_left);
            }
        } else if (storage.has_component<Color>(entity)) {
            auto color_opt = storage.get_component<Color>(entity);
            if (color_opt.has_value()) {
                if (emissive && !(tint && tint->additive)) continue;
                draw_entity(x, y, render_width, render_height, color_opt->get(),
                            nullptr, 0.0f, nullptr, tint, flip_when_left);
            }
        }
        // else: has Position+Size but neither Images nor Color → skip
    }
}

void RenderSystem::render_glow_lines(const std::vector<GlowLine>& lines,
                                     const Blackboard& blackboard) {
    if (lines.empty()) return;

    // Same affine transform CameraSystem applies to entities (world -> screen),
    // then the world Y-flip — which lives in this file and nowhere else.
    float lookat_x = blackboard.get_or<float>("camera.lookat.x", 0.0f);
    float lookat_y = blackboard.get_or<float>("camera.lookat.y", 0.0f);
    float zoom = std::max(blackboard.get_or<float>("camera.zoom", 1.0f), 0.01f);
    int win_w, win_h;
    draw_surface_size(renderer_, &win_w, &win_h);
    const float cam_left = lookat_x - static_cast<float>(win_w) / zoom / 2.0f;
    const float cam_bottom = lookat_y - static_cast<float>(win_h) / zoom / 2.0f;

    SDL_Texture* falloff = resource_manager_.try_load_texture("v2/line_falloff.png");
    if (falloff) SDL_SetTextureBlendMode(falloff, SDL_BLENDMODE_ADD);

    auto draw_ribbon = [&](const std::vector<line_mesh::P2>& pts, float width,
                           Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
        const auto ribbon = line_mesh::build_ribbon(pts, width);
        if (ribbon.size() < 4) return;
        const auto idx = line_mesh::strip_indices(ribbon.size());
        std::vector<SDL_Vertex> verts;
        verts.reserve(ribbon.size());
        const SDL_FColor col{static_cast<float>(r) / 255.0f,
                             static_cast<float>(g) / 255.0f,
                             static_cast<float>(b) / 255.0f,
                             static_cast<float>(a) / 255.0f};
        for (const auto& v : ribbon) {
            SDL_Vertex sv;
            sv.position.x = (v.x - cam_left) * zoom;
            sv.position.y = static_cast<float>(win_h) - (v.y - cam_bottom) * zoom;
            sv.color = col;
            // Cross-section drives the falloff texture's vertical gradient.
            sv.tex_coord.x = 0.5f;
            sv.tex_coord.y = v.v;
            verts.push_back(sv);
        }
        if (!falloff) SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_ADD);
        SDL_RenderGeometry(renderer_, falloff, verts.data(),
                           static_cast<int>(verts.size()), idx.data(),
                           static_cast<int>(idx.size()));
        if (!falloff) SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    };

    for (const auto& line : lines) {
        if (line.points.size() < 2) continue;
        const float w = line.width * zoom;
        draw_ribbon(line.points, w, line.color.r, line.color.g, line.color.b,
                    line.color.a);
        if (line.core) {
            // The hot center: narrower, lifted toward white.
            auto lift = [](Uint8 c) {
                int v = static_cast<int>(c) + 140;
                return static_cast<Uint8>(v > 255 ? 255 : v);
            };
            draw_ribbon(line.points, w * 0.35f, lift(line.color.r),
                        lift(line.color.g), lift(line.color.b), line.color.a);
        }
    }
}

void RenderSystem::present() {
    SDL_RenderPresent(renderer_);
}

void RenderSystem::draw_entity(float x, float y, float width, float height,
                                const Color& color, SDL_Texture* texture,
                                float rotation_angle,
                                const SDL_FRect* src_rect,
                                const Tint* tint,
                                bool flip_when_left) {
    // Get window height for Y-axis flip
    int window_width, window_height;
    draw_surface_size(renderer_, &window_width, &window_height);

    // Compute destination rectangle ONCE — shared by both paths
    // CRITICAL: Y-axis flip from bottom-left game coords to top-left SDL coords
    // Formula: sdl_y = window_height - y - height
    SDL_FRect rect;
    rect.x = x;
    rect.y = static_cast<float>(window_height) - y - height;
    rect.w = width;
    rect.h = height;

    if (texture) {
        // Apply per-entity tint to the shared/cached texture. This state MUST be
        // reset after the draw (below) because textures are cache-shared across
        // entities — leaving it dirty would tint every later entity that reuses
        // the same texture.
        if (tint) {
            SDL_SetTextureColorMod(texture, tint->r, tint->g, tint->b);
            SDL_SetTextureAlphaMod(texture, tint->a);
            SDL_SetTextureBlendMode(texture,
                tint->additive ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
        }

        if (rotation_angle != 0.0f) {
            // Convert radians to degrees and negate for Y-axis flip
            // Positive game angle = CCW, SDL angle = CW, so negate
            double sdl_angle = -(static_cast<double>(rotation_angle) * 180.0 / M_PI);

            // For non-symmetric sprites (e.g. ducks facing right at angle 0):
            // When facing left (|angle| > 90°), flip horizontally instead of
            // rotating upside-down. This keeps the sprite right-side-up.
            // Symmetric v2 art (flip_when_left == false) skips this — pure rotation.
            SDL_FlipMode flip = SDL_FLIP_NONE;
            float abs_angle = std::fabs(rotation_angle);
            if (flip_when_left &&
                abs_angle > M_PI / 2.0f && abs_angle < 3.0f * M_PI / 2.0f) {
                flip = SDL_FLIP_HORIZONTAL;
                // Mirror the angle: subtract π so the sprite faces right in
                // its local frame, then the flip handles the left-facing.
                sdl_angle = -(static_cast<double>(rotation_angle - static_cast<float>(M_PI)) * 180.0 / M_PI);
            }

            SDL_RenderTextureRotated(renderer_, texture, src_rect, &rect,
                                      sdl_angle, nullptr, flip);
        } else {
            // Non-rotated textured rendering path — no rotation overhead
            SDL_RenderTexture(renderer_, texture, src_rect, &rect);
        }

        // Reset the shared texture's mod state to identity/BLEND after every draw.
        if (tint) {
            SDL_SetTextureColorMod(texture, 255, 255, 255);
            SDL_SetTextureAlphaMod(texture, 255);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        }
    } else {
        // Colored rectangle rendering path (backward compatible)
        // Rotation is ignored — SDL3 has no rotated fill-rect function.
        Color c = tint ? modulate_color(color, *tint) : color;
        if (tint && tint->additive) {
            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_ADD);
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            SDL_RenderFillRect(renderer_, &rect);
            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
        } else {
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            SDL_RenderFillRect(renderer_, &rect);
        }
    }
}

void RenderSystem::draw_world_border(const Blackboard& blackboard) {
    // Read world bounds from Blackboard (set by gamedata_loader)
    float world_x = blackboard.get_or<float>("world.x", 0.0f);
    float world_y = blackboard.get_or<float>("world.y", 0.0f);
    float world_w = blackboard.get_or<float>("world.width", 0.0f);
    float world_h = blackboard.get_or<float>("world.height", 0.0f);

    // No world bounds defined — nothing to draw
    if (world_w <= 0.0f || world_h <= 0.0f) return;

    // Read camera parameters for world-to-screen transform
    float lookat_x = blackboard.get_or<float>("camera.lookat.x", 0.0f);
    float lookat_y = blackboard.get_or<float>("camera.lookat.y", 0.0f);
    float zoom     = blackboard.get_or<float>("camera.zoom", 1.0f);
    int win_w, win_h;
    draw_surface_size(renderer_, &win_w, &win_h);

    // Camera transform: world → screen (bottom-left origin)
    float cam_left   = lookat_x - (static_cast<float>(win_w) / zoom) / 2.0f;
    float cam_bottom = lookat_y - (static_cast<float>(win_h) / zoom) / 2.0f;

    // World corners in screen space (bottom-left origin)
    float screen_left   = (world_x - cam_left) * zoom;
    float screen_bottom = (world_y - cam_bottom) * zoom;
    float screen_w      = world_w * zoom;
    float screen_h      = world_h * zoom;

    // Convert to SDL coords (top-left origin)
    // CRITICAL: Y-axis flip from bottom-left game coords to top-left SDL coords
    float sdl_left   = screen_left;
    float sdl_top    = static_cast<float>(win_h) - screen_bottom - screen_h;
    float sdl_right  = sdl_left + screen_w;
    float sdl_bottom = sdl_top + screen_h;

    // Border thickness in pixels (screen space, not affected by zoom)
    constexpr float BORDER_WIDTH = 5.0f;
    // Dash segment length in pixels
    constexpr float DASH_LEN = 15.0f;

    // Colors for the dashed pattern: black, red, green
    struct { uint8_t r, g, b; } colors[] = {
        {0, 0, 0}, {255, 0, 0}, {0, 255, 0}
    };
    constexpr int NUM_COLORS = 3;

    // Helper lambda: draw a dashed line as a series of filled rectangles
    // along a horizontal or vertical strip
    auto draw_dashed_strip = [&](float x0, float y0, float length, bool horizontal) {
        float pos = 0.0f;
        int color_idx = 0;
        while (pos < length) {
            float seg = (pos + DASH_LEN > length) ? (length - pos) : DASH_LEN;
            auto& c = colors[color_idx % NUM_COLORS];
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, 255);

            SDL_FRect seg_rect;
            if (horizontal) {
                seg_rect = {x0 + pos, y0, seg, BORDER_WIDTH};
            } else {
                seg_rect = {x0, y0 + pos, BORDER_WIDTH, seg};
            }
            SDL_RenderFillRect(renderer_, &seg_rect);

            pos += DASH_LEN;
            color_idx++;
        }
    };

    // Draw 4 border strips (outside the world rect):
    // Top edge: above the world top
    draw_dashed_strip(sdl_left - BORDER_WIDTH, sdl_top - BORDER_WIDTH,
                      screen_w + 2.0f * BORDER_WIDTH, true);
    // Bottom edge: below the world bottom
    draw_dashed_strip(sdl_left - BORDER_WIDTH, sdl_bottom,
                      screen_w + 2.0f * BORDER_WIDTH, true);
    // Left edge: left of the world
    draw_dashed_strip(sdl_left - BORDER_WIDTH, sdl_top,
                      screen_h, false);
    // Right edge: right of the world
    draw_dashed_strip(sdl_right, sdl_top,
                      screen_h, false);
}
