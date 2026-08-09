# Project Context

Context files live in `agentProjectDocs/`. Load them
conditionally — do not read all of them for every task.

## Always Read (every session)

1. `agentProjectDocs/progress-tracker.md` — current
   phase, in-progress work, next steps
2. `agentProjectDocs/architecture.md` — **Invariants**
   section at minimum

## Read Before Specific Work

| Before...                          | Read                                    |
| ---------------------------------- | --------------------------------------- |
| Product or scope decisions         | `agentProjectDocs/project-overview.md`  |
| Any UI work                        | `agentProjectDocs/ui-context.md`        |
| Writing or modifying code          | `agentProjectDocs/code-standards.md`    |
| Planning or starting a feature     | `agentProjectDocs/ai-workflow-rules.md` |
| Revisiting a settled design choice | `agentProjectDocs/decisions.md`         |
| Implementing a planned feature     | Its spec in `agentProjectDocs/specs/`   |

## Commands

- Dev server: `[e.g. npm run dev]`
- Build: `[e.g. npm run build]`
- All tests: `[e.g. npm test]`
- Single test: `[e.g. npm test -- path/to/test]`
- Lint: `[e.g. npm run lint]`
- Typecheck: `[e.g. npx tsc --noEmit]`

## Keeping Context in Sync

- Update `agentProjectDocs/progress-tracker.md` after
  each meaningful implementation change.
- Record significant technical decisions (and why) in
  `agentProjectDocs/decisions.md` — append-only.
- If implementation changes the architecture, scope, or
  standards documented in the context files, update the
  relevant file before continuing.
- Before implementing a non-trivial feature, write a
  spec in `agentProjectDocs/specs/` using
  `feature-template.md`.
