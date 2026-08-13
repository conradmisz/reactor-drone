# Feature Spec: In-Game Feedback Reports

## Status

Approved (brainstormed + user-approved 2026-08-12)

## User Story

As the developer, I want players to submit feedback (subject, body, optional
tags, optional name) from inside the game, with enough auto-attached context
that an AI can later triage the reports without guessing which build, player
or run each one is about.

## Requirements

1. **Entry points:** a FEEDBACK button on the pause screen and on the main
   menu. Both open the same `PHASE_FEEDBACK` screen (the `PHASE_NAME_ENTRY`
   pattern). Buttons render only when `net::enabled()` — offline/headless
   never sees them.
2. **Form fields:** SUBJECT (required, ≤120 printable ASCII), BODY (required,
   ≤4000, ENTER inserts newline), TAGS (optional, comma-separated free text,
   ≤200), FROM (optional, ≤60). TAB cycles field focus; the focused field
   shows the `_` cursor; ENTER submits from any field except BODY; ESC backs
   out without sending, discarding nothing until the screen is left.
3. **Submission time** is recorded server-side: `ts INTEGER NOT NULL DEFAULT
   (unixepoch())` — client clocks are never trusted.
4. **Auto-attached context** (user-approved bundles, zero submitter typing):
   - build + platform: `version` (GAME_VERSION), `platform` (`win|linux|mac`,
     compile-time constant);
   - player identity: `player_id` (MetaSave UUID), `pilot` (registered name
     or "");
   - live run state, only when opened from pause mid-run: `in_run=1`, `wave`,
     `score`, `ship`, `difficulty`, `prestige`; from the title these are NULL
     and `in_run=0`;
   - `session` (the per-launch session_id) — joins feedback to that launch's
     telemetry rows.
5. **Backend:** `feedback` table (flat columns, no JSON blob — the table IS
   the future AI export) + `POST /feedback` on the existing Worker, same
   validation discipline as `/telemetry`: game key (constant-time), types,
   length caps, 8 KB body cap, printable-ASCII subject/tags/from. Body text
   allows newlines.
6. **Transport:** the `pending_register` single-future pattern. States:
   "Sending..." → "Thanks, received!" | "Couldn't send — check your
   connection", typed content preserved on failure for retry. No retry queue.
7. **Consent:** submitting the form is the consent — NOT gated on the
   ANALYTICS toggle. One PRIVACY.md line: feedback sends what you type plus
   version/platform/run-state, only when you press submit.
8. **Determinism:** the screen is unreachable headless (net-gated), collects
   nothing per-frame, draws zero RNG; replay canary must stay byte-identical.

## Acceptance Criteria

1. Given a mid-run pause, when FEEDBACK is submitted with subject+body, then
   one `feedback` row lands with `in_run=1`, correct wave/score/ship/
   difficulty/prestige, server `ts`, version, platform and player identity.
2. Given the main menu, when a report is submitted, then the row has
   `in_run=0` and NULL run-state columns.
3. Given empty SUBJECT or BODY, when ENTER is pressed, then no POST fires and
   the screen shows which field is missing.
4. Given a missing/wrong game key, oversized body, or non-string field, when
   POSTed directly, then the Worker returns 401/400 and writes no row.
5. Given a failed POST (server down), then the form keeps the typed content
   and shows the failure message; ESC still exits cleanly.
6. Given `--seed 42 --stopframe 3000` twice, summaries stay byte-identical
   and zero network calls occur.
7. Given `wrangler d1 execute ... "SELECT * FROM feedback" --json`, the
   export contains every submitted report with all context columns populated
   per rules above.

## Out of Scope

- Feedback browser / dashboard tab; edit or delete; rate limiting beyond the
  body cap; offline queue; last-run-summary bundle (declined); tag taxonomy
  (free text now, normalize at AI-ingestion time); email/notification on
  submit.

## Affected Boundaries

- `backend/`: `schema.sql` (+`feedback` table), `worker.js` (+`/feedback`
  route), `test.sh` (+cases).
- `CPP/game/`: `main.cpp` (PHASE_FEEDBACK, field buffers, submit future,
  pause + title buttons), `assets/GameData.json` (feedback screen + two
  buttons), `version.hpp` untouched, `PRIVACY.md` (+1 line).
- No engine changes; no new components (Invariant 6).

## Task Breakdown

1. Backend: table + route + test.sh cases (verify local; prod deploy rides
   the pending telemetry migration).
2. GameData.json: feedback screen widgets + FEEDBACK buttons (pause, title).
3. main.cpp: PHASE_FEEDBACK state machine, TAB focus, typed input reusing the
   name_entry char path, submit + status future, context capture at open.
4. PRIVACY.md line; scripted-display walkthrough (open from both entry
   points, submit, verify rows, verify failure path with server down).
5. Canary + ctest + doc sync (decisions.md entry, tracker).

## Open Questions

None.
