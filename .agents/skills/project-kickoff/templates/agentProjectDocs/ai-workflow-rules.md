# AI Workflow Rules

<!-- Keep under ~100 lines when filled. -->

## Approach

[Describe the overall development approach — e.g. Build
this project incrementally using a spec-driven workflow.
Context files define what to build, how to build it, and
the current state of progress. Always implement against
these specs — do not infer or invent behavior from scratch.]

## Feature Specs

- Before implementing a non-trivial feature, copy
  `specs/feature-template.md` to `specs/[feature-name].md`
  and fill it out
- Acceptance criteria must be written and open questions
  resolved before code is written
- Trivial changes (copy tweaks, small fixes) do not need
  a spec

## Scoping Rules

- Work on one feature unit at a time
- Prefer small, verifiable increments over large
  speculative changes
- Do not combine unrelated system boundaries in a
  single implementation step

## When to Split Work

Split an implementation step if it combines:

- [Concern one — e.g. UI changes and background task changes]
- [Concern two — e.g. Multiple unrelated API routes]
- [Concern three — e.g. Behavior not clearly defined in
  the context files]

If a change cannot be verified end to end quickly,
the scope is too broad — split it.

## Handling Missing Requirements

- Do not invent product behavior not defined in the
  context files
- If a requirement is ambiguous, resolve it in the
  relevant context file before implementing
- If a requirement is missing, add it as an open question
  in `progress-tracker.md` before continuing

## Protected Files

Do not modify the following unless explicitly instructed:

- [e.g. components/ui/* — generated UI library components]
- [e.g. Any third-party library internals]

## Keeping Docs in Sync

Update the relevant context file whenever implementation
changes:

- System architecture or boundaries
- Storage model decisions
- Code conventions or standards
- Feature scope

## Before Moving to the Next Unit

1. The current unit works end to end within its defined scope
2. Its spec's acceptance criteria pass (if it has a spec)
3. No invariant defined in `architecture.md` was violated
4. Tests, lint, and build pass (commands in CLAUDE.md)
5. `progress-tracker.md` reflects the completed work
6. Any lasting technical decision made during the unit is
   recorded in `decisions.md`

## Backing Up to GitHub

<!-- Delete this section if auto-backup was declined at
     kickoff. Cadence and remote live in CLAUDE.md. -->

A push carries the code and the context spine together, so
a clone never has docs describing a different commit.

**The gate — all three, every push:**

1. **The user confirmed the work is verified** since the
   last push, in their own words. Agent judgement does not
   count; a passing test suite is evidence, not consent.
2. The checklist above is green.
3. The tree holds no debug scaffolding, no temporary edits
   made to reach a test state, no secrets. Revert, re-verify.

**Cadence:** every [3] verified checkpoints, or on request;
the count resets at each push.

**Never:** push unverified work to keep the remote fresh,
push mid-feature, force-push a shared branch, or create a
remote repo without asking whether it should be private.
