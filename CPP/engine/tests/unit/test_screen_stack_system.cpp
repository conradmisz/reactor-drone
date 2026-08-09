/**
 * Unit tests for ScreenStackSystem (Phase 4 — promoted full stack)
 *
 * NOTE: This file was REWRITTEN for Phase 4. The Phase 2 stub API
 * (push(name, storage) / pop(name, storage), which only toggled UIScreen.active
 * for an exact-name match and kept no ordered list) has been PROMOTED to a full
 * Blackboard-backed screen stack. This is the documented intentional API change
 * (regression-policy "API intentionally changed" exception): the stub methods no
 * longer exist, so the previous tests no longer compile and are replaced here.
 *
 * The promoted ScreenStackSystem is the single writer of both the Blackboard
 * "screen_stack" ordered list (front=base, back=top) and every UIScreen.active
 * flag, reconciling the two after each mutation. The sentinel base entry
 * "gameplay" has no UIScreen entity and is never popped.
 *
 * Verifies:
 * - R13.1: push_screen("pause") activates the pause UIScreen and appends "pause"
 *   to the stack (depth 2, back == "pause").
 * - R13.2: pop_screen() deactivates the popped screen and removes its name
 *   (depth back to 1).
 * - R13.6: after a fixed push/pop sequence, the set of active UIScreen names
 *   equals the set of stack names that correspond to a real UIScreen entity
 *   (the base "gameplay" is excluded).
 * - Edges: empty-name push, over-pop at depth 1, and a stacked name with no
 *   matching UIScreen entity ("ghost") are all no-throw and leave state
 *   consistent.
 * - process_commands: CMD_PUSH / CMD_POP / CMD_CLEAR_TO are each applied and
 *   removed from the Blackboard.
 *
 * Requirements tested: 13.1, 13.2, 13.6, 2.5, 3.5, 4.4, 5.1, 5.2, 11.4, 11.5, 11.6
 */

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/systems/screen_stack_system.hpp"

namespace {

// Create an entity and attach a UIScreen with the given name (initially inactive
// — the system reconciles the flag, so the starting value is unimportant).
Entity make_screen(EntityManager& entities, ComponentStorage& storage,
                   const std::string& name, bool active = false) {
    Entity e = entities.create_entity();
    storage.add_component(e, UIScreen{name, active});
    return e;
}

// Read back the (required) active flag for a UIScreen entity.
bool active_of(const ComponentStorage& storage, Entity e) {
    auto retrieved = storage.get_component<UIScreen>(e);
    REQUIRE(retrieved.has_value());
    return retrieved->get().active;
}

// The set of screen_names whose UIScreen.active == true.
std::set<std::string> active_screen_names(const ComponentStorage& storage) {
    std::set<std::string> names;
    for (Entity e : storage.entities_with_component<UIScreen>()) {
        auto opt = storage.get_component<UIScreen>(e);
        if (opt.has_value() && opt->get().active) {
            names.insert(opt->get().screen_name);
        }
    }
    return names;
}

// The set of stack names that correspond to a real UIScreen entity (i.e. the
// stack names minus the sentinel base "gameplay", which has no entity).
std::set<std::string> stacked_names_with_entity(const Blackboard& bb,
                                                 const ComponentStorage& storage) {
    std::set<std::string> entity_names;
    for (Entity e : storage.entities_with_component<UIScreen>()) {
        auto opt = storage.get_component<UIScreen>(e);
        if (opt.has_value()) {
            entity_names.insert(opt->get().screen_name);
        }
    }
    std::set<std::string> result;
    for (const std::string& name : ScreenStackSystem::get_stack(bb)) {
        if (entity_names.count(name) > 0) {
            result.insert(name);
        }
    }
    return result;
}

// Whether the stack contains a given name.
bool stack_contains(const Blackboard& bb, const std::string& name) {
    auto stack = ScreenStackSystem::get_stack(bb);
    return std::find(stack.begin(), stack.end(), name) != stack.end();
}

} // namespace

// ---------------------------------------------------------------------------
// R13.1 — push_screen activates the screen and appends it to the stack
// ---------------------------------------------------------------------------

TEST_CASE("ScreenStackSystem: push_screen activates the screen and appends it to the stack",
          "[Engine][screen_stack][unit]") {
    EntityManager entities;
    ComponentStorage storage;
    Blackboard bb;
    ScreenStackSystem sys;

    Entity pause = make_screen(entities, storage, "pause");

    sys.initialize(bb, storage);                 // stack = ["gameplay"], depth 1
    REQUIRE(ScreenStackSystem::depth(bb) == 1);
    REQUIRE(active_of(storage, pause) == false); // reconciled inactive at init

    sys.push_screen("pause", bb, storage);

    REQUIRE(active_of(storage, pause) == true);                   // now active
    REQUIRE(stack_contains(bb, "pause"));                         // present in stack
    REQUIRE(ScreenStackSystem::get_stack(bb).back() == "pause");  // it is the Top_Screen
    REQUIRE(ScreenStackSystem::depth(bb) == 2);                   // depth increased by one
    REQUIRE(ScreenStackSystem::is_modal(bb) == true);             // depth > 1 -> modal
}

