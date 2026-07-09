# Class-110 Narrative: Final Project — Design Your Own Game

## Learning Objectives

After completing this project, you will be able to:

1. **Scope** an original game so it can be designed, built, and polished within the time available

2. **Reuse** code from across the course (Classes 010–090) by lifting the components, systems, and patterns that fit your design

3. **Integrate** engine subsystems — rendering, input, ECS, resources, collision, scripting, animation, AI — into a single coherent game

4. **Test** the game-specific logic you write with unit and property tests, while keeping the engine tests passing at 100%

5. **Present** your finished game to the cohort with a live demo and a short technical walkthrough

---

## Introduction

This is your final project. There is no new engine concept to learn this week. Instead, you take everything you have built across Classes 010–090 and turn it into an original game of your own design.

**This repository is intentionally empty.** There is no `CPP/`, no `tests/`, no `GameData.json`, no `CMakeLists.txt` — just this Workbook and a submission directory. That is the point. Your first task is to copy a starting point into it from one of the classes you have already completed, and then evolve it into your game.

You may copy code from any class in the course. The engine you wrote is yours to use.

---

## How This Fits Your Course Grade

Capstone work is graded on the **course syllabus** (`2026/syllabus.md`), not as a single 100% block inside this repository. The three milestones below are the same deliverables described in `exercises.md`, but the **percentages are course-grade weights**:

| Course component | Weight | Deliverable | Due |
| --- | --- | --- | --- |
| **Design Doc** | **20%** | `submission/design.md` | July 6, 2026 (approval by **July 13**) |
| **Final Project** | **30%** total | Demo video **10%** + final game **20%** | Aug 10 / Aug 18, 2026 |
| Research Paper Presentation | **10%** | Class-100 (separate repo) | July 19, 2026 |
| Weekly Assignments | **40%** | Classes 0–9, two Option modules, and this project | ongoing |

The **design document** is one file: it earns the **Design Doc** course grade (20%) and is also your written plan for the game. The **demo video** and **final game** are the other two milestones; together they are the **30%** Final Project course component (**10%** + **20%**).

`rubric.md` scores each deliverable out of **20**, **10**, or **20** points — the same numbers as its course-grade weight (one rubric point = one percent of your total course grade for that deliverable).

You may start coding before the instructor approves your design doc, but approval by **July 13** confirms scope. **You must pass the final project to pass the course.**

---

## What "Original Game" Means

Original means **designed by you**, not "invented from nothing." You are not required to come up with a genre no one has played before. A platformer, a top-down shooter, a puzzle game, a roguelike, a tower defense variant, a fishing game, a typing game — all are fair game.

What makes the project yours is:

- You chose the mechanics, theme, and goals

- You wrote (or adapted) the components and systems that implement those mechanics

- You tuned the feel — speeds, sizes, colors, timings — so the game plays the way you intended

- You can explain every design decision in your presentation

Copying a course example wholesale and changing only the colors is not original. Taking the Class-050 Asteroids momentum physics and using it to build a moon-lander game is original.

---

## Picking a Starting Point

**Start thinking about your game now.** This document and the design-doc rubric are released early on purpose — the more time you spend turning over game ideas in the background, the better your eventual design will be. You can begin sketching mechanics, scoping, and a candidate starting class as soon as you have read this narrative.

**You do not have to commit to a starting class yet.** Only the classes released so far (and the games they leave behind) are visible to you today. Later classes will introduce additional engine subsystems — collision and physics, spatial partitioning, Lua scripting, animation, pathfinding and AI — and the game from one of those later classes may turn out to be a better foundation for what you want to build than anything available right now. As each new class is released, take a few minutes to look at its game and ask: *would this be a better base for my project than what I picked last week?* It is fine to change your mind.

By the **Design Document deadline (July 6)** you must commit to a starting class in writing. Before then, treat your choice as provisional. After July 6, divergence is still allowed but must be justified in your demo video (see "Deliverable 1" in `exercises.md`).

Each class in the course leaves behind a working game. Any of them is a legitimate starting point for your final project:

- **Class-010** — Hello ECS / red square. Smallest possible base. Useful if you want to build from a clean slate.

- **Class-020** — Timer, input, movement. The minimum needed to make something move on screen.

- **Class-030** — HUD, images, resources. Add when your game needs text and sprites.

- **Class-040** — Camera and debug toolchain. Add when your world is larger than the screen.

- **Class-050** — Collision and Asteroids. Add when entities need to interact physically.

- **Class-060** — Spatial partitioning and OBB collision. Add when you have many entities or need rotated colliders.

- **Class-070** — Lua scripting (Duck Pond). Add when you want gameplay logic that designers can tune without recompiling.

- **Class-080** — Animation and atlases (Gem Crush). Add when you want sprite animation.

- **Class-090** — Pathfinding, waves, AI (Tower Defense). Add when you want autonomous agents that navigate a grid.

Pick the class whose game is closest to the one you want to build, then prune and extend from there. You can also copy individual files (a component, a system, a header) from one class into a base that started from another.

---

## How to Copy a Class Into This Repo

