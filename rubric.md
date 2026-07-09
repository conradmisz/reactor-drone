# Grading Rubric: Class-110 Final Project

## How points map to your course grade

These three deliverables account for **50%** of your course grade (see `2026/syllabus.md` and `narrative.md`, section *How This Fits Your Course Grade*):

| Course component | Course grade weight | This deliverable | Rubric points |
| --- | --- | --- | --- |
| **Design Doc** | **20%** | Design document (July 6) | **20** |
| **Final Project** (demo) | **10%** | Demo video on Canvas (August 10) | **10** |
| **Final Project** (game) | **20%** | Final game — `final` tag (August 18) | **20** |

**Rubric points equal percentage points of your course grade for that deliverable.** Example: 16/20 on the design document is **16%** of your total course grade from that component.

The **research presentation** (Class-100, **10%**) and **weekly assignments** (**40%**) are graded separately.

---

## Deliverable 1: Design Document — 20 points (20% of course grade)

Graded using `Workbook/LLM-Evaluation-Prompt.md`. The LLM produces recommendations; the instructor reviews them before assigning points.

| Criterion | Points | Source |
| --- | --- | --- |
| Section completeness — all 8 required sections present and adequate | 5 | LLM "Section presence" check |
| Scope realism — shippable in available time | 4 | LLM Score A |
| Mechanical clarity — reader can predict the game from the doc | 4 | LLM Score B |
| Engine fit — components/systems plausibly implement the mechanics | 4 | LLM Score C |
| Risk awareness — specific risks, specific plans | 3 | LLM Score D |

A document that is missing more than half the required sections, or describes a non-responsive project, may be returned for resubmission with a late penalty per Canvas policy.

---

## Deliverable 2: Demo Video — 10 points (10% of course grade)

The video is uploaded to the Class-110 Demo Video assignment on Canvas (not committed to the repo).

| Criterion | Points |
| --- | --- |
| Video is uploaded to Canvas on time and under 15 minutes | 2 |
| Game is demonstrated running, with the core loop and an end state visible | 3 |
| Walkthrough of systems and components used to implement the game | 3 |
| Honest discussion of at least one major hurdle and how it was addressed | 2 |

Production value (lighting, microphone, editing) is **not** graded. Substance is.

---

## Deliverable 3: Final Game — 20 points (20% of course grade)

Graded using `Workbook/LLM-final-evaluation-prompt.md`. The LLM produces recommendations for each rubric row; the instructor reviews them before assigning points. The instructor's checklist for verifying these is `Workbook/how-to-submit-final.md`.

| Criterion | Points |
| --- | --- |
| Game builds from a fresh clone with zero compiler warnings using documented commands | 3 |
| Game runs end-to-end: title screen, in-game state, end state (win, lose, or both) | 4 |
| Engine tests from the starting class continue to pass at 100% | 2 |
| At least one unit test and one property test for student-written game logic pass at 100% | 2 |
| Game-specific code follows course standards (SDL3, bottom-left origin, naming, structure) | 2 |
| Game plays as described in the design document — or divergence is explained in the video / postmortem | 3 |
| `submission/postmortem.md` is present and addresses all four required questions | 2 |
| `final` git tag exists and points at the graded commit | 2 |

A submission that does not build receives 0 of the first 3 points and 0 of the test-related points (which depend on the build). It can still earn points for the postmortem and the tag mechanics.

---

## Late Policy

Each deliverable has its own deadline. Late penalties apply per the Canvas course policy and are applied independently — a late design document does not consume the late budget of the final game.

---

## Resubmission

The design document may be resubmitted after grading if the LLM evaluation flagged it as non-responsive. Resubmissions are due within one week of the original grading and are graded with the standard late penalty. The video and final game are not resubmittable — they are graded as turned in.
