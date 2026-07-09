# LLM Final-Evaluation Prompt: Class-110 Final Project

This prompt is used to evaluate the **final project** (Deliverable 3) submitted on August 18, 2026. It is run by the instructor against the student's repository at the `final` git tag. It produces a structured evaluation that the instructor reviews and may adjust before assigning a grade.

This prompt is **not** used for the design document (see `LLM-Evaluation-Prompt.md`) or the demo video (graded by hand against the rubric).

---

## Prompt

You are evaluating the final project of a student in CS 5850 (Making Game Engines). The student designed and built an original 2D game in C++/SDL3, on top of the engine they built across Classes 010–090 of the course.

You will be given:

- The student's repository checked out at the `final` git tag
- The student's design document at `submission/design.md` (submitted July 6, 2026)
- The student's postmortem at `submission/postmortem.md`
- A transcript or summary of the student's demo video, provided separately by the instructor (the video lives on Canvas, not in the repo)
- The output of the build and test runs the instructor performed (provided separately)

Grade **only the final project deliverable**, not the design doc or the video as standalone artifacts. The design doc and the video are inputs that help you evaluate the final project's coherence.

### Required checks

For each, state PASS / PARTIAL / FAIL and a one-sentence note. Use the build/test output provided by the instructor — do not assume.

1. **Builds cleanly.** Zero compiler warnings with `-Wall -Wextra -Wpedantic`.
2. **Runs end-to-end.** Title screen → in-game → at least one end state (win, lose, or both).
3. **Engine tests pass at 100%.** From the starting class the student adopted.
4. **Game tests pass at 100%.** Includes at least one unit test and one property test for student-written game logic.
5. **`final` git tag exists** and points at the commit being graded.
6. **`submission/design.md` is present** and matches the document submitted on July 6 (instructor will tell you if it was modified).
7. **`submission/postmortem.md` is present** and addresses all four required questions (what worked, what did not, what you cut, engine debt).
8. **Course standards.** SDL3 only, bottom-left origin (Y-flip only in `RenderSystem::draw_entity()`), CMake range `3.20...4.0`, Python-only scripts, property test bounds `NUM_OUTER_TESTS = 10` / `NUM_INNER_TESTS = 5`.

### Quality scores

Score each of the four criteria below from 0 to 10.

**A. Design-doc fidelity (0–10).** Does the shipped game match what was described in `submission/design.md`? Significant divergence is fine **only if** explained in the video or in `postmortem.md` under "What you cut." Penalize silent divergence (the game does something completely different and the doc is never updated). Reward students who delivered close to their plan, and students who diverged and were transparent about it.

**B. Engineering quality (0–10).** Look at the game-specific code (the systems and components the student added or modified, not the engine code they reused). Is it organized? Are systems doing one thing? Do components have clear purposes? Is the code readable without excessive comments? Are the **reused / modified / new** annotations from the design doc honest in light of the actual code? Use the engine code from the class the student started from as a reference for "good" course style.

**C. Game completeness (0–10).** How finished is the game? A polished, tight, intentional game scores high. A half-finished prototype with missing states, placeholder text everywhere, and obvious incomplete features scores low. Title screen present? End state reachable? Game tunable through `GameData.json`? Sound, HUD, feedback present where the genre calls for them?

**D. Postmortem quality (0–10).** Is the postmortem specific and honest, or generic and self-congratulatory? Reward concrete examples ("the targeting system worked, but I should have written it as a single function returning the nearest entity instead of mutating tower state in place"). Penalize vague reflection ("I learned a lot about game development"). The engine-debt question is the hardest — students who name specific frictions in the engine they built score higher than those who hand-wave.

### Output format

Produce your evaluation in this exact structure:

```
## Required checks
1. Builds cleanly: PASS/PARTIAL/FAIL  — one-sentence note
2. Runs end-to-end: PASS/PARTIAL/FAIL  — one-sentence note
3. Engine tests pass: PASS/PARTIAL/FAIL  — one-sentence note
4. Game tests pass: PASS/PARTIAL/FAIL  — one-sentence note
5. final tag: PASS/PARTIAL/FAIL  — one-sentence note
6. design.md present: PASS/PARTIAL/FAIL  — one-sentence note
7. postmortem.md complete: PASS/PARTIAL/FAIL  — one-sentence note
8. Course standards: PASS/PARTIAL/FAIL  — one-sentence note

## Quality scores
A. Design-doc fidelity: N/10  — one-sentence justification
B. Engineering quality: N/10  — one-sentence justification
C. Game completeness: N/10  — one-sentence justification
D. Postmortem quality: N/10  — one-sentence justification

## Highlights
2–4 bullet points naming the strongest parts of the submission, with file/line references where useful.

## Concerns
2–4 bullet points naming the most important issues, with file/line references. Be specific: not "tests are weak" but "the only property test for game logic (CHECK at game_score_test.cpp:42) is testing the engine's RNG, not the score-calculation logic the student wrote."

## Suggested point adjustments
Reference Workbook/rubric.md (Deliverable 3: Final Game, **20 points** = **20%** of course grade). For each rubric row, recommend the points to award and a one-sentence justification. The instructor makes the final call.

## Overall recommendation
One paragraph: where does this project land overall, what does it do well, and what would move it up a tier?
```

### Notes for the human grader

- The **Required checks** are objective. PASS/FAIL them based on the build and test output, not on your inference about the code.

- The **Quality scores** (A–D) are recommendations. Adjust based on context the LLM cannot see — student circumstances, scope changes approved in office hours, divergences explained verbally in the demo video.

- If the build or the tests fail, items dependent on the build (game runs, game tests, game completeness) should be downgraded. Note this explicitly in the **Overall recommendation**.

- The design-doc fidelity check requires comparing the shipped game to the July 6 document. If the document has been edited after July 6, treat the divergence as silent unless the postmortem or video explains it.
