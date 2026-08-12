#version 450
// postfx.frag.glsl — v3 Tier 4 (D197) full-screen post-process for the SDL GPU
// renderer path. One pass over the composited frame: chromatic aberration,
// vignette, radial shockwave distortion, and a uniform-driven color grade.
//
// COMPILED OFFLINE, never at build time (same discipline as the PNG
// generators): make.sh -> postfx.frag.spv, committed. The engine loads the
// .spv only — a build never needs glslc.
//
// Binding model is SDL_Renderer's GPU render-state contract:
//   set 2 binding 0 = the texture being drawn (the frame)
//   set 3 binding 0 = fragment uniforms (SDL_SetGPURenderStateFragmentUniforms slot 0)
// Vertex inputs are SDL's: location 0 = color, location 1 = uv.

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 o_color;

layout(set = 2, binding = 0) uniform sampler2D u_frame;

layout(set = 3, binding = 0) uniform PostFx {
    float aberration;   // chromatic aberration strength in UV units (~0.0015)
    float vignette;     // edge darkening amount [0,1]
    float shock_x;      // shockwave centre, UV space
    float shock_y;
    float shock_radius; // current ring radius, UV units (grows over the effect)
    float shock_amp;    // displacement amplitude, UV units (decays to 0 = off)
    float saturation;   // 1 = neutral
    float gain;         // final RGB multiplier, 1 = neutral
} fx;

void main() {
    vec2 uv = v_uv;

    // Radial shockwave: displace UVs inside a thin ring around shock_radius.
    if (fx.shock_amp > 0.0) {
        vec2 to_c = uv - vec2(fx.shock_x, fx.shock_y);
        float d = length(to_c);
        float ring = d - fx.shock_radius;
        // Gaussian-ish falloff around the ring, width ~0.08 UV.
        float w = exp(-ring * ring / (2.0 * 0.04 * 0.04));
        uv += normalize(to_c + 1e-6) * ring * w * fx.shock_amp;
    }

    // Chromatic aberration: sample R/B slightly apart along the from-centre axis.
    vec2 from_c = uv - vec2(0.5);
    vec2 ab = from_c * fx.aberration * 2.0;
    float r = texture(u_frame, uv + ab).r;
    float g = texture(u_frame, uv).g;
    float b = texture(u_frame, uv - ab).b;
    vec3 rgb = vec3(r, g, b);

    // Grade: saturation about luma, then gain.
    float luma = dot(rgb, vec3(0.2126, 0.7152, 0.0722));
    rgb = mix(vec3(luma), rgb, fx.saturation) * fx.gain;

    // Vignette: smooth edge falloff.
    float vig = 1.0 - fx.vignette * smoothstep(0.45, 0.85, length(from_c));
    rgb *= vig;

    o_color = vec4(rgb, 1.0) * v_color;
}
