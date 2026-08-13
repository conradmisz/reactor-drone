# Idea: Dev Ops + Tech Center Dashboard

Status: **idea only** — not a spec, not approved, nothing built. When this gets
picked up it should go through the normal path: brainstorm → `specs/` →
`plans/` → implement.

---

## 1. Original prompt (verbatim, 2026-08-13)

> okay also remind me create a dashboard for viewership of the entire game as a
> dev view. I want to see a live leaderboard, player count (number of people who
> have played at least once, with diagnostics on), easy access to telemetry data,
> I want to be able to see feedback come in, live. I want to see status of my
> databases. I also want this to serve as the technical information center for
> the full-stack view of the game, while also serving that functional purpose. In
> a file, copy this prompt into that file and then create a better prompt based on
> that input for me to then use. Remind me about this dashboard idea if i forget

---

## 2. Refined prompt (use this one)

Copy everything below into a fresh session, from the repo root.

---

**Build a private developer dashboard for Reactor Drone that is both a live ops
console and the technical reference for the whole stack.**

It extends what already exists rather than starting over. Today the Worker
(`backend/src/worker.js`, D1 `reactor-drone-db`) serves eight routes —
`/version`, `/register`, `/score`, `/top`, `/telemetry`, `/feedback`, `/stats`,
`/dashboard` — and `/dashboard` is already a public, self-contained,
no-build-step HTML page that polls `/stats` every 15s (decision D198). This new
view is the **private** sibling: assume it can show things the public page must
not.

### Functional half — the live console

Five panels, all live:

1. **Leaderboard** — highest and cumulative, the same data `/top` serves, but
   without the top-20 cap and with the junk-account problem visible: show
   registered-but-never-played pilots separately instead of letting them drag
   the averages down.

2. **Player counts — and get the three populations right, because they are not
   the same number.** This distinction is the single most important thing in
   this prompt:
   - `players` = pilots who completed name entry (registered).
   - `scores` = runs banked; only registered players reach `/score`.
   - `runs` = telemetry; requires ANALYTICS on but **not** registration.

   So "people who have played at least once with diagnostics on" is
   `SELECT COUNT(DISTINCT player_id) FROM runs` — which can legitimately exceed
   the registered-player count, because a player who skips name entry with ESC
   still sends telemetry. Show all three counts side by side with the overlap,
   and label them so future-you cannot confuse them.

3. **Feedback inbox, live.** Newest first, full subject/body/tags/from plus the
   auto-attached context (version, platform, pilot, and — when `in_run=1` —
   wave/score/ship/difficulty). Filter by tag and platform. Make each report
   one click from the telemetry rows sharing its `session`, which is the join
   key that exists precisely for this. Unread/triaged state is fine to keep
   client-side at first.

4. **Telemetry explorer.** The `runs` table has indexed columns for the GROUP BY
   targets and the full report JSON in `body`, read via `json_extract`. Surface
   the questions the telemetry spec was written to answer: outcome split
   (death/victory/quit/close), wave-reached histogram, upgrade purchase rate
   versus survival, UI funnel counts, and the per-arena 32×32 occupancy and
   death heatmaps decoded from the base64 grids. Note `analytics/report.py`
   (telemetry plan Task 6) was specced for exactly these renders and never
   built — decide whether this dashboard replaces it or reuses it.

5. **Database status.** Per-table row counts and growth rate, D1 storage used
   against the 5 GB free tier, and daily write volume against the 100 K
   rows/day limit that drove the one-row-per-run design. Flag the deploy state:
   `RELEASE_VERSION` from the Worker versus `CPP/game/version.hpp`, and whether
   the live schema has all four tables. Workers observability is already
   enabled in `wrangler.jsonc`, so request counts and error rates can come from
   the Cloudflare GraphQL Analytics API — confirm that before promising it.

### Reference half — the tech center

The same page is the full-stack map, so it must be generated from the repo
rather than hand-maintained, or it will rot:

- Architecture: game client → `net::post_json` → Worker route → D1 table, with
  the actual call sites. There are exactly three network call sites in the
  client and that is worth showing.
- Route inventory: method, auth (game key vs public), body caps, validation
  rules, response codes.
- Schema: all four tables with column meanings and which routes write them.
- Build and release: the `RD_PORTABLE` flag, the Windows/Linux/macOS jobs, what
  each produces.
- Links into `agentProjectDocs/decisions.md` (D195–D201 cover this subsystem),
  the specs, and the plans.

### Constraints

- Cloudflare Worker + D1. No new service, no new bill.
- **Private.** Unlike `/dashboard`, this exposes feedback bodies and player
  identity, so it must be gated — Cloudflare Access in front of the route is
  the answer, not app-level auth code (D198 rejected app auth for the public
  page for the opposite reason).
- Never leak `player_id` to anything public; this page may show it internally.
- Follow the existing page's precedent: self-contained HTML, no build step, no
  chart library, inline SVG, light/dark from the data-viz palette, responsive.
- Read-only. No mutation of game data from this page in v1.
- The `/stats` route stays as-is; the public dashboard must not regress.

### Deliverables

Spec in `agentProjectDocs/specs/`, plan in `plans/`, implementation, backend
test cases in `backend/test.sh`, a `decisions.md` entry at the next free D-id,
and a tracker update. Verify against a local `wrangler dev` + local D1 before
anything touches production, and add a section to `scripts/verify_branch.sh`.

### Open questions to settle during brainstorming

- One page with tabs, or several routes?
- Live means polling (like the 15s existing page) or SSE/WebSocket?
- Does this supersede `analytics/report.py`, or does that stay for offline PNGs?
- Retention: is there ever a reason to delete old telemetry, or does it grow
  forever within the free tier?

---

## 3. Prerequisites

- The production migration + deploy must have run, or `runs` and `feedback`
  will be empty:
  `cd backend && npx wrangler d1 execute reactor-drone-db --remote --file schema.sql && npx wrangler deploy`
- Clearing the ~9 junk test pilots first makes every count on this page honest.
