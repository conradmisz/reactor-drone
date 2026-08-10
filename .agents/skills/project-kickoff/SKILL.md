---
name: project-kickoff
description: Use when the user is starting a new project from an idea and its context documentation does not exist yet — e.g. invoked from a fresh/empty project directory with a project idea, brief, or "set up this project". Generates agentProjectDocs/ and a root CLAUDE.md by interviewing the user.
---

# Project Kickoff Interview

## Overview

Turn a project idea into fully filled-out context files by
interviewing the user, then writing the results into the
current project directory. The output is `agentProjectDocs/`
(the filled context files) plus `CLAUDE.md` at the project
root.

**Canonical templates:** `templates/` in this skill's
own directory. Read every file there first (including
`templates/agentProjectDocs/specs/feature-template.md`
and `templates/CLAUDE.md`). The templates are the source
of truth for structure — never reconstruct them from
memory.

## When NOT to Use

- `agentProjectDocs/` already exists in the current
  directory → offer to review/update instead of overwrite
- The user wants a one-off script, not a project

## Process

1. **Read all template files** from the canonical path.
2. **Read the user's idea and pre-fill aggressively.**
   Draft answers for every section you can reasonably
   infer (goals, core flow, feature categories, likely
   stack, entities). The interview confirms and corrects
   drafts — it does not start from blank.
3. **Interview in phases** (below). Use AskUserQuestion
   for enumerable choices (max 4 questions per call, with
   your recommendation first, labeled "(Recommended)").
   Use plain conversation for open-ended answers. Never
   ask something already answerable from the idea — state
   the inference and let them correct it.
4. **Write the files** into the current working directory:
   - `agentProjectDocs/` — all context files filled out,
     plus `specs/feature-template.md` copied as-is
   - `CLAUDE.md` at the project root — Commands section
     filled with the *real* commands for the chosen stack,
     not placeholders
5. **Seed the living files:**
   - `decisions.md` — one entry per stack/architecture
     choice made during the interview, with the why
   - `progress-tracker.md` — phase "Not started", Next Up
     = first feature unit, every unresolved interview
     item listed under Open Questions
6. **Report:** list files written, open questions, and
   the suggested first feature to spec out.

## Interview Phases

| Phase | Covers | Fills |
|-------|--------|-------|
| 1. Product | What/who/why, core user flow, in/out of scope, success criteria | project-overview.md |
| 2. Architecture | Stack, storage, auth model, core entities, invariants, env/setup | architecture.md |
| 3. UI | Theme direction, palette, fonts, component library, layout patterns | ui-context.md |
| 4. Standards & workflow | Deviations from defaults only | code-standards.md, ai-workflow-rules.md |
| 5. Commands & backup | Confirm dev/build/test/lint commands for chosen stack; offer GitHub auto-backup (below) | CLAUDE.md, ai-workflow-rules.md |

Phase order matters: product answers constrain
architecture; architecture constrains UI and commands.

Phases 4–5 should usually be a single confirmation, not
an interrogation — propose sensible defaults derived from
the chosen stack and ask only "any objections?" The one
exception is the backup question below: it is a real
choice, so ask it explicitly.

## GitHub Auto-Backup (Phase 5)

Ask every project, once, with AskUserQuestion:

> Enable automatic GitHub backups of this project's code
> and context files?

| Option | Means |
|--------|-------|
| Yes — verified checkpoints only *(Recommended)* | Push after every ~3 states **the user** has confirmed working |
| Yes — confirm each push | Same gate, plus an explicit prompt before every push |
| No | Manual pushes only |

If enabled, also capture: the remote URL (or "not created
yet"), the branch, and the checkpoint count if they want
something other than 3. Then fill the **Context Backup**
section of `CLAUDE.md` and keep the **Backing Up to
GitHub** section of `ai-workflow-rules.md`. If declined,
collapse the `CLAUDE.md` section to one line and delete
the `ai-workflow-rules.md` section outright — a disabled
feature must not leave instructions lying around.

**The gate is the whole point of the feature.** An
automatic push must never carry code the *user* has not
confirmed works; the agent's own confidence and a green
test suite are both insufficient on their own. Write the
rule into *both* files: `CLAUDE.md` is loaded every
session, `ai-workflow-rules.md` only before feature work,
and the rule has to hold in either entry path.

Do **not** create the remote during kickoff. Record the
intent; the first verified checkpoint creates the repo,
asking public-or-private at that moment. Kickoff writes
documentation, not side effects.

## Rules

- Every `[placeholder]` in the templates must be resolved
  in the output — filled, or moved to Open Questions.
  Grep the written files for `[` to verify none remain
  (except legitimate markdown links).
- "Out of Scope", "Invariants", "Never Do", and per-file
  size-budget comments are the highest-value content —
  push the user for real answers there; don't accept
  empty sections.
- If the user says "you decide", decide, and record it in
  decisions.md as your recommendation with reasoning.
- Do not scaffold application code. This skill produces
  documentation only — including no `git init`, no remote
  creation, and no first push.
- The backup answer is recorded in `decisions.md` like any
  other choice, with its cadence and gate.

## Common Mistakes

| Mistake | Fix |
|---------|-----|
| Asking 20+ questions up front | Pre-fill from the idea; interview only gaps and confirmations |
| Leaving `[e.g. ...]` examples in output files | Replace or delete every bracketed placeholder |
| Copying placeholder commands into CLAUDE.md | Write actual commands for the chosen stack |
| Writing files into the template directory | Output goes to the *current working directory* |
| Skipping decisions.md seeding | Every interview choice with a "why" belongs there |
| Leaving backup instructions in place after the user declined | Collapse the `CLAUDE.md` section, delete the `ai-workflow-rules.md` one |
| Treating a green test run as the user's verification | Only the user's own confirmation opens the gate |
