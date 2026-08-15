# Feature Spec: Dashboard Inbox & Mailing List

## Status

Approved (2026-08-15). Narrow first slice of `ideas/dev-dashboard-prompt.md` —
it adds two live panels to the existing `/dashboard` and gates the page. The
telemetry explorer, database-status panel and tech-center half of that idea
are explicitly NOT in this slice.

## User Story

As the developer, I want the live-ops page to show feedback as it arrives and
the mailing list as it grows, alongside the leaderboard I already have, so one
page tells me everything players are doing.

## Requirements

1. `/dashboard` and `/stats` require authentication. They currently do not,
   and this feature is the moment that stops being acceptable: it puts
   subscriber email addresses and player-written feedback bodies on the page.
2. Auth is HTTP Basic against a `DASH_PASS` Worker secret. If `DASH_PASS` is
   unset the routes must fail closed, never open.
3. A **Feedback** panel: newest first, subject, body, tags, sender, and the
   auto-attached context (version, platform, pilot, and wave/score/difficulty
   when the report came from inside a run).
4. A **Mailing list** panel: address, signup source (`web` | `game`), and when.
5. Two new tiles: total subscribers and total feedback reports, each with a
   24-hour delta, matching the existing tile row.
6. The leaderboard, activity chart and player table keep working unchanged.
7. Poll interval relaxes from 15 s to 30 s. The page now pulls six queries per
   poll instead of four, and the developer explicitly accepted slower refresh.

## Acceptance Criteria

1. Given no credentials, when `/dashboard` or `/stats` is requested, then the
   reply is 401 with a `www-authenticate: Basic` header and no body content
   from the database.
2. Given correct credentials, when `/stats` is requested, then it returns the
   existing keys plus `feedback` and `subs` arrays and the new totals.
3. Given `DASH_PASS` is unset on the Worker, when `/stats` is requested with
   any credentials, then the reply is 401 — never 200.
4. Given a feedback report containing `<script>`, when the panel renders it,
   then the markup is escaped and not executed.
5. Given a subscriber row, when the panel renders it, then the address appears
   only on this authenticated page and never in `/top` or any public reply.
6. Given the public game routes (`/top`, `/register`, `/score`, `/telemetry`,
   `/feedback`, `/subscribe`, `/unsubscribe`, `/version`), when they are
   called without credentials, then they behave exactly as before.

## Out of Scope

- The telemetry explorer, database-status panel, and the tech-center reference
  half of `ideas/dev-dashboard-prompt.md`. Still wanted; not this slice.
- Triage state (read/unread, resolved) on feedback. Nothing writes yet.
- Replying to feedback from the page. Read-only, per D198.
- Cloudflare Access. Chosen against for now only because it needs the Worker
  on a hostname in a Cloudflare zone, and `thebrainstormlabs.com` is still on
  Squarespace nameservers. See Ceiling below.

## Affected Boundaries

- `backend/src/worker.js` — Basic auth gate; `/stats` gains two queries and
  four totals.
- `backend/src/dashboard.js` — two panels, two tiles, 30 s poll.
- `backend/test.sh` — auth cases and the new `/stats` keys.
- `scripts/verify_branch.sh` — passes `DASH_PASS` to the local worker.

## Design Notes

- Basic rather than a bearer token or a `?k=` query param: the browser holds
  the credential and replays it on the page's own `fetch('/stats')` polls with
  no client code at all. A query param would end up in history and in the
  Referer header.
- Fixed username `dev`. A username adds nothing when there is one operator;
  the password is the whole secret.
- The comparison reuses `safeEqual`, the same constant-time helper `/score`
  and `/telemetry` use for `X-Game-Key`.
- Feedback bodies are player-written and untrusted. They go through the page's
  existing `esc()` on every field, and the body renders in a `pre-wrap` cell so
  newlines survive without `innerHTML` tricks.

## Ceiling

Basic auth is one shared password with no rotation, no audit trail, and no
second factor. The upgrade path is Cloudflare Access on an
`ops.thebrainstormlabs.com` route once that domain's nameservers move to
Cloudflare, at which point the auth block in `worker.js` is deleted rather
than extended. The game's `NET_BASE` stays on the workers.dev hostname either
way — it is compiled into every shipped binary.

## Deploy Steps

1. `npx wrangler secret put DASH_PASS` (from `backend/`)
2. `npx wrangler deploy`

## Merge Notes (for `master` at merge time)

- **Proposed decisions.md entry — D203, "Dashboard is authenticated; Basic
  auth until Access is possible."** *Rejected:* leaving it public (it now
  carries email addresses and untrusted player text); an unlisted URL (not
  auth); Cloudflare Access today (blocked on DNS). *Ceiling:* as above.
- **project-overview.md → Features:** "Live-ops dashboard — leaderboard,
  activity, feedback inbox and mailing list, behind a password."
- No engine change; `ENGINE.md` untouched.
