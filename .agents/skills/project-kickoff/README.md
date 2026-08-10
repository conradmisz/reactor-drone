# project-kickoff

Turn a project idea into the context files your coding agent actually needs — by interviewing you, not by guessing.

`project-kickoff` is an [Agent Skill](https://docs.claude.com/en/docs/claude-code/skills) for Claude Code. Point it at an empty directory, describe what you want to build, and it runs a structured five-phase interview and writes out a complete `agentProjectDocs/` folder plus a root `CLAUDE.md`. Every session after that starts with real project context instead of re-deriving it.

## Why

Most `CLAUDE.md` files are written once, in a hurry, at the moment you're least sure what you're building. They end up either empty or full of generalities — and a vague context file is worse than none, because the agent trusts it.

The expensive knowledge isn't in your code. It's the stuff that never gets written down: what you decided *not* to build, which invariants the codebase must never violate, why you picked Postgres over SQLite, what "done" means for a feature. Without it, an agent will cheerfully invent a schema field, mix concerns across boundaries you care about, or re-litigate a decision you settled last week.

This skill front-loads that. It asks the questions while the answers are still cheap to change, then writes them into files structured for *conditional* loading — so the agent reads the two files it always needs, and the other five only when the task calls for them.

> [!NOTE]
> The context-file structure here is adapted from the **Six-File Context System** by [Adrian Hajdin / JavaScript Mastery](https://jsmastery.com/waitlist/six-file-context) — his idea, not mine. What this skill adds is the automation: an interview that actually fills the files in, plus a few extra ones. See [Credits](#credits).

## What it generates

```
your-project/
├── CLAUDE.md                            # Entry point: what to read, when, and the real commands
└── agentProjectDocs/
    ├── project-overview.md              # What/who/why, core user flow, in and out of scope, success criteria
    ├── architecture.md                  # Stack, system boundaries, storage, entities, auth model, invariants
    ├── ui-context.md                    # Theme, color tokens, typography, component library, layout patterns
    ├── code-standards.md                # Conventions, testing bar, file organization, and a "Never Do" list
    ├── ai-workflow-rules.md             # How work gets scoped, split, and verified before moving on
    ├── decisions.md                     # Append-only log of technical decisions and their reasoning
    ├── progress-tracker.md              # Current phase, next up, open questions, session notes
    ├── specs/
    │   └── feature-template.md          # Copy per feature; acceptance criteria before code
    └── bugs/
        └── bug-template.md              # Copy per bug; append-only record of what's been ruled out
```

Two design choices do most of the work here.

**Ephemeral state and permanent record are separate files.** `progress-tracker.md` holds only what's true right now — current goal, in progress, next up — and gets pruned as it grows. `decisions.md` is append-only: a reversed decision gets a new entry that supersedes the old one, never an edit. Mixing these is the usual failure mode of a single sprawling context file, where last month's status notes quietly rot into misinformation.

**Bug investigations get their own files, not a section.** `bugs/` holds one markdown file per bug, status in the frontmatter — no index file and no `open/`/`closed/` folders, because both encode status a second time and the two will drift. `rg -N '^(id|title|status):' agentProjectDocs/bugs/[0-9]*.md` is the index, and it cannot go stale. The load-bearing part is an append-only **Ruled Out** section where every entry says what was tested, what was observed, and *what that eliminates*. Negative results are the expensive ones and the ones you forget; without somewhere to put them you re-run the same test in three weeks. This also keeps `progress-tracker.md` honest — one long investigation living there is the usual way that file blows past its line budget.

**Every file carries a line budget.** The templates open with comments like `<!-- Keep under ~120 lines when filled. -->`. Those aren't decoration — they're the reason the system stays usable. Context files get read on every session; a 400-line architecture doc costs you real tokens forever and stops being read carefully.

## How the interview works

Five phases, in this order:

| Phase | Covers | Fills |
| ----- | ------ | ----- |
| 1. Product | What/who/why, core user flow, in and out of scope, success criteria | `project-overview.md` |
| 2. Architecture | Stack, storage, auth model, core entities, invariants, env and setup | `architecture.md` |
| 3. UI | Theme direction, palette, fonts, component library, layout patterns | `ui-context.md` |
| 4. Standards & workflow | Deviations from the defaults only | `code-standards.md`, `ai-workflow-rules.md` |
| 5. Commands | Confirm dev/build/test/lint commands for the chosen stack | `CLAUDE.md` |

The ordering is load-bearing: product answers constrain architecture, and architecture constrains both UI and commands. Asking about your color palette before you know whether this is a CLI wastes everyone's time.

It also **pre-fills aggressively**. The skill drafts answers for every section it can reasonably infer from your initial idea, then asks you to confirm or correct them. You are editing a draft, not filling in a blank form — and phases 4 and 5 are usually a single "any objections?" rather than an interrogation.

Three rules keep the output honest:

- No `[placeholder]` survives. Every bracketed template slot is either filled or moved to Open Questions in `progress-tracker.md`. The skill greps its own output to verify.
- The high-value sections get pushed on. "Out of Scope", "Invariants", and "Never Do" are where the real constraints live, and empty ones aren't accepted.
- "You decide" is a valid answer. The skill decides, then records the choice and its reasoning in `decisions.md` so you can audit it later.

## GitHub auto-backup

Phase 5 asks one more thing: should the project back itself up to GitHub as you go?

Say yes and the generated context files carry a standing instruction to push the code **and** `CLAUDE.md` and `agentProjectDocs/` together, every few checkpoints — so a fresh clone never ends up with documentation describing a commit it doesn't have.

The interesting part is the gate. A push requires that **you** confirmed the work is verified since the last one, in your own words. The agent's own confidence doesn't open the gate, and neither does a green test suite on its own — that's evidence, not consent. The whole point is that nothing reaches the remote unless a human has said it works, which is exactly the failure mode "commit early, commit often" has when an agent is the one doing the committing.

Three more rules come with it: the pre-push checklist has to be green, the working tree must be free of debug scaffolding and the temporary tuning edits agents make to reach a test state, and a repository that doesn't exist yet is never created without asking whether it should be private.

Decline, and the skill strips the backup sections entirely rather than leaving dormant instructions in your context files. Kickoff itself never runs `git init`, creates a remote, or pushes — it writes the policy; the first verified checkpoint acts on it.

## Example

```
$ mkdir tempo && cd tempo
$ claude
> /project-kickoff  a web app for tracking practice time on an instrument,
  with streaks and per-piece breakdowns

● Reading templates...

  Here's what I've inferred — correct anything wrong:
  · Single-user first, no social features
  · Core flow: pick a piece → start timer → stop → see the week
  · Out of scope (my guess): audio recording, sheet music, teacher accounts

? Storage model
  ▸ SQLite via Prisma (Recommended) — single-user, local-first, zero ops
    Postgres via Prisma — if you want hosted multi-device sync later
    LocalStorage only — no server at all

? Is audio recording really out of scope?
  ▸ Yes, out of scope
    In scope — changes the storage model significantly

  [phases 3–4 ...]

? Enable automatic GitHub backups of code and context files?
  ▸ Yes — verified checkpoints only (Recommended)
    Yes — confirm each push
    No — manual pushes only

● Wrote 10 files:
    CLAUDE.md
    agentProjectDocs/  (7 files + specs/ and bugs/ templates)

  Open questions carried into progress-tracker.md:
    · Streak reset rule across timezones — undecided

  Suggested first feature to spec: "start and stop a practice session"
```

## Installation

Any one of these — pick whichever fits how you manage skills.

**Skills CLI**

```bash
npx skills add conradmisz/agent-skills@project-kickoff
```

**Claude Code plugin marketplace**

```
/plugin marketplace add conradmisz/agent-skills
/plugin install project-kickoff
```

**Manual**

```bash
git clone https://github.com/conradmisz/agent-skills.git
cp -r agent-skills/skills/project-kickoff ~/.claude/skills/
```

No dependencies beyond Claude Code itself. The skill writes markdown; it never touches your package manager.

## Usage

Run it from the directory the project will live in:

```bash
mkdir my-project && cd my-project
claude
> /project-kickoff  <your idea, a paragraph is plenty>
```

You can also just describe the situation — "I'm starting a new project, set up the context files" — and the skill triggers on its own.

> [!IMPORTANT]
> Output goes to the **current working directory**. Start Claude Code from inside the project folder, not from its parent.

> [!NOTE]
> If `agentProjectDocs/` already exists, the skill won't overwrite it — it offers to review and update instead. It's also not meant for one-off scripts; the overhead only pays off for something you'll come back to.

It produces documentation only. No application code is scaffolded, no dependencies installed, no framework chosen on your behalf without asking.

## Customizing the templates

The templates in [`templates/`](templates) are opinionated on purpose, and they're meant to be edited. They currently lean toward a TypeScript web stack in their example values — if you mostly write Rust, or firmware, or data pipelines, rewrite the placeholders in `architecture.md` and `code-standards.md` to match. The skill reads whatever is in `templates/` at run time, so your changes take effect immediately.

Two things worth keeping if you do fork them:

- **The line-budget comments.** They're what stops context files from bloating into something nobody reads.
- **The "Never Do" section in `code-standards.md`.** Negatively-phrased, specific prohibitions outperform positive guidance in practice — "never hardcode hex values, use the tokens in `ui-context.md`" lands where "use design tokens consistently" does not.

## Requirements

Claude Code. That's it.

## Credits

The context-file structure is adapted from the **Six-File Context System** by **Adrian Hajdin** ([JavaScript Mastery](https://github.com/adrianhajdin)) — the idea that an agent should be handed a small set of purpose-separated context files, rather than one sprawling `CLAUDE.md`, is his. I ran into it via his [`ghost-ai`](https://github.com/adrianhajdin/ghost-ai) project and have used it on everything since.

His original is available as a free guide: **[jsmastery.com/waitlist/six-file-context](https://jsmastery.com/waitlist/six-file-context)**. Go read it — it explains the reasoning behind the structure far better than this README does.

What this skill contributes is the part his guide leaves to you:

- **The interview.** A five-phase, pre-filled questioning pass that populates the files instead of handing you templates to fill in by hand.
- **Four additional files.** `decisions.md` (append-only decision log), `progress-tracker.md` (ephemeral state, deliberately split from decisions), `specs/feature-template.md` (per-feature acceptance criteria), and `bugs/bug-template.md` (one file per bug, with an append-only record of what each test has ruled out).
- **Enforcement rules.** No `[placeholder]` may survive into the output; "Out of Scope", "Invariants", and "Never Do" can't be left empty.

The templates in [`templates/`](templates) are my own rewrite of the structure, so any clumsiness in them is mine and not his.
