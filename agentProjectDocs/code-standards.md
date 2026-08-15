# Code Standards

## General

- Read `ENGINE.md` before touching `CPP/engine/` or the `main.cpp` frame order,
  and update it in the same commit as the change.
- Fix the root cause. The particle leak was fixed by an `emit` flag on
  `ParticleSystem::update`, not by running `lifetime.update` in the shop branch
  — the smaller diff there was the wrong fix (it would have expired the
  player's uncollected loot).
- No speculative generality: no system, component or config knob without a
  caller in this game today.
- Prefer data over code. If a playtest would want to change a number, it belongs
  in `GameData.json`.

## C++

- C++17. No compiler extensions; the `-Wpedantic` gate is enforced.
- Pure logic goes in a **free function in a header** (`feedback.hpp`,
  `obstacles.hpp`, `parallax.hpp`, `enemy_path.hpp`, `item_system.hpp`,
  `shield_system.hpp`, `aim_math.hpp`) so it is unit- and property-testable with
  no SDL, no renderer and no entity manager. Promote a header to a real system
  only when it needs to own state.
- Getters do not mutate. `wave_just_cleared()` is a plain getter cleared at the
  top of the next `update()`, not a consume-on-read (D15).
- Dispatch behaviour on a JSON `effect` **string**, never on a row index — a
  re-ordered catalogue must never silently apply the wrong effect (D26/D36).
- Parse JSON with `json.value(key, default)` so an older data file still loads.

## ECS

- Adding a component type is the expensive move. Try, in order: a field on an
  existing struct (`ShipState` was declared fat on purpose, D17), a Blackboard
  key (`ship.extra_shots`, `ship.item_amount`, D28/D41), then a new type.
- A new type requires all of: storage member + two `get_storage<>`
  specialisations + the explicit instantiations in `component_storage.cpp`, a
  sweep in `destruction.cpp`, and a `debug_adapters` registration.
- A new engine `.cpp` must be added to three explicit CMake source lists.
- Anything that must survive a restart is skipped in `spawn_world()`.

## Determinism

- Every RNG draw happens on every path, in a fixed order. Pre-roll the maximum
  and let a conditional choose which draws are *used*. The loop looks wasteful;
  that is the point.
- No draw sits behind a conditional that is not part of the game state (loot is
  rolled before the explosion sprite loads, so a missing sidecar cannot shift
  the stream).
- Presentation clocks stay out of the Blackboard (`UIRenderSystem::elapsed_`),
  so a replay cannot diverge on render state.

## Rendering & particles

- World coordinates flip in exactly one place; UI coordinates in exactly one
  other. Never add a third.
- `DEFAULT_MAX_PARTICLES` is 2000 and truncates **silently**. Measure the live
  count at the moment your emitter actually fires, not on average.
- Colour comes from a palette or a `ui_styles` entry, never a literal.

## Testing and Verification

- Catch2. Unit tests for new pure logic; a property test as well where the
  logic round-trips or has a monotone/range invariant
  (`NUM_OUTER_TESTS=10`, `NUM_INNER_TESTS=5`).
- TDD for new pure functions: the test first.
- **Verified means**: `python runTestsAll.py` green (8/8), the build log has no
  warning from our code, the seeded replay canary prints identically twice, and
  the mechanic was exercised in a real `python run.py` window. A passing build
  alone is not verification, and headless verification is not a playtest —
  say which one you did.
- Headless driving: `SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed S
  --keys 5:SPACE --clicks F:x,y --stopframe N`. `--fps` does **not** speed a run
  up; to reach a late wave, temporarily change the trigger and revert it.

## File Organization

- `CPP/engine/` — game-agnostic. If it names Reactor Drone, it is in the wrong
  folder.
- `CPP/game/` — gameplay. `main.cpp` owns the phase machine and frame order;
  systems own behaviour, not sequencing (`main.cpp` gets four shop calls and no
  shop logic).
- `CPP/*/tests/unit`, `CPP/*/tests/property` — mirror the source name.
- `assets/generator/v2/` — offline Python. `assets/images/v2/`, `assets/Audio/`
  — its committed output.
- `backend/src/` — the Cloudflare Worker. `worker.js` is routes only;
  `dashboard.js` is the ops page as **one template literal**, which has a trap:
  a backslash escape inside it is eaten by the template literal before the
  browser ever sees it, so a regex like `/[\s,]+/` silently becomes `/[s,]+/`.
  Write `\\s`, or avoid the regex (the tag splitter uses `split(',')` for
  exactly this reason). Same rule for `${` — the file's own header comment
  says the page is written without it so the literal stays unbroken.

## Never Do

- Never add a warning. Never silence one with a cast; fix the code.
- Never hardcode a tunable in C++ that a playtest would want to change.
- Never bake an arena colour into enemy art — tint at spawn.
- Never make an RNG draw conditional on outcome-dependent state.
- Never modify inherited engine tests to make a change pass.
- Never reorder the frame without reading `ENGINE.md` §3 and updating it.
- Never leave a temporary debug edit (a `% 1` shop trigger, a throwaway
  `GameData.json`) in the tree — the verification section must say it was
  reverted.
- Never claim a feature is verified when only tests ran.
