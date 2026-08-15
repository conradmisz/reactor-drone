#include "engine/ecs/systems/bloom_system.hpp"

namespace {

SDL_Texture* make_target(SDL_Renderer* r, int w, int h) {
    SDL_Texture* t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA8888,
                                       SDL_TEXTUREACCESS_TARGET, w, h);
    if (t) {
        // Linear filtering is the whole blur; nearest would make the chain a
        // pixelated downscale instead of a widening halo.
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    }
    return t;
}

} // namespace

BloomSystem::BloomSystem(SDL_Renderer* renderer, int logical_w, int logical_h,
                         const BloomConfig& config)
    : renderer_(renderer), config_(config), w_(logical_w), h_(logical_h) {
    if (!config_.enabled || !renderer_) return;

    const auto sizes = bloom_math::chain_sizes(w_, h_, config_.levels);
    if (sizes.empty()) return;

    scene_ = make_target(renderer_, w_, h_);
    if (!scene_) return;
    emissive_ = make_target(renderer_, w_, h_);
    if (!emissive_) { destroy_targets(); return; }

    for (const auto& s : sizes) {
        SDL_Texture* t = make_target(renderer_, s.w, s.h);
        if (!t) { destroy_targets(); return; }
        chain_.push_back(t);
    }
    active_ = true;
}

BloomSystem::~BloomSystem() {
    destroy_targets();
}

void BloomSystem::destroy_targets() {
    for (SDL_Texture* t : chain_) SDL_DestroyTexture(t);
    chain_.clear();
    if (scene_) { SDL_DestroyTexture(scene_); scene_ = nullptr; }
    if (emissive_) { SDL_DestroyTexture(emissive_); emissive_ = nullptr; }
    active_ = false;
}

void BloomSystem::begin() {
    if (!active_) return;
    // v3 Tier 4 (D197): remember where the composite should land. Normally the
    // backbuffer (nullptr); when PostFxSystem is live, main.cpp has already set
    // its frame target, and resolve() must restore THAT, not hard-code null.
    dest_ = SDL_GetRenderTarget(renderer_);
    SDL_SetRenderTarget(renderer_, scene_);
    // The frame's first draw is a full clear (render_layers), but clear anyway
    // so a config with no backdrop layers still starts from black, not stale
    // last-frame pixels.
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
}

void BloomSystem::begin_emissive() {
    if (!active_) return;
    SDL_SetRenderTarget(renderer_, emissive_);
    // Transparent black: the emissive target holds only what should halo.
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0);
    SDL_RenderClear(renderer_);
}

void BloomSystem::resolve() {
    if (!active_) return;

    // Walk the EMISSIVE target down the chain (Tier 2) — dark hulls, backdrop
    // and HUD text stay out of the halo. Each RenderTexture into a half-size
    // target is one linearly-filtered box blur; by the last level the halo is
    // wide and soft.
    // v3 Tier 12: each step is a FOUR-TAP downsample, not one straight blit.
    // A plain half-size linear blit is a 2x2 box average, and a chain of box
    // averages spreads light into an axis-aligned BOX — that is why every
    // entity radiated a square halo even after the particles themselves stopped
    // being squares (bugs/004 fixed the source; this fixes the kernel). Four
    // half-texel diagonal taps, averaged, is the Kawase trick: the same cost
    // class, a far rounder falloff.
    //
    // The alphas are progressive equal-weight averaging: writing tap i over the
    // running mean of i taps with alpha 255/(i+1) leaves the mean of i+1 taps.
    // ...but only from level 1 down. The boxiness comes from the DEEP levels,
    // where one texel covers 8-16 screen pixels; level 0 is half-res and is by
    // far the most expensive target in the chain, so paying 4x there bought
    // nothing visible and cost ~10% of frame time (139.5s -> 154s on the
    // canary). Level 0 keeps its single blit.
    auto blur_step = [&](SDL_Texture* from, SDL_Texture* to, bool multi) {
        float dw = 0.0f, dh = 0.0f;
        SDL_GetTextureSize(to, &dw, &dh);
        constexpr float O = 0.5f;   // half a DESTINATION texel = one source texel
        const float tx[4] = {-O, +O, -O, +O};
        const float ty[4] = {-O, -O, +O, +O};
        const Uint8 alpha[4] = {255, 128, 85, 64};
        SDL_SetRenderTarget(renderer_, to);
        if (!multi) {
            SDL_SetTextureBlendMode(from, SDL_BLENDMODE_NONE);
            SDL_RenderTexture(renderer_, from, nullptr, nullptr);
            return;
        }
        for (int i = 0; i < 4; ++i) {
            SDL_FRect r{tx[i], ty[i], dw, dh};
            SDL_SetTextureBlendMode(from, i == 0 ? SDL_BLENDMODE_NONE
                                                 : SDL_BLENDMODE_BLEND);
            SDL_SetTextureAlphaMod(from, alpha[i]);
            SDL_RenderTexture(renderer_, from, nullptr, &r);
        }
        SDL_SetTextureAlphaMod(from, 255);
        SDL_SetTextureBlendMode(from, SDL_BLENDMODE_NONE);
    };

    SDL_SetTextureBlendMode(emissive_, SDL_BLENDMODE_NONE);
    SDL_Texture* src = emissive_;
    for (std::size_t i = 0; i < chain_.size(); ++i) {
        blur_step(src, chain_[i], i > 0);
        src = chain_[i];
    }

    // Back to the destination captured at begin(): scene 1:1, then the chain
    // additively on top.
    SDL_SetRenderTarget(renderer_, dest_);
    SDL_SetTextureBlendMode(scene_, SDL_BLENDMODE_NONE);
    SDL_RenderTexture(renderer_, scene_, nullptr, nullptr);
    for (std::size_t i = 0; i < chain_.size(); ++i) {
        SDL_Texture* t = chain_[i];
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD);
        SDL_SetTextureAlphaMod(t, bloom_math::level_alpha(
            config_.intensities, i, config_.default_intensity));
        SDL_RenderTexture(renderer_, t, nullptr, nullptr);
        SDL_SetTextureAlphaMod(t, 255);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
    }
}
