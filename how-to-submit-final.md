# How to Submit: Final Game (Deliverable 3)

**Due: Monday, August 18, 2026** — end of day Pacific Time. See Canvas for the official deadline.

**Course grade weight:** **20%** (part of the **30%** Final Project component). Graded out of **20** rubric points — see `rubric.md`.

This is the comprehensive checklist for the **final game submission**. For an overview of all three deliverables, see `how-to-submit.md`. For the design document (Deliverable 1) and demo video (Deliverable 2), refer back to `how-to-submit.md`.

Work through this document top to bottom. Every check is something a grader will verify; missing checks lose points.

---

## Part 1 — Code state

### 1.1 Build cleanly

- [ ] Repository is on a single committed branch (no uncommitted changes)

- [ ] `python run.py` (or the build command from the class you started from) produces a runnable game binary

- [ ] The build emits **zero compiler warnings** with `-Wall -Wextra -Wpedantic`

- [ ] No `.sh` or `.bat` scripts in the repo (Python only)

- [ ] CMake version range is `3.20...4.0`

### 1.2 Game runs end-to-end

- [ ] Game launches without crashing

- [ ] Title screen is reachable

- [ ] In-game state is reachable from the title screen

- [ ] At least one end state is reachable (win, lose, or both — whichever your game has)

- [ ] Game can be exited cleanly (no hangs, no zombie processes)

- [ ] `GameData.json` (or your equivalent) is committed; the game runs with no manual file edits at launch

### 1.3 Tests pass

- [ ] All engine tests from the starting class pass at 100%:
  ```bash
  ctest --test-dir build -R "^Engine" --output-on-failure
  ```

- [ ] All game-specific tests pass at 100%:
  ```bash
  ctest --test-dir build -R "^Game" --output-on-failure
  ```

- [ ] At least **one unit test** for game-specific logic you wrote is present and passing

- [ ] At least **one property test** for game-specific logic you wrote is present and passing

- [ ] Property tests use the standard bounds: `NUM_OUTER_TESTS = 10`, `NUM_INNER_TESTS = 5`

### 1.4 Course standards

- [ ] SDL3 only (no SDL2)

- [ ] Bottom-left coordinate origin; Y-flip happens **only** in `RenderSystem::draw_entity()`

- [ ] Code formatting is consistent with the class you started from

---

## Part 2 — Required files in `submission/`

- [ ] `submission/design.md` — the Deliverable 1 file, still committed and unchanged from its Jul 6 form (or with the divergence noted in `postmortem.md`)

- [ ] `submission/postmortem.md` — the reflection document

The demo video lives **on Canvas**, not in this repo. Confirm it was uploaded for the Aug 10 deadline; nothing about it is checked at the `final` tag.

### `submission/postmortem.md` content check

The postmortem must address all four items:

- [ ] **What worked** — 2–3 design/implementation decisions you would make again

- [ ] **What did not work** — 2–3 things you would do differently

- [ ] **What you cut** — concept-doc items that did not ship, and why

- [ ] **Engine debt** — what you would change about the engine you built across the course

Roughly two pages is the target. Be specific and honest.

---

## Part 3 — Git tag mechanics

The grader will check out the `final` git tag. The commit at that tag is what gets graded.

### 3.1 Create the tag

```bash
git add .
git commit -m "Final submission"
git tag final
git push
git push --tags
```

- [ ] Working tree is clean (`git status` shows nothing to commit)

- [ ] `final` tag exists locally (`git tag --list | grep '^final$'`)

- [ ] `final` tag is pushed (`git ls-remote --tags origin | grep '/final$'`)

### 3.2 If you push more commits after tagging, **move the tag**

```bash
git tag -f final
git push --force-with-lease origin final
```

- [ ] The commit `final` points to is the one you want graded — verify with:
  ```bash
  git log -1 final
  ```

---

## Part 4 — Verification from a fresh clone

This is the single most common failure: the project works on the student's machine and not on the grader's. Reproduce the grader's workflow before you sleep on Aug 17.

```bash
# In a scratch directory, NOT in your working copy:
git clone <your-repo-url> verify-final
cd verify-final
git checkout final
# Follow the build commands from your starting class
python run.py
ctest --test-dir build -R "^Engine" --output-on-failure
ctest --test-dir build -R "^Game" --output-on-failure
```

- [ ] Fresh clone of the `final` tag builds with zero warnings

- [ ] Engine tests pass 100% on the fresh clone

- [ ] Game tests pass 100% on the fresh clone

- [ ] Game runs end-to-end on the fresh clone

- [ ] `submission/design.md` and `submission/postmortem.md` are both present on the fresh clone

If any of these fail on the fresh clone, **fix and re-tag** before the deadline. A submission that fails on a fresh clone forfeits the points tied to the build and the tests (see `rubric.md`).

---

## Part 5 — Final sweep

- [ ] Read `submission/design.md` one more time. Anything you ended up doing differently is mentioned in the video (Deliverable 2, on Canvas) **or** in `postmortem.md` under "What you cut"

- [ ] No secret keys, credentials, or personal information committed

- [ ] No giant binary artifacts committed by accident (check `git log --stat` for anything multi-MB you did not intend)

- [ ] Pushed everything: `git status` is clean, `git push` reports "Everything up-to-date"

---

## You are done

When every box above is checked, the final project is submitted. The grader will:

1. Check out the `final` tag

2. Build the game and run the tests

3. Play the game

4. Watch the demo video from Canvas (Deliverable 2)

5. Read `submission/design.md`, `submission/postmortem.md`, and your code

6. Run the prompt in `LLM-final-evaluation-prompt.md` against the repo and review the output

Anything not on the checklist above will not retroactively help your grade. Anything missing from the checklist will.
