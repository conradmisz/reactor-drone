# LLM Evaluation Prompt: Class-110 Design Document

This prompt is used to evaluate the **design document** (`submission/design.md`) submitted on July 6, 2026 as Deliverable 1 of the Class-110 final project (**20%** of course grade; **20** rubric points in `rubric.md`). It is **not** used to evaluate the final game or the demo video — those are graded separately.

The prompt is run by the instructor against the student's `submission/design.md` along with the contents of this repository. It produces a structured evaluation that the instructor reviews and may adjust before assigning a grade.

---

## Prompt

You are evaluating a game design document submitted by a student in CS 5850 (Making Game Engines). The student is required to design and build an original 2D game using the C++/SDL3 engine they built across Classes 010–090 of the course.

The design document is the **first** of three deliverables (design doc → demo video → final game). You are grading **only the design document**, not the final game.

You will be given the student's `submission/design.md`. Evaluate it against the criteria below. Be specific, cite passages from the document, and flag missing or vague sections explicitly.

### Required sections

The document must contain all eight of the following sections. For each, judge whether it is present and adequate.

1. **Game description** — genre, theme, premise, audience; what the player sees and does. Expect two to four paragraphs.

2. **Core loop** — the single repeating activity at the heart of the game. Three to five sentences. The student should be able to state this in one sentence if pressed.

3. **Controls and player goals** — how the player interacts, and how they win or lose.

4. **Starting class** — which course class (010–090) the student is copying as their baseline, and a justification. One paragraph.

5. **Components** — every component the student plans to use, each marked **reused** / **modified** / **new**, with one line per component (name, purpose, source).

6. **Systems** — every system, same format as components.

7. **Data** — what lives in `GameData.json` (or equivalent) and what gets tuned without recompiling.

8. **Risks** — two or three things the student is unsure how to do, with a plan for each.

### Evaluation rubric

Score each of the four criteria below from 0 to 10.

**A. Scope realism (0–10).** Is this game shippable by a single student in roughly six weeks of part-time work? Penalize concepts that require more than three enemy types, more than three levels, multiplayer, networking, or significant art/audio production beyond what one student can produce. A small, tight game scores higher than an ambitious vague one.

**B. Mechanical clarity (0–10).** Can you, from the document, predict what the game will look and feel like? Is the core loop concrete (specific verbs, specific feedback) or generic ("the player explores a world")? Higher scores for documents where a peer could write a one-paragraph review of the unseen game and be roughly correct.

**C. Engine fit (0–10).** Do the listed components and systems plausibly implement the described mechanics? Are the **reused / modified / new** annotations honest? A student who claims to write everything new in six weeks should be flagged. A student who claims everything is reused but describes mechanics the course engine does not support should also be flagged. Higher scores for honest, plausible mappings.

**D. Risk awareness (0–10).** Are the listed risks real and specific (e.g. "I don't know how to do A* on a non-rectangular grid" — and a plan to spike it) or generic ("I might run out of time")? Penalize generic risks. Reward concrete unknowns with concrete plans.

### Output format

Produce your evaluation in this exact structure:

```
## Section presence
For each of the 8 required sections, state PRESENT / WEAK / MISSING and a one-sentence note.

## Scores
A. Scope realism: N/10  — one-sentence justification
B. Mechanical clarity: N/10  — one-sentence justification
C. Engine fit: N/10  — one-sentence justification
D. Risk awareness: N/10  — one-sentence justification

## Strengths
2–4 bullet points naming the strongest parts of the document, with direct quotes or section references.

## Concerns
2–4 bullet points naming the most important issues. Be specific: "The 'systems' section lists a CollisionSystem as 'reused' but the described game has rotated colliders, which would actually require the Class-060 OBB collision modification — needs to be marked 'modified' or 'new'."

## Questions for the student
2–4 questions the student should answer in the demo video (Deliverable 2). Pick the questions whose answers would most reduce uncertainty about whether the project will ship.

## Overall recommendation
One paragraph: where does this project stand, and what is the single most important thing the student should do next?
```

### Notes for the human grader

- This prompt evaluates the **plan**, not the implementation. A great plan that ships a mediocre game still scores well here; a vague plan that somehow ships a good game does not get retroactive credit on this deliverable.

- The four scores (A–D) are recommendations, not final grades. The instructor may adjust based on context the LLM cannot see (prior conversations, scope changes approved in office hours, student circumstances).

- If the document is fundamentally non-responsive (e.g. describes a different course's project, or fewer than half the required sections are present), say so plainly in the **Overall recommendation** and recommend a resubmission window.
