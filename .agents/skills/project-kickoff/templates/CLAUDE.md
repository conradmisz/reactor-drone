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

## Context Backup

<!-- Filled from the kickoff interview. If auto-backup was
     declined, replace this whole section with the single
     line: "Backups are manual — push when asked." -->

Auto-backup: **[on/off]**. Remote `[url]`, branch `[main]`.

- Push only from a **verified checkpoint** — a state the
  *user* has confirmed works. A green test run is evidence,
  not confirmation; nobody but the user can supply it.
- Cadence: after every **[3]** verified checkpoints, or
  whenever asked. Never mid-feature.
- Every push carries `CLAUDE.md` and `agentProjectDocs/`
  alongside the code, so a fresh clone's context always
  describes the commit it ships with.
- Nothing verified since the last push? Skip it and say so.
  Do not push to "keep the remote current".

## Keeping Context in Sync

### Bugs

- **Before investigating any bug or unexplained behavior, read
  `agentProjectDocs/bugs/` first.** A report may already exist,
  and its Ruled Out section will stop you re-running a test
  that was already done.
- Bug investigations live in `bugs/`, **not** in
  `progress-tracker.md`. That file is for phase and next-steps
  state only; letting one long investigation live there is how
  it grows past its size budget.
- New bug: copy `bugs/bug-template.md` to `bugs/NNN-slug.md`,
  next free number. Status lives in the frontmatter — there is
  no index file and no open/closed folders to keep in sync. To
  list every bug and its state:

      rg -N '^(id|title|status|severity):' agentProjectDocs/bugs/[0-9]*.md

- **Log every test into Ruled Out as you run it, including
  negative and inconclusive results.** Each entry states what
  was tested, what was observed, and what it eliminates. A test
  whose result was never written down will be run again.
- **Never close a bug on a passing build alone.** Fill in
  Resolution with the root cause and how it was verified, then
  set `resolved:`.

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
