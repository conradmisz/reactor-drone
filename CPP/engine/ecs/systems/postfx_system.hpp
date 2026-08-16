/**
 * PostFxSystem — full-screen fragment-shader post-process (v3 Tier 4, D210).
 *
 * Runs ONLY on the SDL GPU renderer: a precompiled SPIR-V fragment shader
 * (assets/shaders/postfx.frag.spv, built offline by assets/shaders/make.sh)
 * is attached to an ordinary full-screen SDL_RenderTexture draw through
 * SDL_CreateGPURenderState. One pass applies chromatic aberration, vignette,
 * an optional radial shockwave, and a saturation/gain grade — all authored in
 * GameData's optional "postfx" block.
 *
 * Degrades to a no-op exactly like BloomSystem: not a GPU renderer, missing
 * .spv, disabled config, or any create failure → apply() draws nothing and
 * the frame presents exactly as composited. The classic-renderer path (and
 * every headless run) therefore never changes.
 *
 * Presentation-only: reads sim state (via the uniforms main.cpp pushes),
 * writes pixels. Nothing reads it back.
 */
#pragma once

#include <SDL3/SDL.h>
#include <string>

/** Authored in GameData.json under the optional top-level "postfx" block. */
struct PostFxConfig {
    bool enabled = true;
    float aberration = 0.0015f;   // chromatic aberration, UV units
    float vignette = 0.32f;       // edge darkening [0,1]
    float saturation = 1.12f;     // 1 = neutral
    float gain = 1.02f;           // 1 = neutral
    float shock_duration = 0.9f;  // seconds a triggered shockwave lives
    float shock_amp = 0.035f;     // peak UV displacement
};

class PostFxSystem {
public:
    /**
     * @param renderer   The live renderer (any backend — non-GPU disables).
     * @param spv_path   Absolute path to the compiled fragment shader.
     */
    PostFxSystem(SDL_Renderer* renderer, const std::string& spv_path,
                 const PostFxConfig& config);
    ~PostFxSystem();

    PostFxSystem(const PostFxSystem&) = delete;
    PostFxSystem& operator=(const PostFxSystem&) = delete;

    /** Start a shockwave at (x, y) in [0,1] UV space (0,0 = top-left). */
    void trigger_shock(float x, float y);

    /** Advance the shock timer. Call once per simulated frame. */
    void update(float dt);

    /**
     * Post-process the current backbuffer: copy it into an internal target,
     * then draw it back through the shader. Call after the last composite
     * (bloom resolve) and before screenshot/present. No-op when inactive.
     */
    void apply();

    bool active() const { return active_; }

    /**
     * The target the whole frame should composite into when post-fx is live
     * (main.cpp makes it the render target before bloom.begin(), and bloom's
     * resolve restores to it). nullptr when inactive — i.e. "the backbuffer".
     */
    SDL_Texture* frame_target();

private:
    SDL_Renderer* renderer_ = nullptr;
    PostFxConfig config_;
    SDL_GPUDevice* device_ = nullptr;         // owned by the renderer, not us
    SDL_GPUShader* shader_ = nullptr;
    struct SDL_GPURenderState* state_ = nullptr;
    SDL_Texture* frame_copy_ = nullptr;       // backbuffer snapshot target
    int w_ = 0, h_ = 0;
    float shock_t_ = -1.0f;                   // <0 = no live shockwave
    float shock_x_ = 0.5f, shock_y_ = 0.5f;
    bool active_ = false;
};
