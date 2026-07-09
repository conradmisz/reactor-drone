# Class-110 Final Project: Deliverables

The final project has **three deliverables**. Weights below are **course grade** percentages from `2026/syllabus.md` (see also `narrative.md`, section *How This Fits Your Course Grade*). `rubric.md` scores each deliverable out of **20**, **10**, or **20** points — the same numbers as its course-grade weight.

| # | Deliverable | Due | Course grade weight |
| --- | --- | --- | --- |
| 1 | Design Document | Mon, July 6, 2026 | **20%** (syllabus **Design Doc**) |
| 2 | Demo Video (uploaded to Canvas) | Mon, August 10, 2026 | **10%** (part of **Final Project**) |
| 3 | Final Game (GitHub tag `final`) | Mon, August 18, 2026 | **20%** (part of **Final Project**) |

There is no lecture, no prescribed code, and no intermediate walking-skeleton or vertical-slice checkpoint. You set your own work pace between deliverables.

See `how-to-submit.md` for the mechanics of each submission and `rubric.md` for how each is graded.

---

## Deliverable 1: Design Document — due July 6, 2026

**File:** `submission/design.md`

**Course grade weight:** **20%** (syllabus **Design Doc** row). Graded with `LLM-Evaluation-Prompt.md`.

The design document is where you commit, in writing, to the game you are going to build and the engine pieces you will use to build it.

### Required sections

1. **Game description.** What is the game? Genre, theme, premise, audience. What does the player see when they start it, and what do they do? Two to four paragraphs is plenty.

2. **Core loop.** The single repeating activity at the heart of the game. What does the player do, what is the feedback, and what makes them want to do it again? Three to five sentences.

3. **Controls and player goals.** How does the player interact, and how do they win or lose?

4. **Starting class.** Which class (010 through 090) are you copying as your base, and why? One short paragraph. You can start designing before all classes are released — see the "Picking a Starting Point" section in `narrative.md` for guidance on choosing now versus revisiting your choice as later classes ship.

5. **Components.** List every component you plan to use, marking each as **reused** (taken from a course class — name which), **modified** (taken and changed — say how), or **new** (written for this game). One line per component is enough — name, purpose, source.

6. **Systems.** Same format as components. List every system, mark it reused / modified / new, and write one line on what it does and where it came from.

7. **Data.** What lives in `GameData.json` (or your equivalent)? What gets tuned without recompiling?

8. **Risks.** Two or three things you are not yet sure how to do, and your plan for finding out.

The design document is **not gated** — you do not need instructor approval to start coding. But it is graded, and your final project will be evaluated in part on how closely it matches what you described here. Significant divergence from the design document is fine if you note and justify the change in your video.

---

## Deliverable 2: Demo Video — due August 10, 2026

**Submitted to:** Canvas (not this repository)

**Course grade weight:** **10%** (part of the **30%** Final Project component).

**Length:** up to 15 minutes. No minimum.

The video is an **overview** of the game. It has three parts, roughly in this order:

1. **Demo the game.** Show it running. Play through enough of it that a viewer can see the core loop, win/loss state, and anything visually distinctive. This is the part you should not skip — graders need to see the game working before anything else.

2. **Introduce the systems and components.** Walk through the systems and components you built or adapted to make the game work. You do not need to show every line of code — talk about which systems do which jobs and how they fit together. Reference the design document so a viewer can see what you stuck with and what you changed.

3. **Major hurdles.** Anything that gave you trouble — a system you rewrote, a bug that took a day to track down, an idea you cut, an integration that did not work the first time. Be honest. The graders are more interested in your engineering than in a clean narrative.

### Submitting the video

Upload the video to the Class-110 Demo Video assignment on **Canvas**. MP4 with H.264 is the safe choice for compatibility. Do not commit the video file into this repository — Canvas is the system of record for the video.

---

## Deliverable 3: Final Game — due August 18, 2026

**Files:** the entire repository at the `final` git tag.

**Course grade weight:** **20%** (part of the **30%** Final Project component).

By August 18 the repository must contain:

- A buildable game that runs from a fresh clone with documented build commands and zero compiler warnings

- A title screen, an in-game state, and at least one end state (win, lose, or both)

- A `GameData.json` (or your equivalent) checked in so the game runs without manually editing files at launch

- All engine tests from the class you started from, passing at 100%

- At least one unit test and one property test for game-specific logic you wrote, passing at 100%

- The design document still at `submission/design.md`

- A `submission/postmortem.md` (see below)

The demo video is **on Canvas**, not in this repository.

### `submission/postmortem.md`

A short reflection — two pages is fine — covering:

1. **What worked.** Two or three design or implementation decisions you would make again.

2. **What did not work.** Two or three things you would do differently with another week. Be specific.

3. **What you cut.** What was in your design document that did not make it into the final game, and why.

4. **Engine debt.** What would you change about the engine you built across the course, now that you have used it to build a real game?

---

## Pre-Submission Checklist

Before each due date, verify:

**July 6 — Design Document:**

- [ ] `submission/design.md` is committed

- [ ] All eight required sections are present

- [ ] Components and systems are explicitly marked reused / modified / new

**August 10 — Video:**

- [ ] Video uploaded to the Class-110 Demo Video assignment on Canvas

- [ ] Video shows the game running, walks through systems/components, covers hurdles

- [ ] Video is under 15 minutes

**August 18 — Final Game:**

- [ ] Game builds with zero compiler warnings from a fresh clone

- [ ] All engine tests pass at 100%

- [ ] Game-specific unit + property tests pass at 100%

- [ ] `submission/design.md` and `submission/postmortem.md` are committed

- [ ] Git tag `final` points at the commit you want graded
