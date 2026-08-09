#include "engine/ecs/systems/screen_stack_system.hpp"

#include "engine/ecs/components.hpp"
#include "engine/ecs/blackboard.hpp"

#include <algorithm>
#include <unordered_set>

// ---------------------------------------------------------------------------
// Pure readers
// ---------------------------------------------------------------------------

std::vector<std::string> ScreenStackSystem::get_stack(const Blackboard& blackboard) {
    // Absent stack -> empty list (depth 0). initialize() establishes ["gameplay"].
    return blackboard.get_or<std::vector<std::string>>(STACK_KEY, {});
}

std::size_t ScreenStackSystem::depth(const Blackboard& blackboard) {
    return get_stack(blackboard).size();
}

bool ScreenStackSystem::is_modal(const Blackboard& blackboard) {
    return depth(blackboard) > 1;
}

// ---------------------------------------------------------------------------
// Reconciliation core — the ONLY writer of UIScreen.active (R1.5, R4.1, R4.2)
// ---------------------------------------------------------------------------

void ScreenStackSystem::reconcile_active_flags(Blackboard& blackboard, ComponentStorage& storage) {
    // Build a set of the current stack names. The base "gameplay" is included
    // but matches no UIScreen entity, so it is harmless (R4.4).
    const std::vector<std::string> stack = get_stack(blackboard);
    const std::unordered_set<std::string> names(stack.begin(), stack.end());

    // active = (screen_name in stack): true iff present, false otherwise (R4.1, R4.2).
    auto entities = storage.entities_with_component<UIScreen>();
    for (Entity entity : entities) {
        auto screen_opt = storage.get_component<UIScreen>(entity);
        if (!screen_opt.has_value()) {
            continue;
        }
        UIScreen& screen = screen_opt->get();
        screen.active = (names.count(screen.screen_name) > 0);
    }
}

// ---------------------------------------------------------------------------
// Mutators
// ---------------------------------------------------------------------------

void ScreenStackSystem::initialize(Blackboard& blackboard, ComponentStorage& storage) {
    // Establish the live truth at startup: stack = ["gameplay"] (depth 1, non-modal). (R1.2)
    blackboard.set<std::vector<std::string>>(STACK_KEY, { BASE_SCREEN });
    reconcile_active_flags(blackboard, storage);
}

void ScreenStackSystem::push_screen(const std::string& name, Blackboard& blackboard, ComponentStorage& storage) {
    // Empty name -> no-op, no throw (R2.5).
    if (name.empty()) {
        return;
    }
    std::vector<std::string> stack = get_stack(blackboard);
    stack.push_back(name);
    blackboard.set(STACK_KEY, stack);
    reconcile_active_flags(blackboard, storage);
}

void ScreenStackSystem::pop_screen(Blackboard& blackboard, ComponentStorage& storage) {
    std::vector<std::string> stack = get_stack(blackboard);
    // Base is never removed; at depth <= 1 this is a no-op (R3.5).
    if (stack.size() <= 1) {
        return;
    }
    stack.pop_back();
    blackboard.set(STACK_KEY, stack);
    reconcile_active_flags(blackboard, storage);
}

void ScreenStackSystem::clear_to(const std::string& name, Blackboard& blackboard, ComponentStorage& storage) {
    // Truncate to the base, then push name when non-empty (R11.6).
    std::vector<std::string> stack = { BASE_SCREEN };
    if (!name.empty()) {
        stack.push_back(name);
    }
    blackboard.set(STACK_KEY, stack);
    reconcile_active_flags(blackboard, storage);
}

// ---------------------------------------------------------------------------
// Command consumption — single consumer, one shot per key per frame (R5, R11.4-6)
// ---------------------------------------------------------------------------

void ScreenStackSystem::process_commands(Blackboard& blackboard, ComponentStorage& storage) {
    // Fixed order: clear_to -> push -> pop. Each command is read, removed, then applied.

    if (blackboard.has(CMD_CLEAR_TO)) {
        const std::string name = blackboard.get<std::string>(CMD_CLEAR_TO);
        blackboard.remove(CMD_CLEAR_TO);
        clear_to(name, blackboard, storage);
    }

    if (blackboard.has(CMD_PUSH)) {
        const std::string name = blackboard.get<std::string>(CMD_PUSH);
        blackboard.remove(CMD_PUSH);
        push_screen(name, blackboard, storage);
    }

    if (blackboard.get_or<bool>(CMD_POP, false)) {
        blackboard.remove(CMD_POP);
        pop_screen(blackboard, storage);
    }
}
