# AI Workflow Rules

## Approach

Work in **phases**: one independently shippable slice at a time, each leaving
every gate green (build warning-free, 8/8 ctest, replay canary identical)
before the next starts. This is how all 11 commits so far were built and it is
what makes the history readable.

Every phase ends with a written handoff covering: what shipped, what did *not*
ship and why, files touched, new surface area, tuning values chosen, known
rough edges, and the verification actually run. The historical handoffs are
`handoff-phase-1..4.md` and `HANDOFF.md`; new ones append to the living files
here instead (see "Keeping Docs in Sync").

## Feature Specs

- Non-trivial feature → copy `specs/feature-template.md` to
  `specs/[feature-name].md`, fill it out, resolve its open questions, *then*
  write code. Acceptance criteria first.
- Trivial changes (a tuning number, a copy tweak, a one-line fix) need no spec.
- A balance change is a `GameData.json` edit, not a feature.

## Scoping Rules

- One phase at a time. Do not combine an engine change with a gameplay change
  with a data change unless the phase is explicitly about all three.
- If a change cannot be verified end to end quickly, split it.
- Prefer the cheapest structural option: a field on an existing struct, then a
  Blackboard key, then a new component type.

## When to Split Work

Split if a step combines:

- An engine (`CPP/engine/`) change and a gameplay (`CPP/game/`) change that do
  not strictly depend on each other.
- A new mechanic and its balance numbers — ship the mechanic with deliberately
  provisional values and label them as provisional.
- Behaviour and its visuals. (Gameplay Phase 4 shipped four items with no visual
  tell at all, deliberately, and said so.)

## Handling Missing Requirements

- Do not invent game behaviour. If a design call is unmade — e.g. how the four
  arena themes should spread across 20 waves — flag it, do not decide it.
- Balance numbers with no playtest behind them are labelled **provisional** in
  the data file and in the handoff.
- Unresolved items go to `progress-tracker.md` → Open Questions.

## Verification Discipline

- State which verification actually happened. "Headless" and "played it" are
  different claims; four phases in a row were shipped unplayed and the docs say
  so each time.
- Temporary edits made to reach a late game state (a `% 1` shop trigger, a
  throwaway `GameData.json`) must be reverted, and the revert stated.
- Two runs of the same `--seed` must print an identical summary line.

## Protected Files

Do not modify unless explicitly instructed:

- Inherited engine tests under `CPP/engine/tests/` that came from the class
  baseline — they are the proof the engine still works.
- Committed generated assets under `assets/images/v2/` and `assets/Audio/` —
  regenerate via `assets/generator/v2/` instead (and note that regenerating the
  backdrops will change the pre-`crc32` PNGs once).
- Vendored/fetched dependencies.

## Keeping Docs in Sync

| Changed | Update |
| --- | --- |
| Anything under `CPP/engine/`, a new game system, or the frame order | `ENGINE.md` — **same commit**, provenance re-measured not remembered |
| A design call with a reason worth keeping | `decisions.md` — append a new entry, never rewrite one |
| A shipped feature | `project-overview.md` → Features, one line |
| Current state / next step / a new unknown | `progress-tracker.md` |
| A convention or an anti-pattern learned the hard way | `code-standards.md` |
| A palette, style id, screen, or layout rule | `ui-context.md` |

## Before Moving to the Next Phase

1. The phase works end to end within its own scope.
2. Its spec's acceptance criteria pass, if it has one.
3. No invariant in `architecture.md` was violated.
4. `python runTestsAll.py` → 8/8, zero warnings from our code.
5. The seeded replay canary is identical across two runs.
6. `progress-tracker.md` reflects reality; every design call is in
   `decisions.md`; `ENGINE.md` is current.
7. Say plainly whether anyone has actually played it.
