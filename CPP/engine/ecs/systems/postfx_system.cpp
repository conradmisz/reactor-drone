#include "engine/ecs/systems/postfx_system.hpp"

#include <cstring>
#include <string>

namespace {

/** The uniform block pushed to slot 0 — must match PostFx in postfx.frag.glsl
 *  (eight tightly packed floats; std140 scalar alignment is 4 bytes). */
struct PostFxUniforms {
    float aberration;
    float vignette;
    float shock_x;
    float shock_y;
    float shock_radius;
    float shock_amp;
    float saturation;
    float gain;
};

} // namespace

PostFxSystem::PostFxSystem(SDL_Renderer* renderer, const std::string& spv_path,
                           const PostFxConfig& config)
    : renderer_(renderer), config_(config) {
    if (!config_.enabled || !renderer_) return;

    // GPU renderer only — the render-state API exists solely on that backend.
    device_ = SDL_GetGPURendererDevice(renderer_);
    if (!device_) return;

    size_t code_size = 0;
    void* code = SDL_LoadFile(spv_path.c_str(), &code_size);
    if (!code) {
        SDL_Log("PostFx: no compiled shader at %s — post-processing off", spv_path.c_str());
        return;
    }

    SDL_GPUShaderCreateInfo sci;
    SDL_zero(sci);
    sci.code = static_cast<const Uint8*>(code);
    sci.code_size = code_size;
    sci.entrypoint = "main";
    sci.format = SDL_GPU_SHADERFORMAT_SPIRV;
    sci.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    sci.num_samplers = 1;          // u_frame (the texture being drawn)
    sci.num_uniform_buffers = 1;   // PostFx block
    shader_ = SDL_CreateGPUShader(device_, &sci);
    SDL_free(code);
    if (!shader_) {
        SDL_Log("PostFx: shader create failed: %s", SDL_GetError());
        return;
    }

    SDL_GPURenderStateCreateInfo rsci;
    SDL_zero(rsci);
    rsci.fragment_shader = shader_;
    state_ = SDL_CreateGPURenderState(renderer_, &rsci);
    if (!state_) {
        SDL_Log("PostFx: render state create failed: %s", SDL_GetError());
        SDL_ReleaseGPUShader(device_, shader_);
        shader_ = nullptr;
        return;
    }

    // The frame target the whole composite lands in (see begin()).
    SDL_RendererLogicalPresentation mode;
    if (!SDL_GetRenderLogicalPresentation(renderer_, &w_, &h_, &mode) ||
        w_ <= 0 || h_ <= 0) {
        SDL_GetCurrentRenderOutputSize(renderer_, &w_, &h_);
    }
    frame_copy_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                    SDL_TEXTUREACCESS_TARGET, w_, h_);
    if (!frame_copy_) return;
    SDL_SetTextureScaleMode(frame_copy_, SDL_SCALEMODE_LINEAR);
    active_ = true;
}

PostFxSystem::~PostFxSystem() {
    // The device may still be executing the last frame's command buffers;
    // releasing the shader/state under it hangs (or faults) in the driver.
    // DELIBERATE LEAK of state_/shader_ (D197). On SDL 3.5.0-prerelease,
    // destroying a GPU render state (and SDL_WaitForGPUIdle itself) wedges the
    // device at shutdown — bisected: create the objects, never use them, hang;
    // leak them, full pipeline live, clean exit. PostFxSystem lives for the
    // whole process, so the OS reclaims them microseconds later. Revisit when
    // the local SDL is updated: try restoring WaitForGPUIdle + destroy here.
    if (frame_copy_) SDL_DestroyTexture(frame_copy_);
}

void PostFxSystem::trigger_shock(float x, float y) {
    if (!active_) return;
    shock_t_ = 0.0f;
    shock_x_ = x;
    shock_y_ = y;
}

void PostFxSystem::update(float dt) {
    if (shock_t_ >= 0.0f) {
        shock_t_ += dt;
        if (shock_t_ > config_.shock_duration) shock_t_ = -1.0f;
    }
}

void PostFxSystem::apply() {
    if (!active_ || SDL_getenv("POSTFX_NO_ROUTE") != nullptr) return;

    // main.cpp routed the whole composite into frame_copy_ (it was made the
    // render target before bloom.begin(), and bloom's resolve restores to it).
    // Draw it back to the backbuffer through the shader.
    SDL_SetRenderTarget(renderer_, nullptr);

    PostFxUniforms u;
    u.aberration = config_.aberration;
    u.vignette = config_.vignette;
    u.shock_x = shock_x_;
    u.shock_y = shock_y_;
    if (shock_t_ >= 0.0f && config_.shock_duration > 0.0f) {
        float t = shock_t_ / config_.shock_duration;       // 0..1
        u.shock_radius = 0.05f + 0.55f * t;                // ring grows outward
        u.shock_amp = config_.shock_amp * (1.0f - t);      // and fades
    } else {
        u.shock_radius = 0.0f;
        u.shock_amp = 0.0f;                                // branch off in-shader
    }
    u.saturation = config_.saturation;
    u.gain = config_.gain;

    SDL_SetGPURenderStateFragmentUniforms(state_, 0, &u, sizeof(u));
    SDL_SetGPURenderState(renderer_, state_);
    SDL_SetTextureBlendMode(frame_copy_, SDL_BLENDMODE_NONE);
    SDL_RenderTexture(renderer_, frame_copy_, nullptr, nullptr);
    SDL_SetGPURenderState(renderer_, nullptr);
}

SDL_Texture* PostFxSystem::frame_target() {
    return active_ ? frame_copy_ : nullptr;
}
