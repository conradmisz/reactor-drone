# How to Submit: Class-110 Final Project

The final project is submitted in three parts. Course grade weights and rubric points are listed in `rubric.md` (**20% / 10% / 20%** = **20 / 10 / 20** points).

| # | Deliverable | Due | Course grade weight |
| --- | --- | --- | --- |
| 1 | Design Document | Mon, July 6, 2026 | **20%** (syllabus **Design Doc**) |
| 2 | Demo Video (uploaded to Canvas) | Mon, August 10, 2026 | **10%** (part of **Final Project**) |
| 3 | Final Game (GitHub tag `final`) | Mon, August 18, 2026 | **20%** (part of **Final Project**) |

Deadlines are end-of-day Pacific Time on the date shown. See Canvas if the official deadline differs from this document.

---

## Submission 1: Design Document — July 6, 2026

**File:** `submission/design.md`

**Course grade weight:** **20%** (syllabus **Design Doc**). See `exercises.md` for the eight required sections. Graded with `LLM-Evaluation-Prompt.md`.

### How to submit

```bash
# From the root of your Class-110 repo, after writing submission/design.md:
git add submission/design.md
git commit -m "Design document"
git push
```

You may start coding before instructor approval, but the professor reviews and approves each design doc by **July 13** (see `2026/syllabus.md`).

---

## Submission 2: Demo Video — August 10, 2026

**Submitted to:** Canvas (not this repository)

**Course grade weight:** **10%** (part of the **30%** Final Project component).

**Length:** up to 15 minutes (no minimum).

The video is an **overview** of the game: demo the game running, walk through the systems and components you used, then talk about your major hurdles. See `exercises.md` for the breakdown.

### How to submit

Upload the video file to the Class-110 Demo Video assignment on **Canvas**. MP4 with H.264 is the safe choice for compatibility. Canvas accepts large video uploads directly — you do not need to host the file elsewhere.

Do not commit the video file into this repository. Canvas is the system of record for the video; the grader will look for it there, not in the repo.

---

## Submission 3: Final Game — August 18, 2026

**Files:** the entire repository at the `final` git tag.

**Course grade weight:** **20%** (part of the **30%** Final Project component).

The final submission is large enough that it has its own document. **See `how-to-submit-final.md` for the comprehensive checklist** — code state, required files, git tag mechanics, and the fresh-clone verification you should run before the deadline.

The short version:

```bash
# Stage your final code state, including submission/postmortem.md:
git add .
git commit -m "Final submission"
git tag final
git push
git push --tags
```

The commit pointed to by the `final` tag is the one that will be graded. If you push more commits after tagging, **move the tag** so the grader sees the right version:

```bash
git tag -f final
git push --force-with-lease origin final
```

---

## Pre-Submission Checklist (per deadline)

**July 6 — Design Document:**

- [ ] `submission/design.md` exists, is committed, and is pushed

- [ ] All eight required sections are present

- [ ] Components and systems are explicitly marked reused / modified / new

**August 10 — Demo Video:**

- [ ] Video uploaded to the Class-110 Demo Video assignment on Canvas

- [ ] Video covers: game demo, systems/components walkthrough, major hurdles

- [ ] Video is under 15 minutes

**August 18 — Final Game:**

- [ ] `final` git tag exists and is pushed

- [ ] Game builds with zero compiler warnings from a fresh clone

- [ ] All engine tests pass at 100%

- [ ] Game-specific unit + property tests pass at 100%

- [ ] `submission/design.md` and `submission/postmortem.md` are committed and pushed

---

## Questions?

Post in the course forum or attend office hours.
