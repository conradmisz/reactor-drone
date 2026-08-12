# Reactor Drone v2 — Project Context

C++17 / SDL3 top-down arena survival shooter on the CS-5850 ECS engine.
Context files live in `agentProjectDocs/`. Load them conditionally — do not
read all of them for every task.

## Always Read (every session)

1. `agentProjectDocs/progress-tracker.md` — current state, next step, open
   questions
2. `agentProjectDocs/architecture.md` — the **Invariants** section at minimum
3. `ENGINE.md` — before touching `CPP/engine/`, adding a system, or changing the
   `main.cpp` frame order. It is the architecture doc and is updated in the same
   commit as the change.

## Read Before Specific Work

| Before...                             | Read                                    |
| ------------------------------------- | --------------------------------------- |
| Product or scope decisions            | `agentProjectDocs/project-overview.md`  |
| Any UI, menu, palette or HUD work     | `agentProjectDocs/ui-context.md`        |
| Writing or modifying code             | `agentProjectDocs/code-standards.md`    |
| Planning or starting a phase          | `agentProjectDocs/ai-workflow-rules.md` |
| Revisiting a settled design choice    | `agentProjectDocs/decisions.md`         |
| Implementing a planned feature        | Its spec in `agentProjectDocs/specs/`   |
| Frame order, provenance, known traps  | `ENGINE.md`                             |

## Commands

- Interactive menu (build / test / run): `python run.py`
- Configure + build: `cmake -B CPP/build -S CPP && cmake --build CPP/build -j$(nproc)`
- All tests: `python runTestsAll.py`
- Engine tests only: `python runEngineTests.py` (`ctest -R "^(Engine|ResourceManager)"`)
- Game tests only: `python runGameTests.py` (`ctest -R "^Game"`)
- Single test case: `./CPP/build/game/tests/game_unit_tests "[items],[consumables]"`
- Run the game: `python run.py -- --seed 42`
- Headless run: `SDL_VIDEODRIVER=dummy ./CPP/build/game/game --seed 42 --keys 5:SPACE --stopframe 3000`
- Replay canary: run the line above twice — the summary must be identical.
- Warning check: build and grep the log for `warning:` — only Lua's vendored
  `tmpnam` is allowed.
- Regenerate assets (offline, needs Pillow): `python assets/generator/v2/make_sprites.py`

## Bugs

- **Before investigating any bug or unexplained behavior, read
  `agentProjectDocs/bugs/` first.** A report may already exist, and its Ruled Out
  section will stop you re-running a test that was already done.
- Bug investigations live in `bugs/`, **not** in `progress-tracker.md`.
- New bug: copy `bugs/bug-template.md` to `bugs/NNN-slug.md`, next free number.
  Status lives in the frontmatter — there is no index file. To list every bug:

      rg -N '^(id|title|status|severity):' agentProjectDocs/bugs/[0-9]*.md

- **Log every test into Ruled Out as you run it, including negative and
  inconclusive results.** A test whose result was never written down will be run
  again.
- **Never close a bug on a passing build alone.** Fill in Resolution with the
  root cause and how it was verified, then set `resolved:`.

## Keeping Context in Sync

- Update `progress-tracker.md` after every meaningful change.
- Append design calls (with the *why* and what was rejected) to `decisions.md`.
  Ids are stable and cited from code — next free id is **D199**.
  (`feature/engine-suite` reserves D138-D180; the gameplay-polish batch is
  D181-D191, logged as one entry; the second playtest batch is D192 and the
  third is D193, each also one entry.)
  (D50-D104 are allocated: iteration 3 lanes A-G = D52-D83, integration = D84,
  iteration 4 lanes H/I/J/K = D85-D104. D91-92, D104 unused but burned.
  Iteration 5 lanes L/M/N/O = D105-D131, integration = D132.
  Iteration 6: modular chassis + upgrade kit = D133-D134.
  Field-manual redesign = D135, arena prop art = D136, main-menu suite = D137.
  D110-D112, D118-D119, D124 unused but burned.)
- One line per shipped feature in `project-overview.md` → Features.
- Any engine change updates `ENGINE.md` **in the same commit**.
- Before a non-trivial feature, write a spec from `specs/feature-template.md`.
- State which verification actually ran. Tests passing is not a playtest.
