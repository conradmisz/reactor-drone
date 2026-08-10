---
id: NNN
title: One line, symptom not theory
status: open          # open | investigating | resolved | wontfix
severity: high        # high | medium | low
area: [area]          # e.g. backend | ui | build | data | hardware
opened: YYYY-MM-DD
resolved:
---

<!-- Copy to bugs/NNN-slug.md, next free number. Keep under ~80 lines;
     if a bug needs more than that, it is probably two bugs. -->

## Symptom

What was observed, and how. No theory, no guesses — "the total shows
0 for accounts created today", not "the timezone conversion is wrong".

Theory belongs in Suspects, where it can be tested and killed.

## Reproduce

Numbered steps, with the exact inputs, version, and configuration. If
it does not happen every time, say how often.

A bug you cannot reproduce is a bug you cannot prove you fixed. If it
is intermittent, say so explicitly rather than leaving it implied.

## Environment

Whatever a reader would need to recreate the conditions: commit or
version, OS, runtime, browser or device, config flags, and the shape of
the data involved.

## Ruled Out

**Append-only. Never delete a line from this section.**

Each entry states three things:

- **Tested:** what was actually done
- **Observed:** what happened, with numbers or output where possible
- **Eliminates:** which suspect this kills, or "nothing — inconclusive"

Negative results belong here just as much as positive ones. They are
the expensive ones and the ones you forget, and a test whose result was
never written down will be run again.

## Suspects

Ranked most to least likely. Each carries the single test that would
confirm or kill it, so the next session knows what to do without
re-deriving the whole problem.

1. **[suspect]** — test: [the one experiment that settles it]

## Resolution

Root cause, the fix, and how it was verified. Leave empty until the bug
is actually closed.

**A passing build is never sufficient evidence.** Close it when the
original symptom was reproduced and then observed to be gone — not when
the code compiled, and not when the fix "looks right".
