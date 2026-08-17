---
id: 014
title: FEEDBACK from the pause row opens underneath the pause menu
status: resolved
severity: medium
area: ui
opened: 2026-08-16
resolved: 2026-08-16
---

## Symptom

Clicking FEEDBACK on the pause row pushes the feedback screen, but the pause
menu keeps drawing on top of it — the form is "hidden behind the esc menu"
(playtest #3 item 7; gameplay.md flagged the same thing pre-pack).

## Reproduce

1. Start a run, press ESC, click FEEDBACK.
2. The feedback panel appears UNDER the pause panel.

Deterministic — it is arithmetic: UIRenderSystem draws all active screens'
widgets sorted by z_order (entity-id tiebreak), NOT by screen-stack order.
Feedback was authored z 9/10; pause is z 30/40. Stack order is irrelevant —
pause always wins.

## Ruled Out

- **Tested:** checked the click routes and the stack push (main.cpp
  PHASE_FEEDBACK + CMD_PUSH SCREEN_FEEDBACK). **Observed:** the push happens
  and the screen activates. **Eliminates:** routing/stack logic — this is
  purely draw order.

## Suspects

1. **z_order authored below pause's** — confirmed by the table: every screen
   is 0/10 except gameplay (10-22) and pause (30/40); feedback at 9/10 loses
   to pause by construction.

## Resolution

Feedback is always a pushed overlay and must draw above anything it can be
pushed over, including pause. Its z_orders were raised 9/10 → 50/60 (D229).

Verification: by the sort rule itself — UIRenderSystem draws ascending
z_order, and 50/60 strictly outranks pause's 30/40, so pause can no longer
draw over it. A scripted screenshot was NOT possible: `on_feedback_click` is
gated on `net::enabled()`, which is off headless (and a scripted ESC at
frame 200 stalled the paused loop — separate observation, not chased).
Owner re-checks windowed in playtest #4.