// ---------------------------------------------------------------------------
// R13.2 — pop_screen deactivates the popped screen and removes its name
// ---------------------------------------------------------------------------

TEST_CASE("ScreenStackSystem: pop_screen deactivates the popped screen and removes its name",
          "[Engine][screen_stack][unit]") {
    EntityManager entities;
    ComponentStorage storage;
    Blackboard bb;
    ScreenStackSystem sys;

    Entity pause = make_screen(entities, storage, "pause");

    sys.initialize(bb, storage);
    sys.push_screen("pause", bb, storage);
    REQUIRE(active_of(storage, pause) == true);
    REQUIRE(ScreenStackSystem::depth(bb) == 2);

    sys.pop_screen(bb, storage);

    REQUIRE(active_of(storage, pause) == false);     // popped -> inactive
    REQUIRE_FALSE(stack_contains(bb, "pause"));      // name removed from stack
    REQUIRE(ScreenStackSystem::depth(bb) == 1);      // back to base depth
    REQUIRE(ScreenStackSystem::is_modal(bb) == false);
}

// ---------------------------------------------------------------------------
// R13.6 — after a fixed sequence, active set == stacked names with an entity
// ---------------------------------------------------------------------------

TEST_CASE("ScreenStackSystem: active-flag set agrees with the stack after a fixed sequence",
          "[Engine][screen_stack][unit]") {
    EntityManager entities;
    ComponentStorage storage;
    Blackboard bb;
    ScreenStackSystem sys;

    Entity main_menu = make_screen(entities, storage, "main_menu");
    Entity pause     = make_screen(entities, storage, "pause");

    // Fixed sequence: initialize; push main_menu; push pause; pop.
    sys.initialize(bb, storage);
    sys.push_screen("main_menu", bb, storage);
    sys.push_screen("pause", bb, storage);
    sys.pop_screen(bb, storage);

    // Resulting stack is ["gameplay", "main_menu"] — pause popped.
    REQUIRE(ScreenStackSystem::get_stack(bb)
            == std::vector<std::string>{"gameplay", "main_menu"});

    // The set of active screen names equals the set of stacked names that have a
    // UIScreen entity (base "gameplay" excluded — it has no entity).
    REQUIRE(active_screen_names(storage) == stacked_names_with_entity(bb, storage));

    // Spell out the concrete expectation as well.
    REQUIRE(active_of(storage, main_menu) == true);
    REQUIRE(active_of(storage, pause) == false);
    REQUIRE(active_screen_names(storage) == std::set<std::string>{"main_menu"});
}

// ---------------------------------------------------------------------------
// Edge — empty-name push is a no-op and does not throw (R2.5)
// ---------------------------------------------------------------------------

TEST_CASE("ScreenStackSystem: push_screen with an empty name is a no-op and does not throw",
          "[Engine][screen_stack][unit]") {
    EntityManager entities;
    ComponentStorage storage;
    Blackboard bb;
    ScreenStackSystem sys;

    Entity main_menu = make_screen(entities, storage, "main_menu");

    sys.initialize(bb, storage);
    const auto stack_before = ScreenStackSystem::get_stack(bb);

    REQUIRE_NOTHROW(sys.push_screen("", bb, storage));

    REQUIRE(ScreenStackSystem::get_stack(bb) == stack_before);  // stack unchanged
    REQUIRE(ScreenStackSystem::depth(bb) == 1);
    REQUIRE(active_of(storage, main_menu) == false);            // flags unchanged
}

// ---------------------------------------------------------------------------
// Edge — pop at depth 1 leaves ["gameplay"] and does not throw (R3.5)
// ---------------------------------------------------------------------------

TEST_CASE("ScreenStackSystem: pop_screen at depth 1 leaves the base and does not throw",
          "[Engine][screen_stack][unit]") {
    EntityManager entities;
    ComponentStorage storage;
    Blackboard bb;
    ScreenStackSystem sys;

    Entity pause = make_screen(entities, storage, "pause");

    sys.initialize(bb, storage);
    REQUIRE(ScreenStackSystem::depth(bb) == 1);

    REQUIRE_NOTHROW(sys.pop_screen(bb, storage));

    REQUIRE(ScreenStackSystem::get_stack(bb)
            == std::vector<std::string>{"gameplay"});           // base retained
    REQUIRE(ScreenStackSystem::depth(bb) == 1);
    REQUIRE(active_of(storage, pause) == false);
}

