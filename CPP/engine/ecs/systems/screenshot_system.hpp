/**
 * screenshot_system.hpp - Captures a BMP screenshot when signalled via the Blackboard
 *
 * Call update() once per frame after render_system.present(). If the Blackboard
 * key "screenshot_frame" is set, the system reads pixels from the renderer,
 * saves a zero-padded BMP file to the log directory, then clears the key.
 *
 * Signal protocol:
 *   Set:   blackboard.set("screenshot_frame", frame_count)   // before update()
 *   Clear: done automatically inside update()
 *
 * Output filename: <log_dir>/<NNNNNN>-screenshot.bmp
 */

#ifndef SCREENSHOT_SYSTEM_HPP
#define SCREENSHOT_SYSTEM_HPP

#include <SDL3/SDL.h>
#include <string>
#include "engine/ecs/blackboard.hpp"

class ScreenshotSystem {
public:
    /**
     * @param renderer  The SDL renderer to read pixels from
     * @param log_dir   Directory where BMP files are written
     */
    ScreenshotSystem(SDL_Renderer* renderer, std::string log_dir);

    /**
     * Capture a screenshot if "screenshot_frame" is present on the Blackboard.
     * Clears the key after capture regardless of success.
     */
    void update(Blackboard& blackboard);

private:
    SDL_Renderer* renderer_;
    std::string   log_dir_;
};

#endif // SCREENSHOT_SYSTEM_HPP
