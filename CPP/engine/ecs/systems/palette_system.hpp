#ifndef PALETTE_SYSTEM_HPP
#define PALETTE_SYSTEM_HPP

#include <cstdint>

#include <SDL3/SDL.h>

/**
 * PaletteSystem — the whole frame runs through a palette (#5, Lane W, D147).
 *
 * The Downwell trick: the composed world is captured to a texture and resolved
 * through a palette before it reaches the screen, so an arena shift is a
 * world-wide colour swap and taking damage flashes the *world*, not a vignette.
 *
 * THE FEASIBILITY GATE, RESOLVED. The spec left three options open. SDL3's 2D
 * renderer has no shader hook, so a true indexed LUT would mean either a CPU pass
 * over every pixel of a 980x660 frame (~650k pixels per frame — far past the
 * budget for a cosmetic layer) or an `SDL_RenderGeometry` trick that still cannot
 * express an arbitrary index remap. What ships is the third option, and the one
 * the spec called "honestly cheapest": a **duotone resolve** of the captured
 * frame — the world is drawn once modulated toward the palette's shadow colour
 * and once additively toward its highlight colour, which recolours every pixel
 * from one table without touching a single game system.
 *
 * That is a *tone map*, not an index remap: two frames whose colours differ but
 * whose luminance matches resolve to the same output, where a real LUT could send
 * them anywhere. On bespoke hardware with an indexed framebuffer the true LUT is
 * free and this ports forward into it — the palette table is already the data it
 * would need.
 *
 * RENDER-ONLY. Palette choice may READ sim state (which arena, hull critical);
 * nothing reads back. No RNG, no sim writes, so the replay canary cannot see it.
 *
 * Disabled (`enabled: false`, the shipped default) the system never creates a
 * target and `begin_capture` returns false, so `main.cpp` renders straight to the
 * backbuffer on exactly the code path it used before the feature existed.
 */
class PaletteSystem {
public:
    ~PaletteSystem();

    PaletteSystem() = default;
    PaletteSystem(const PaletteSystem&) = delete;
    PaletteSystem& operator=(const PaletteSystem&) = delete;

    /// One palette: two anchor colours the frame is resolved between, plus how
    /// hard to push. `mix` 0 is a pass-through, 1 is full duotone.
    struct Palette {
        uint8_t shadow_r = 12, shadow_g = 16, shadow_b = 40;
        uint8_t light_r = 255, light_g = 236, light_b = 190;
        float mix = 0.0f;
    };

    /**
     * Point the renderer at the capture texture. Returns false when the feature
     * is off or the texture could not be created — the caller then draws
     * normally, so a failure here costs colour, never a frame.
     */
    bool begin_capture(SDL_Renderer* renderer, int width, int height);

    /// Resolve the captured frame to the backbuffer through `p`.
    void resolve(SDL_Renderer* renderer, const Palette& p);

    bool capturing() const { return capturing_; }

private:
    SDL_Texture* target_ = nullptr;
    int width_ = 0, height_ = 0;
    bool capturing_ = false;
};

#endif  // PALETTE_SYSTEM_HPP
