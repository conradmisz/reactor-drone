/**
 * ScreenFadeSystem implementation (Phase 6, o-040-06-lua-screens).
 *
 * Detects screen-stack transitions by snapshotting the stack signature (depth +
 * top-screen name) frame-to-frame, so ScreenStackSystem stays frozen. On a
 * change it restarts the fade (progress 0); otherwise it advances progress by
 * delta_time / FADE_DURATION. render() draws a full-window black overlay whose
 * alpha follows the pure fade_overlay_alpha curve.
 *
 * Note: delta_time is stored on the Blackboard as a double (Timer writes it as
 * double), so it is read with get_or<double> — get_or throws on a type mismatch,
 * it does NOT fall back to the default. ui.fade.progress is owned entirely by
 * this system and kept as a float.
 */

#include "engine/ecs/systems/screen_fade_system.hpp"
#include "engine/ecs/systems/screen_stack_system.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ui_fade_math.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

ScreenFadeSystem::ScreenFadeSystem(SDL_Renderer* renderer, int window_w, int window_h)
    : renderer_(renderer), window_w_(window_w), window_h_(window_h) {
}

void ScreenFadeSystem::update(Blackboard& blackboard) {
    // Current stack signature: depth + the back (top) screen name. At depth 1
    // the top is the base sentinel (gameplay).
    std::vector<std::string> stack = ScreenStackSystem::get_stack(blackboard);
    int depth = static_cast<int>(stack.size());
    std::string top = stack.empty() ? std::string(ScreenStackSystem::BASE_SCREEN)
                                     : stack.back();

    if (depth != last_depth_ || top != last_top_) {
        // Transition detected (including the first frame): restart the fade.
        blackboard.set<float>(PROGRESS_KEY, 0.0f);
        last_depth_ = depth;
        last_top_ = top;
        return;
    }

    // No transition: advance any in-flight fade toward 1.0 (idle).
    float progress = blackboard.get_or<float>(PROGRESS_KEY, 1.0f);
    if (progress < 1.0f) {
        double dt = blackboard.get_or<double>("delta_time", 0.0);
        progress = std::min(1.0f, progress + static_cast<float>(dt) / FADE_DURATION);
        blackboard.set<float>(PROGRESS_KEY, progress);
    }
}

void ScreenFadeSystem::render(const Blackboard& blackboard) {
    int a = fade_overlay_alpha(blackboard.get_or<float>(PROGRESS_KEY, 1.0f));
    if (a <= 0) return;

    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, static_cast<Uint8>(a));
    SDL_FRect overlay{0.0f, 0.0f,
                      static_cast<float>(window_w_),
                      static_cast<float>(window_h_)};
    SDL_RenderFillRect(renderer_, &overlay);
}
