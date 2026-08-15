---
name: playtest
description: Use when the user pastes playtest feedback, a gameplay-test list, or says "feedback from the most recent run" / "just did a gameplay test" in a reactor-drone repo.
---

# Playtest Batch Triage

Turn a pasted playtest feedback list into an implemented, correctly-logged batch
following this repo's CLAUDE.md conventions.

## Before touching anything

1. Read `agentProjectDocs/progress-tracker.md` and skim `agentProjectDocs/decisions.md`.
2. Read the **next free D-id** from CLAUDE.md (Keeping Context in Sync section).

## Triage

Number the pasted items, then classify each one:

- **Bug** (unexpected/broken behavior): check `agentProjectDocs/bugs/` for an
  existing report first; otherwise it gets a new `bugs/NNN-slug.md` from the
  template. Bugs are investigated via the bugs workflow, not lumped into the batch.
- **Conflicts with a settled decision**: cite the D-id and ask the user before
  implementing. Do not silently override a settled call.
- **Tuning / change / feature**: batch item. Non-trivial features get a spec
  from `specs/feature-template.md` first.

Present the triage as a short table (item → class → planned action) and get the
user's confirmation before implementing.

## After implementing

- Log the whole batch as **one** `decisions.md` entry at the next free D-id
  (with the why and what was rejected), then bump the next-free id in CLAUDE.md.
- Update `progress-tracker.md`.
- One line per shipped feature in `project-overview.md` → Features.

## Verify

Build, run the affected tests, and run the replay canary if determinism could be
affected. State exactly which verification ran — tests passing is not a
playtest; ask the user to re-playtest.
