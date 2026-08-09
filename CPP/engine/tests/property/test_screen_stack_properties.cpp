/**
 * Property-based tests for the Option-040 Phase 4 screen stack & input routing
 * (o-040-04-screen-stack).
 *
 * Implements the four correctness properties from the design "Correctness
 * Properties" section, one property-based TEST_CASE each:
 *   Property 1: Stack and active-flag set agreement under any push/pop sequence
 *   Property 2: Modal-capture flag equals (depth > 1) after UISystem runs, and
 *               is idempotent
 *   Property 3: Clear-to-main_menu from any reachable stack yields the main-menu
 *               modal (["gameplay", "main_menu"])
 *   Property 4: Command consumption is apply-once and idempotent
 *
 * The screen stack is a pure, deterministic, SDL-free state machine over plain
 * data (std::vector<std::string> + UIScreen.active flags + Blackboard command
 * keys). Every generated case constructs a fresh
 * EntityManager/ComponentStorage/Blackboard (and a LuaManager for Property 2),
 * with no window.
 *
 * Feature: o-040-04-screen-stack
 * Testing Framework: Catch2 v3
 * All property tests bounded per the workspace property-test-bounds policy.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>

#include "engine/ecs/systems/screen_stack_system.hpp"
#include "engine/ecs/systems/ui_system.hpp"
#include "engine/ecs/components.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "engine/ecs/blackboard.hpp"
#include "engine/lua_manager.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

// Configurable test iteration counts (MANDATORY — workspace property-test-bounds policy)
constexpr int NUM_OUTER_TESTS = 10;  // Number of different outer subjects (sequences/states)
constexpr int NUM_INNER_TESTS = 5;   // Number of value variations per subject

// ---------------------------------------------------------------------------
// Helpers shared by the screen-stack properties.
// ---------------------------------------------------------------------------
namespace {

// Create an entity and attach a UIScreen with the given name (initial active
// value is unimportant — the system reconciles every flag).
Entity make_screen(EntityManager& entities, ComponentStorage& storage,
                   const std::string& name, bool active = false) {
    Entity e = entities.create_entity();
    storage.add_component(e, UIScreen{name, active});
    return e;
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
// stack names minus any sentinel like the base "gameplay", which has no entity).
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

// Snapshot every UIScreen entity's active flag for an idempotence comparison.
std::map<Entity, bool> active_flag_snapshot(const ComponentStorage& storage) {
    std::map<Entity, bool> snap;
    for (Entity e : storage.entities_with_component<UIScreen>()) {
        auto opt = storage.get_component<UIScreen>(e);
        if (opt.has_value()) {
            snap[e] = opt->get().active;
        }
    }
    return snap;
}

// Read back the (required) active flag for a single UIScreen entity.
bool active_of(const ComponentStorage& storage, Entity e) {
    auto opt = storage.get_component<UIScreen>(e);
    REQUIRE(opt.has_value());
    return opt->get().active;
}

} // namespace

// ---------------------------------------------------------------------------
// Feature: o-040-04-screen-stack, Property 1: Stack and active-flag set
// agreement under any push/pop sequence.
//
// For any initial set of UIScreen entities (names from a small pool that
// includes duplicates) and any generated finite sequence of push_screen and
// pop_screen operations applied starting from initialize(), after EACH operation
// the set of screen names whose UIScreen.active == true equals the set of stack
// names that correspond to an existing UIScreen entity (the base "gameplay",
// which has no entity, is excluded); the base entry is always present at the
// front of the stack; and the stack depth is always at least one.
//
// **Validates: Requirements 1.1, 1.2, 2.1, 2.2, 2.3, 3.1, 3.2, 3.3, 3.5, 4.1, 4.2, 4.3, 4.4**
// ---------------------------------------------------------------------------
TEST_CASE("Property 1: stack and active-flag set agree under any push/pop sequence",
          "[Engine][screen_stack][property]") {

    SECTION("after every push/pop, active set == stacked names with an entity; base at front; depth >= 1") {
        // Two generated chunks of ints, concatenated into one op sequence. Each
        // int encodes one operation (push a pooled name or pop). 10 x 5 = 50 cases.
        auto outer = GENERATE(take(NUM_OUTER_TESTS, chunk(8, random(0, 1000))));
        auto inner = GENERATE(take(NUM_INNER_TESTS, chunk(4, random(0, 1000))));

        EntityManager entities;
        ComponentStorage storage;
        Blackboard bb;
        ScreenStackSystem sys;

        // Small name pool with a duplicate ("main_menu" appears twice -> two
        // distinct UIScreen entities share that name). Each pool entry gets an
        // entity, so every pushed name corresponds to at least one entity.
        const std::vector<std::string> pool =
            {"main_menu", "pause", "settings", "main_menu"};
        for (const std::string& name : pool) {
            make_screen(entities, storage, name);
        }

        sys.initialize(bb, storage);

        // Invariant must already hold right after initialize().
        REQUIRE(ScreenStackSystem::depth(bb) >= 1);
        REQUIRE(ScreenStackSystem::get_stack(bb).front()
                == std::string(ScreenStackSystem::BASE_SCREEN));
        REQUIRE(active_screen_names(storage) == stacked_names_with_entity(bb, storage));

        // Build the op sequence from the generated ints.
        std::vector<int> ops;
        ops.insert(ops.end(), outer.begin(), outer.end());
        ops.insert(ops.end(), inner.begin(), inner.end());

        for (int v : ops) {
            // Bias toward push (2/3 of values) so the stack actually grows.
            if (v % 3 == 0) {
                sys.pop_screen(bb, storage);
            } else {
                const std::string& name = pool[(v / 3) % pool.size()];
                sys.push_screen(name, bb, storage);
            }

            // Invariant after EACH operation:
            // (1) depth is always at least one (base never removed).
            REQUIRE(ScreenStackSystem::depth(bb) >= 1);
            // (2) the base entry is always present at the front.
            REQUIRE(ScreenStackSystem::get_stack(bb).front()
                    == std::string(ScreenStackSystem::BASE_SCREEN));
            // (3) active-name set == stacked names that have a UIScreen entity.
            REQUIRE(active_screen_names(storage) == stacked_names_with_entity(bb, storage));
        }
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-04-screen-stack, Property 2: Modal-capture flag equals
// (depth > 1) after UISystem runs, and is idempotent.
//
// For any screen_stack state (generated by pushing 0..k screens onto the base),
// after UISystem::update runs ui_captured_input is true if and only if the stack
// depth is greater than one, and running UISystem::update again on the same
// unchanged stack leaves ui_captured_input unchanged.
//
// **Validates: Requirements 1.3, 2.4, 3.4, 6.1, 6.2, 6.3, 12.4**
// ---------------------------------------------------------------------------
TEST_CASE("Property 2: modal-capture flag equals (depth > 1) after UISystem runs, idempotently",
          "[Engine][screen_stack][property]") {

    SECTION("ui_captured_input == (depth > 1) after update; a second update is unchanged") {
        // outer: number of screens to push (0..5). inner: which pool names.
        auto push_count = GENERATE(take(NUM_OUTER_TESTS, random(0, 5)));
        auto picks      = GENERATE(take(NUM_INNER_TESTS, chunk(5, random(0, 2))));

        const std::vector<std::string> pool = {"main_menu", "pause", "settings"};

        LuaManager lua_manager;
        EntityManager entities;
        ComponentStorage storage;
        Blackboard bb;
        ScreenStackSystem sys;

        // One UIScreen entity per pool name.
        for (const std::string& name : pool) {
            make_screen(entities, storage, name);
        }

        sys.initialize(bb, storage);
        for (int i = 0; i < push_count; ++i) {
            sys.push_screen(pool[picks[i] % pool.size()], bb, storage);
        }

        const std::size_t depth = ScreenStackSystem::depth(bb);
        const bool expected_modal = depth > 1;

        const int window_height = 600;
        UISystem ui(lua_manager, window_height);

        // Offscreen pointer + button edges down so UISystem performs no widget
        // interaction (there are no widgets anyway) — it only publishes the flag.
        bb.set<int>("window_height", window_height);
        bb.set<float>("mouse.screen_x", -100000.0f);
        bb.set<float>("mouse.screen_y", -100000.0f);
        bb.set<bool>("mouse.down", false);
        bb.set<bool>("mouse.up", false);

        ui.update(storage, entities, bb);
        REQUIRE(bb.get_or<bool>(ScreenStackSystem::CAPTURE_KEY, false) == expected_modal);

        // Idempotent: a second update on the same unchanged stack is the same.
        ui.update(storage, entities, bb);
        REQUIRE(bb.get_or<bool>(ScreenStackSystem::CAPTURE_KEY, false) == expected_modal);
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-04-screen-stack, Property 3: Clear-to-main_menu from any
// reachable state yields the main-menu modal.
//
// For any reachable screen_stack state (any sequence of pushes of
// main_menu/pause/other names onto the base), processing a clear-to-main_menu
// command via process_commands results in main_menu's UIScreen.active == true,
// pause's UIScreen.active == false, the base entry retained at the front, and a
// stack whose names are exactly ["gameplay", "main_menu"].
//
// **Validates: Requirements 11.6, 12.3**
// ---------------------------------------------------------------------------
TEST_CASE("Property 3: clear-to-main_menu from any reachable stack yields the main-menu modal",
          "[Engine][screen_stack][property]") {

    SECTION("process CMD_CLEAR_TO=main_menu -> stack == [gameplay, main_menu]; main_menu active, pause inactive") {
        auto outer = GENERATE(take(NUM_OUTER_TESTS, chunk(6, random(0, 1000))));
        auto inner = GENERATE(take(NUM_INNER_TESTS, chunk(3, random(0, 1000))));

        EntityManager entities;
        ComponentStorage storage;
        Blackboard bb;
        ScreenStackSystem sys;

        Entity main_menu = make_screen(entities, storage, "main_menu");
        Entity pause     = make_screen(entities, storage, "pause");

        // Names pushed to reach an arbitrary state. "other" has no UIScreen
        // entity (an allowed unmatched stacked name, R4.4).
        const std::vector<std::string> pool = {"main_menu", "pause", "other"};

        sys.initialize(bb, storage);

        std::vector<int> pushes;
        pushes.insert(pushes.end(), outer.begin(), outer.end());
        pushes.insert(pushes.end(), inner.begin(), inner.end());
        for (int v : pushes) {
            sys.push_screen(pool[v % pool.size()], bb, storage);
        }

        // Process a clear-to-main_menu command.
        bb.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO, "main_menu");
        sys.process_commands(bb, storage);

        REQUIRE(ScreenStackSystem::get_stack(bb)
                == std::vector<std::string>{"gameplay", "main_menu"});
        REQUIRE(ScreenStackSystem::get_stack(bb).front()
                == std::string(ScreenStackSystem::BASE_SCREEN));
        REQUIRE(active_of(storage, main_menu) == true);
        REQUIRE(active_of(storage, pause) == false);
        REQUIRE(bb.has(ScreenStackSystem::CMD_CLEAR_TO) == false);  // command consumed
    }
}

// ---------------------------------------------------------------------------
// Feature: o-040-04-screen-stack, Property 4: Command consumption is apply-once
// and idempotent.
//
// For any combination of command keys present on the Blackboard (ui.cmd.push,
// ui.cmd.pop, ui.cmd.clear_to — each independently generated present/absent), a
// single process_commands call applies each present command once and removes ALL
// command keys (bb.has is false for each afterward); and an immediately
// following process_commands call with no command pending leaves both the
// screen_stack and every UIScreen.active flag unchanged.
//
// **Validates: Requirements 5.1, 5.2, 11.4, 11.5**
// ---------------------------------------------------------------------------
TEST_CASE("Property 4: command consumption is apply-once and idempotent",
          "[Engine][screen_stack][property]") {

    SECTION("one process_commands applies + removes every present command; a second call is a no-op") {
        // outer: which of the three command keys are present (3 booleans).
        // inner: initial-stack setup variation (so pop has something to remove).
        auto flags = GENERATE(take(NUM_OUTER_TESTS, chunk(3, random(0, 1))));
        auto setup = GENERATE(take(NUM_INNER_TESTS, chunk(3, random(0, 1000))));

        EntityManager entities;
        ComponentStorage storage;
        Blackboard bb;
        ScreenStackSystem sys;

        make_screen(entities, storage, "main_menu");
        make_screen(entities, storage, "pause");

        const std::vector<std::string> pool = {"main_menu", "pause"};

        sys.initialize(bb, storage);
        // Seed an arbitrary starting stack so a pop command has an effect.
        for (int v : setup) {
            sys.push_screen(pool[v % pool.size()], bb, storage);
        }

        const bool has_push  = (flags[0] != 0);
        const bool has_pop   = (flags[1] != 0);
        const bool has_clear = (flags[2] != 0);

        if (has_push)  bb.set<std::string>(ScreenStackSystem::CMD_PUSH, "pause");
        if (has_pop)   bb.set<bool>(ScreenStackSystem::CMD_POP, true);
        if (has_clear) bb.set<std::string>(ScreenStackSystem::CMD_CLEAR_TO, "main_menu");

        // One process_commands call applies each present command.
        sys.process_commands(bb, storage);

        // ALL command keys are removed afterward.
        REQUIRE(bb.has(ScreenStackSystem::CMD_PUSH) == false);
        REQUIRE(bb.has(ScreenStackSystem::CMD_POP) == false);
        REQUIRE(bb.has(ScreenStackSystem::CMD_CLEAR_TO) == false);

        // Capture the resulting state.
        const std::vector<std::string> stack_after = ScreenStackSystem::get_stack(bb);
        const std::map<Entity, bool> flags_after = active_flag_snapshot(storage);

        // An immediately following call with no command pending changes nothing.
        sys.process_commands(bb, storage);

        REQUIRE(ScreenStackSystem::get_stack(bb) == stack_after);
        REQUIRE(active_flag_snapshot(storage) == flags_after);
    }
}
