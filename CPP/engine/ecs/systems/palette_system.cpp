#include "engine/ecs/systems/palette_system.hpp"

#include <algorithm>

PaletteSystem::~PaletteSystem() {
    if (target_ != nullptr) SDL_DestroyTexture(target_);
}

bool PaletteSystem::begin_capture(SDL_Renderer* renderer, int width, int height) {
    capturing_ = false;
    if (renderer == nullptr || width <= 0 || height <= 0) return false;

    if (target_ == nullptr || width != width_ || height != height_) {
        if (target_ != nullptr) SDL_DestroyTexture(target_);
        target_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                    SDL_TEXTUREACCESS_TARGET, width, height);
        if (target_ == nullptr) return false;
        width_ = width;
        height_ = height;
        SDL_SetTextureBlendMode(target_, SDL_BLENDMODE_BLEND);
    }

    if (!SDL_SetRenderTarget(renderer, target_)) return false;
    // The capture texture is not the backbuffer: it carries last frame's pixels
    // until something clears it, and a frame that only partially covers it would
    // otherwise ghost.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    capturing_ = true;
    return true;
}

void PaletteSystem::resolve(SDL_Renderer* renderer, const Palette& p) {
    if (!capturing_ || renderer == nullptr || target_ == nullptr) return;
    capturing_ = false;
    SDL_SetRenderTarget(renderer, nullptr);

    const float mix = std::min(1.0f, std::max(0.0f, p.mix));

    // Pass 1 — the frame, modulated toward the palette's shadow colour. At
    // mix 0 the modulation is white, i.e. the untouched frame, which is what
    // makes a zero-mix palette a genuine pass-through rather than "nearly".
    auto lerp255 = [&](uint8_t c) {
        return static_cast<uint8_t>(255.0f + (static_cast<float>(c) - 255.0f) * mix);
    };
    SDL_SetTextureBlendMode(target_, SDL_BLENDMODE_NONE);
    SDL_SetTextureColorMod(target_, lerp255(p.shadow_r), lerp255(p.shadow_g),
                           lerp255(p.shadow_b));
    SDL_SetTextureAlphaMod(target_, 255);
    SDL_RenderTexture(renderer, target_, nullptr, nullptr);

    if (mix <= 0.0f) {
        SDL_SetTextureColorMod(target_, 255, 255, 255);
        return;
    }

    // Pass 2 — the same frame added back in the highlight colour, weighted by
    // the mix. Additive blending uses the frame's own luminance as the weight, so
    // bright pixels take the highlight and dark ones keep the shadow: a duotone
    // resolve out of two draw calls, with no per-pixel CPU work at all.
    SDL_SetTextureBlendMode(target_, SDL_BLENDMODE_ADD);
    // The 0.85 is a LUMINANCE-MATCHING gain, measured rather than guessed: pass 1
    // multiplies the frame down toward the shadow colour, so without enough added
    // back the resolve reads as "the game got dimmer" instead of "the game got
    // recoloured". At 0.5 the mean frame luminance fell ~33% against the
    // unresolved frame (44/31/21 -> 30/15/9 on a wave-6 capture); this is the knob
    // to turn if a playtest still finds it dark, and it wants a human eye rather
    // than another mean-pixel measurement.
    constexpr float LIGHT_GAIN = 0.85f;
    SDL_SetTextureColorMod(target_,
                           static_cast<uint8_t>(p.light_r * mix * LIGHT_GAIN),
                           static_cast<uint8_t>(p.light_g * mix * LIGHT_GAIN),
                           static_cast<uint8_t>(p.light_b * mix * LIGHT_GAIN));
    SDL_RenderTexture(renderer, target_, nullptr, nullptr);

    // Leave the texture neutral: the next frame's capture clears it, but its
    // modulation state persists, and a stale colour mod would tint the capture.
    SDL_SetTextureColorMod(target_, 255, 255, 255);
    SDL_SetTextureBlendMode(target_, SDL_BLENDMODE_BLEND);
}