// ---------------------------------------------------------------------------
// Edge — a stacked name with no matching UIScreen entity is kept, no throw (R4.4)
// ---------------------------------------------------------------------------

TEST_CASE("ScreenStackSystem: a stacked name with no UIScreen entity is kept and does not throw",
          "[Engine][screen_stack][unit]") {
    EntityManager entities;
    ComponentStorage storage;
    Blackboard bb;
    ScreenStackSystem sys;

    Entity main_menu = make_screen(entities, storage, "main_menu");

    sys.initialize(bb, storage);
    sys.push_screen("main_menu", bb, storage);

    // "ghost" has no UIScreen entity (like the base sentinel "gameplay").
    REQUIRE_NOTHROW(sys.push_screen("ghost", bb, storage));

    // The name stays in the stack and reconciliation simply matches no entity.
    REQUIRE(stack_contains(bb, "ghost"));
    REQUIRE(ScreenStackSystem::get_stack(bb).back() == "ghost");
    REQUIRE(ScreenStackSystem::depth(bb) == 3);  // gameplay, main_menu, ghost

    // Real screens remain consistent: main_menu is still in the stack -> active.
    REQUIRE(active_of(storage, main_menu) == true);
    REQUIRE(active_screen_names(storage) == stacked_names_with_entity(bb, storage));

    // Popping "ghost" cleanly returns to the main_menu modal.
    REQUIRE_NOTHROW(sys.pop_screen(bb, storage));
    REQUIRE_FALSE(stack_contains(bb, "ghost"));
    REQUIRE(active_of(storage, main_menu) == true);
}

// ---------------------------------------------------------------------------
// process_commands — CMD_PUSH applies the push and removes the command key
// ---------------------------------------------------------------------------

TEST_CASE("ScreenStackSystem: process_commands consumes CMD_PUSH and clears the key",
          "[Engine][screen_stack][unit]") {
    EntityManager entities;
    ComponentStorage storage;
    Blackboard bb;
    ScreenStackSystem sys;

    Entity pause = make_screen(entities, storage, "pause");

    sys.initialize(bb, storage);
    bb.set<std::string>(ScreenStackSystem::CMD_PUSH, "pause");

    sys.process_commands(bb, storage);

    REQUIRE(active_of(storage, pause) == true);                  // push applied
    REQUIRE(stack_contains(bb, "pause"));
    REQUIRE(bb.has(ScreenStackSystem::CMD_PUSH) == false);       // command key removed
}

// ---------------------------------------------------------------------------
// process_commands — CMD_POP pops the Top_Screen and removes the command key
// ---------------------------------------------------------------------------

TEST_CASE("ScreenStackSystem: process_commands consumes CMD_POP and clears the key",
          "[Engine][screen_stack][unit]") {
    EntityManager entities;
    ComponentStorage storage;
    Blackboard bb;
    ScreenStackSystem sys;

    Entity pause = make_screen(entities, storage, "pause");

    sys.initialize(bb, storage);
    sys.push_screen("pause", bb, storage);
    REQUIRE(ScreenStackSystem::depth(bb) == 2);

    bb.set<bool>(ScreenStackSystem::CMD_POP, true);
    sys.process_commands(bb, storage);

    REQUIRE(active_of(storage, pause) == false);                 // pop applied
    REQUIRE_FALSE(stack_contains(bb, "pause"));
    REQUIRE(ScreenStackSystem::depth(bb) == 1);
    REQUIRE(bb.has(ScreenStackSystem::CMD_POP) == false);        // command key removed
}

// ---------------------------------------------------------------------------
// process_commands — CMD_CLEAR_TO clears to base + main_menu and removes the key
// ---------------------------------------------------------------------------

TEST_CASE("ScreenStackSystem: process_commands consumes CMD_CLEAR_TO to main_menu and clears the key",
          "[Engine][screen_stack][unit]") {
    EntityManager entities;
    ComponentStorage storage;
    Blackboard bb;
    ScreenStackSystem sys;

    Entity main_menu = make_screen(entities, storage, "main_menu");
    Entity pause     = make_screen(entities, storage, "pause");

    // Start from a deeper stack with pause on top.
    sys.initialize(bb, storage);
    sys.push_screen("pause", bb, storage);
    REQUIRE(active_of(storage, pause) == true);

    bb.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO, "main_menu");
    sys.process_commands(bb, storage);

    REQUIRE(ScreenStackSystem::get_stack(bb)
            == std::vector<std::string>{"gameplay", "main_menu"});  // base + main_menu
    REQUIRE(active_of(storage, main_menu) == true);                 // main_menu active
    REQUIRE(active_of(storage, pause) == false);                    // pause inactive
    REQUIRE(bb.has(ScreenStackSystem::CMD_CLEAR_TO) == false);       // command key removed
}
