/**
 * InputSystem implementation
 * 
 * Uses SDL_GetKeyboardState for continuous key polling (no OS repeat delay).
 * SDL events are only used for quit/close detection.
 * Each frame, the keyboard state array is read and Input component flags
 * are set directly from the current physical key state.
 */

#include "engine/ecs/systems/input_system.hpp"
#include <iostream>

void InputSystem::process_events(ComponentStorage& storage, bool& running, Blackboard& blackboard,
                                 SDL_Renderer* renderer) {
    // Reset per-frame mouse click state
    blackboard.set("mouse.clicked", false);
    // Reset the per-frame edges consumed by UISystem (Option-040 port). Cleared
    // unconditionally at the top so a frame with no event leaves them all false.
    blackboard.set("mouse.down", false);
    blackboard.set("mouse.up", false);
    blackboard.set("ui.escape_pressed", false);
    blackboard.set("ui.tab_pressed", false);
    blackboard.set("ui.enter_pressed", false);

    // 1. Process SDL events — for quit/close detection and mouse clicks
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
                // One edge per physical press: the OS auto-repeat would otherwise
                // fire Escape/Tab/Enter every frame the key is held.
                if (event.key.repeat) break;
                if (event.key.key == SDLK_ESCAPE) {
                    // Publish a one-frame edge. main.cpp decides what Escape means:
                    // it pushes the pause screen during gameplay and still quits
                    // when no menu layer wants it.
                    blackboard.set("ui.escape_pressed", true);
                } else if (event.key.key == SDLK_TAB) {
                    blackboard.set("ui.tab_pressed", true);
                } else if (event.key.key == SDLK_RETURN ||
                           event.key.key == SDLK_KP_ENTER) {
                    blackboard.set("ui.enter_pressed", true);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    blackboard.set("mouse.clicked", true);
                    blackboard.set("mouse.down", true);
                    blackboard.set("mouse_click_x", event.button.x);
                    blackboard.set("mouse_click_y", event.button.y);
                    // Log click for debug script building
                    uint64_t frame = blackboard.get_or<uint64_t>("frame_count", 0);
                    std::cout << "  [Click] Frame " << frame
                              << " → " << static_cast<int>(event.button.x)
                              << "," << static_cast<int>(event.button.y) << "\n";
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    blackboard.set("mouse.up", true);
                }
                break;
            case SDL_EVENT_QUIT:
                running = false;
                break;
            default:
                break;
        }
    }

    // 2. Poll keyboard state for continuous input (no repeat delay)
    const bool* keys = SDL_GetKeyboardState(nullptr);

    auto entities = storage.entities_with_component<Input>();
    for (Entity entity : entities) {
        auto input_opt = storage.get_component<Input>(entity);
        if (input_opt.has_value()) {
            Input& input = input_opt->get();
            // Arrows and WASD both drive the same flags (WASD is free — the
            // camera-pan system that used it is not wired into the game).
            input.up    = keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W];
            input.down  = keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S];
            input.left  = keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A];
            input.right = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
            input.fire  = keys[SDL_SCANCODE_SPACE];
        }
    }

    // 3. Poll mouse position and convert to world coordinates
    //    Inverse of CameraSystem transform: screen → world
    float screen_x, screen_y;
    SDL_GetMouseState(&screen_x, &screen_y);
    // SDL_GetMouseState is in *window* pixels; the blackboard size below is the
    // renderer's logical size. Under a logical presentation (letterbox) those differ.
    if (renderer) {
        SDL_RenderCoordinatesFromWindow(renderer, screen_x, screen_y, &screen_x, &screen_y);
    }

    // Publish the pointer in *logical* screen pixels for UISystem. Taken after
    // the conversion above, so it shares one space with window_width/height and
    // with the widget rects — the game's world-space mouse.x/y is a different
    // space entirely and must not be reused for hit-testing.
    blackboard.set<float>("mouse.screen_x", screen_x);
    blackboard.set<float>("mouse.screen_y", screen_y);

    int win_w = blackboard.get_or<int>("window_width", 800);
    int win_h = blackboard.get_or<int>("window_height", 600);
    float zoom = blackboard.get_or<float>("camera.zoom", 1.0f);
    float lookat_x = blackboard.get_or<float>("camera.lookat.x", 0.0f);
    float lookat_y = blackboard.get_or<float>("camera.lookat.y", 0.0f);

    // Screen to world: undo the camera affine transform
    // CameraSystem does: screen_x = (world_x - lookat_x) * zoom + win_w/2
    // Inverse:           world_x  = (screen_x - win_w/2) / zoom + lookat_x
    double world_x = (static_cast<double>(screen_x) - win_w / 2.0) / zoom + lookat_x;
    double world_y = (static_cast<double>(win_h - screen_y) - win_h / 2.0) / zoom + lookat_y;

    blackboard.set("mouse.x", world_x);
    blackboard.set("mouse.y", world_y);
}