Once you have decided which class to start from, copy its contents into this directory. The starter classes live in the course monorepo under `2026/Class-NNN/`. From the root of the **course monorepo** (not this student repo), run:

```bash
# Pick the class you want as your starting point, for example Class-050:
CLASS=Class-050

# Copy the code, tests, assets, build files, and run scripts into your final-project repo:
cp -r 2026/$CLASS/CPP                /path/to/your/Class-110-repo/
cp -r 2026/$CLASS/tests              /path/to/your/Class-110-repo/
cp -r 2026/$CLASS/assets             /path/to/your/Class-110-repo/ 2>/dev/null
cp    2026/$CLASS/CMakeLists.txt     /path/to/your/Class-110-repo/
cp    2026/$CLASS/GameData.json      /path/to/your/Class-110-repo/ 2>/dev/null
cp    2026/$CLASS/run*.py            /path/to/your/Class-110-repo/
cp    2026/$CLASS/compileGame.py     /path/to/your/Class-110-repo/ 2>/dev/null
```

Replace `/path/to/your/Class-110-repo/` with the path where you cloned your GitHub Classroom repo for this assignment.

After copying, verify the starting point builds and the tests pass before you change a line:

```bash
cd /path/to/your/Class-110-repo/
python run.py            # or follow the build steps from the class you copied
ctest --test-dir build -R "^Engine" --output-on-failure
ctest --test-dir build -R "^Game"   --output-on-failure
```

Commit this baseline as your first commit. Now you have a known-good starting point you can revert to if a refactor goes wrong.

You can also cherry-pick from multiple classes. For example, start from Class-050 (Asteroids physics) but copy `CPP/engine/animation_system.*` from Class-080 if you want sprite animation. When you do this, expect to spend time wiring the new files into your `CMakeLists.txt` and resolving any compile errors — that integration work is part of the project.

---

## What Stays the Same

Whatever you build, the **engine standards from the course still apply**:

- **SDL3 only.** No SDL2.

- **Bottom-left coordinate origin.** Y increases upward. The Y-flip happens only in `RenderSystem::draw_entity()`.

- **Zero compiler warnings.** `-Wall -Wextra -Wpedantic`.

- **CMake** as the build system, version range `3.20...4.0`.

- **All engine tests pass at 100%.** The cumulative engine test suite from the class you started from must continue to pass in your final project. If you remove an engine subsystem, you must also remove its tests — you cannot leave failing tests behind.

- **Property tests use the standard bounds.** `NUM_OUTER_TESTS = 10`, `NUM_INNER_TESTS = 5`.

- **No `.sh` or `.bat` scripts.** Python only.

These are not arbitrary rules — they are what make your project look and behave like a professional codebase, and they are what your peers and the graders will be looking for.

---

## What This Class Replaces

Unlike every other class in the course, Class-110 has:

- **No lecture.** You have all the engine concepts you need.

- **No prescribed components or systems.** The spec is the one you write.

- **No worked example to follow.** Pick a starting class, copy it in, and make it yours.

Instead, you have **three deliverables** on this schedule (see `exercises.md` for section requirements):

| # | Deliverable | Due | Course grade weight |
| --- | --- | --- | --- |
| 1 | Design Document (`submission/design.md`) | Mon, July 6, 2026 | **20%** (syllabus **Design Doc**) |
| 2 | Demo Video (Canvas, up to 15 min) | Mon, August 10, 2026 | **10%** (part of **Final Project**) |
| 3 | Final Game (GitHub tag `final`) | Mon, August 18, 2026 | **20%** (part of **Final Project**) |

The design document commits you to a concrete plan before the heavy build phase. The video is your overview of the running game and how you built it. The final game is the playable artifact graders run from a fresh clone.

---

## Scoping Advice

The most common way final projects fail is **over-scoping**. A polished, finished, 90-second-long game is a better submission than an ambitious half-finished one. Some heuristics:

- If you cannot describe the core loop in one sentence, the scope is too big

- If you need more than three enemy types or more than three levels, the scope is too big

- If you cannot get something on screen and moving within the first few hours of work, the starting point is wrong — pick a simpler base class to build from

- "Cut the scope" is almost always the right answer when you are behind schedule

Plan to spend the last quarter of your time on polish — sound, tuning, the title screen, the win/lose state — not on new features.

---

## Submission

You will submit your game as a tagged commit in this repository (`final`) and your design document and postmortem inside `submission/`. The demo video is uploaded to **Canvas** — it is your overview of the running game, the systems and components you used, and the major hurdles you hit. See `how-to-submit.md` for the mechanics and `rubric.md` for how each deliverable is graded.

---

## Key Takeaways

1. **The engine you built is the asset.** This week you spend that asset on a game of your own design.

2. **Start from working code.** Pick the class whose game is closest to yours, copy its directory, and modify from there.

3. **Cut scope aggressively.** A finished small game beats an ambitious unfinished one every time.

4. **The course standards still apply.** SDL3, bottom-left origin, zero warnings, tests at 100%, Python scripts only.

5. **Polish counts.** Reserve real time for tuning, sound, and end-states — not just features.
