/**
 * InputSystem - System for processing SDL3 keyboard events
 * 
 * This system demonstrates how ECS handles input by storing input state
 * as components rather than global variables. The InputSystem processes
 * SDL events and updates Input components for all entities that have them.
 * 
 * Key Design Decisions:
 * - Input state is stored in components, not global variables
 * - All entities with Input components are updated simultaneously
 * - Supports multiple players (each entity can have its own Input component)
 * - SDL event processing is centralized in one system
 * 
 * Requirements: 2.1-2.11, 9.1-9.3
 */

#ifndef INPUT_SYSTEM_HPP
#define INPUT_SYSTEM_HPP

#include <SDL3/SDL.h>
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/blackboard.hpp"

/**
 * InputSystem class
 * 
 * Responsible for processing SDL3 keyboard events and updating Input components.
 * This system demonstrates how ECS separates input handling (system logic)
 * from input state (component data).
 */
class InputSystem {
public:
    /**
     * Process SDL events and update Input components
     * 
     * This method:
     * 1. Polls SDL events using SDL_PollEvent()
     * 2. Handles SDL_EVENT_KEY_DOWN and SDL_EVENT_KEY_UP for arrow keys
     * 3. Updates all entities with Input components
     * 4. Handles SDL_EVENT_QUIT to signal game termination
     * 
     * @param storage Component storage to query for entities and update Input components
     * @param running Reference to game loop running flag, set to false on quit event
     */
    void process_events(ComponentStorage& storage, bool& running, Blackboard& blackboard);

private:
    /**
     * Handle key down events for arrow keys
     * 
     * Sets the corresponding Input component flag to true for all entities
     * with Input components.
     * 
     * @param key SDL key code (SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT)
     * @param storage Component storage to update Input components
     */
    void handle_key_down(SDL_Keycode key, ComponentStorage& storage);
    
    /**
     * Handle key up events for arrow keys
     * 
     * Sets the corresponding Input component flag to false for all entities
     * with Input components.
     * 
     * @param key SDL key code (SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT)
     * @param storage Component storage to update Input components
     */
    void handle_key_up(SDL_Keycode key, ComponentStorage& storage);
};

#endif // INPUT_SYSTEM_HPP
