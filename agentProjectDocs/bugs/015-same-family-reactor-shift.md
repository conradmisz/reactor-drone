---
id: 015
title: Wave-7 REACTOR SHIFT announced Prism while the screen was already Prism
status: resolved
severity: low
area: data
opened: 2026-08-16
resolved: 2026-08-16
---

## Symptom

Between waves 6 and 7 the banner said "Prism — REACTOR SHIFT" but the arena
did not visibly change (playtest #5 item 1). The owner also expected no
boundary at 6→7 at all ("waves change every 4").

## Reproduce

Any run whose shuffled order puts Prism II and Prism in adjacent slots — the
playtest #3 session log shows exactly that (`Core II; Prism II; Prism; ...`).

## Ruled Out

- **Tested:** read begin_arena_shift's guard. **Observed:** it rejects a shift
  to the same INDEX only; Prism and Prism II are different indices sharing a
  backdrop family. **Eliminates:** the shift mechanism — this is a shuffle
  constraint gap.

## Suspects

1. **shuffle_arena_order allows same-family neighbours** — confirmed.

## Resolution

Two facts. (1) The authored ladder is THREE-wave blocks (1,4,7,10,...,28,30),
not four — the 6→7 boundary is real and correct. (2) The shuffle had no rule
against same-family adjacency, so a shift could land on a visually identical
arena. Rule 3 added (D231): a bounded greedy repair pass swaps conflicts
apart, preserving the pinned finale and the no-Prism-opener rule (the first
repair draft violated it — caught by the 200-seed test, which now also pins
no-same-family-neighbours). Verified: 200 seeds green.
