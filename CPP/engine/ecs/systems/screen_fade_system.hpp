#ifndef SCREEN_FADE_SYSTEM_HPP
#define SCREEN_FADE_SYSTEM_HPP

#include <string>

struct SDL_Renderer;
class Blackboard;

/**
 * ScreenFadeSystem (Phase 6, o-040-06-lua-screens) — a fade-through-black
 * transition that plays whenever the screen stack changes.
 *
 * The system detects transitions ITSELF by snapshotting the stack signature
 * (depth + top-screen name) each frame and starting a new fade when it changes,
 * so the frozen ScreenStackSystem is never modified. Live transition progress is
 * kept in the Blackboard key "ui.fade.progress" (float, 1.0 = idle) so it is
 * visible to the dump/trace path; all other state is private.
 *
 * The overlay is a full-window screen-space black rect drawn directly in SDL
 * coordinates (it covers the whole window, so no bottom-left conversion applies)
 * — drawn between the game layer and the UI layer so gameplay dims during a
 * transition while the destination menu reads crisply on top. The alpha follows
 * the pure fade_overlay_alpha curve in ui_fade_math.hpp.
 */
class ScreenFadeSystem {
public:
    static constexpr const char* PROGRESS_KEY = "ui.fade.progress";

    ScreenFadeSystem(SDL_Renderer* renderer, int window_w, int window_h);

    /// Advance the transition: detect a stack-signature change (start a new
    /// fade) or advance the in-flight progress by delta_time / FADE_DURATION.
    /// Runs every frame, next to UISystem::update (outside the simulation gate).
    void update(Blackboard& blackboard);

    /// Draw the full-window black overlay at the current alpha. A no-op when the
    /// alpha is 0 (idle / settled). Drawn between RenderSystem and UIRenderSystem.
    void render(const Blackboard& blackboard);

private:
    SDL_Renderer* renderer_;
    int window_w_;
    int window_h_;

    // Stack-signature snapshot used to detect a transition without touching
    // ScreenStackSystem. last_depth_ = -1 means "no snapshot yet".
    int last_depth_ = -1;
    std::string last_top_;
};

#endif // SCREEN_FADE_SYSTEM_HPP
