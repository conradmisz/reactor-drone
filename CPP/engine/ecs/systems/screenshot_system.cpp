/**
 * screenshot_system.cpp - Captures a BMP screenshot when signalled via the Blackboard
 */

#include "engine/ecs/systems/screenshot_system.hpp"

#include <SDL3/SDL.h>
#include <iomanip>
#include <iostream>
#include <sstream>

ScreenshotSystem::ScreenshotSystem(SDL_Renderer* renderer, std::string log_dir)
    : renderer_(renderer), log_dir_(std::move(log_dir)) {}

void ScreenshotSystem::update(Blackboard& blackboard) {
    uint64_t frame = 0;
    try {
        frame = blackboard.get<uint64_t>("screenshot_frame");
    } catch (...) {
        return;  // key not set — nothing to do
    }

    // Clear the signal immediately so it fires only once
    blackboard.remove("screenshot_frame");

    SDL_Surface* surface = SDL_RenderReadPixels(renderer_, nullptr);
    if (!surface) {
        std::cerr << "[Screenshot] SDL_RenderReadPixels failed: "
                  << SDL_GetError() << "\n";
        return;
    }

    std::ostringstream path;
    path << log_dir_ << "/"
         << std::setw(6) << std::setfill('0') << frame
         << "-screenshot.bmp";

    if (!SDL_SaveBMP(surface, path.str().c_str())) {
        std::cerr << "[Screenshot] SDL_SaveBMP failed: " << SDL_GetError() << "\n";
    } else {
        std::cout << "  [Screenshot] Frame " << frame
                  << " → " << path.str() << "\n";
    }

    SDL_DestroySurface(surface);
}
