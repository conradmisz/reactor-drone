#include "engine/ecs/systems/scar_system.hpp"

#include <algorithm>
#include <cmath>

namespace {

/// Edge length of the procedural scorch sprite. 64 is enough for a soft falloff
/// at the sizes a death stamps (24-90 px) and costs 16 KB once.
constexpr int STAMP_SIZE = 64;

/// World px a stamp of scale 1.0 covers.
constexpr float STAMP_WORLD_SIZE = 56.0f;

}  // namespace

ScarSystem::~ScarSystem() {
    if (target_ != nullptr) SDL_DestroyTexture(target_);
    if (stamp_ != nullptr) SDL_DestroyTexture(stamp_);
}

bool ScarSystem::ensure_stamp_texture(SDL_Renderer* renderer) {
    if (stamp_ != nullptr) return true;

    // A radial scorch: opaque-ish in the middle, falling to nothing at the rim,
    // with the centre darkened rather than coloured so it reads as burnt floor
    // under any arena palette (and stays correct if Lane W's palette engine
    // recolours the world).
    SDL_Surface* surf = SDL_CreateSurface(STAMP_SIZE, STAMP_SIZE, SDL_PIXELFORMAT_RGBA32);
    if (surf == nullptr) return false;
    SDL_LockSurface(surf);
    auto* px = static_cast<uint32_t*>(surf->pixels);
    const int pitch = surf->pitch / 4;
    const float half = STAMP_SIZE * 0.5f;
    for (int y = 0; y < STAMP_SIZE; ++y) {
        for (int x = 0; x < STAMP_SIZE; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f - half) / half;
            const float dy = (static_cast<float>(y) + 0.5f - half) / half;
            const float d = std::sqrt(dx * dx + dy * dy);
            float a = d >= 1.0f ? 0.0f : (1.0f - d);
            a *= a;                      // squared falloff: a tighter, sootier core
            // A faint warm rim keeps the mark from reading as a grey blob.
            const float rim = (d > 0.55f && d < 1.0f) ? (1.0f - d) * 0.5f : 0.0f;
            const uint8_t r = static_cast<uint8_t>(40 + 150 * rim);
            const uint8_t g = static_cast<uint8_t>(28 + 70 * rim);
            const uint8_t b = static_cast<uint8_t>(24 + 30 * rim);
            const uint8_t al = static_cast<uint8_t>(std::min(255.0f, a * 255.0f));
            px[y * pitch + x] = (static_cast<uint32_t>(al) << 24) |
                                (static_cast<uint32_t>(b) << 16) |
                                (static_cast<uint32_t>(g) << 8) |
                                static_cast<uint32_t>(r);
        }
    }
    SDL_UnlockSurface(surf);
    stamp_ = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    if (stamp_ == nullptr) return false;
    SDL_SetTextureBlendMode(stamp_, SDL_BLENDMODE_BLEND);
    return true;
}

bool ScarSystem::configure(SDL_Renderer* renderer, int width, int height,
                           float origin_x, float origin_y) {
    origin_x_ = origin_x;
    origin_y_ = origin_y;
    if (renderer == nullptr || width <= 0 || height <= 0) return false;
    if (target_ != nullptr && width == width_ && height == height_) return true;

    if (target_ != nullptr) SDL_DestroyTexture(target_);
    target_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                SDL_TEXTUREACCESS_TARGET, width, height);
    if (target_ == nullptr) return false;
    width_ = width;
    height_ = height;
    SDL_SetTextureBlendMode(target_, SDL_BLENDMODE_BLEND);
    clear(renderer);
    return ensure_stamp_texture(renderer);
}

void ScarSystem::clear(SDL_Renderer* renderer) {
    if (target_ == nullptr || renderer == nullptr) return;
    SDL_Texture* prev = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, target_);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderTarget(renderer, prev);
}

void ScarSystem::update_and_render(SDL_Renderer* renderer, const Blackboard& blackboard,
                                   const std::vector<fx_events::Stamp>& stamps) {
    if (renderer == nullptr || target_ == nullptr || stamp_ == nullptr) return;

    // --- accumulate -------------------------------------------------------
    if (!stamps.empty() && max_per_frame_ > 0) {
        SDL_Texture* prev = SDL_GetRenderTarget(renderer);
        SDL_SetRenderTarget(renderer, target_);
        const int budget = std::min(static_cast<int>(stamps.size()), max_per_frame_);
        for (int i = 0; i < budget; ++i) {
            const fx_events::Stamp& s = stamps[static_cast<size_t>(i)];
            const float size = STAMP_WORLD_SIZE * std::max(0.1f, s.scale);
            // Texture space: x is world-relative to the origin, y is FLIPPED —
            // the world is bottom-left origin and a texture is top-left. This is
            // the only place this layer flips, and it is a texture-local flip,
            // not a third world-space flip (see ENGINE.md §4).
            SDL_FRect dst{s.x - origin_x_ - size * 0.5f,
                          static_cast<float>(height_) - (s.y - origin_y_) - size * 0.5f,
                          size, size};
            SDL_SetTextureAlphaModFloat(stamp_, std::min(1.0f, std::max(0.0f, alpha_)));
            SDL_RenderTextureRotated(renderer, stamp_, nullptr, &dst,
                                     static_cast<double>(s.angle) * 57.2957795,
                                     nullptr, SDL_FLIP_NONE);
            ++stamps_applied_;
        }
        SDL_SetRenderTarget(renderer, prev);
    }

    // --- draw the accumulated floor ---------------------------------------
    // Same world→screen transform as RenderSystem::render, so the scars sit in
    // the world and track the follow camera and the shake.
    const float lookat_x = blackboard.get_or<float>("camera.lookat.x", 0.0f);
    const float lookat_y = blackboard.get_or<float>("camera.lookat.y", 0.0f);
    const float zoom = blackboard.get_or<float>("camera.zoom", 1.0f);
    const int win_w = blackboard.get_or<int>("window_width", 800);
    const int win_h = blackboard.get_or<int>("window_height", 600);
    if (zoom <= 0.0f) return;

    const float cam_left = lookat_x - (static_cast<float>(win_w) / zoom) * 0.5f;
    const float cam_bottom = lookat_y - (static_cast<float>(win_h) / zoom) * 0.5f;
    const float w = static_cast<float>(width_) * zoom;
    const float h = static_cast<float>(height_) * zoom;
    SDL_FRect dst{(origin_x_ - cam_left) * zoom,
                  static_cast<float>(win_h) - (origin_y_ - cam_bottom) * zoom - h,
                  w, h};
    SDL_RenderTexture(renderer, target_, nullptr, &dst);
}
