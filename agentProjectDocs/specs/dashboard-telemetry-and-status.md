# Feature Spec: Dashboard Telemetry, DB Status & Tech Center

## Status

Approved (2026-08-15). Second slice of `ideas/dev-dashboard-prompt.md`, on top
of `specs/dashboard-inbox-and-list.md`. With this slice the idea doc's five
panels all exist in some form; what remains after it is depth, not surface
(heatmaps, UI funnel, per-arena drilldowns).

## User Story

As the developer, I want the ops page to answer "how do runs actually end, how
far do players get, how close am I to the free tier, and how does this stack
fit together" — without opening wrangler or the repo.

## Requirements

1. **Telemetry panel** answering the two headline questions from the telemetry
   spec with the correct chart forms (dataviz method):
   - Outcome split (death / victory / quit / close) — part-to-whole, so a
     single horizontal **stacked bar**, categorical slots 1–4 in fixed order,
     every segment direct-labeled (4 series makes labels mandatory), 2px
     surface gaps between segments. Palette validated for light and dark.
   - Waves reached — magnitude, so a **column histogram** in the single
     sequential hue, one bar per wave bucket, hover tooltip per bar.
2. **The three populations, side by side and labeled.** Registered pilots
   (`players`), pilots with banked scores, and distinct telemetry players —
   the idea doc's "single most important thing": these are different numbers
   and telemetry can legitimately exceed registration (ESC skips name entry
   but not analytics).
3. **DB status panel**: per-table row counts for all five tables; rows written
   today as a **meter** against the D1 free tier's 100K rows/day; live release
   version. Storage-bytes and the Cloudflare GraphQL analytics are out of
   scope (no API token in the Worker, and adding one for a number that moves
   slowly is not worth the secret).
4. **Tech center**: a reference section on the same page — request flow,
   route inventory (method, auth, caps), table→writer map. Kept honest by a
   `verify_branch.sh` check that every route in `worker.js` appears in the
   dashboard's route table, so the list cannot silently rot.
5. **Layout pass** (research-backed): KPI tiles stay on top (inverted
   pyramid); two-column grid ≥960px so related panels sit side by side and
   the page stops being one long single-weight stack; feedback keeps full
   width (prose); tech center last (reference, not live).
6. All new data rides the existing authenticated `/stats` batch — no new
   route, no second poll. `/stats` still never emits a `player_id`.

## Acceptance Criteria

1. Given runs with mixed outcomes, when `/stats` is fetched with credentials,
   then `outcomes`, `waves`, `pops`, and `db` keys are present and correct.
2. Given zero telemetry rows, when the page renders, then the telemetry panel
   shows its empty state and no chart artifacts (no NaN widths).
3. Given the wave histogram, when a bar is hovered, then a tooltip names the
   wave and run count; segments of the outcome bar likewise.
4. Given `verify_branch.sh`, when a route exists in `worker.js` but not in the
   dashboard's tech-center table, then the check fails.
5. Given the palette validator, when run on the four outcome hues for light
   and dark surfaces, then all checks pass (the light-mode contrast WARN is
   discharged by the mandatory direct labels).
6. Given the public routes, when called without credentials, then nothing
   about their behavior changed.

## Out of Scope

- Occupancy/death heatmaps, UI-funnel counts, upgrade-rate-vs-survival, and
  per-session feedback↔telemetry linking (the idea doc's deep telemetry) —
  the next slice, once there is real player data worth exploring.
- `analytics/report.py` (telemetry Task 6): the dashboard now covers the
  outcome/wave renders it was specced for; heatmap PNGs remain its niche if
  it is ever built. Decision deferred until someone misses it.
- D1 storage bytes / Workers request metrics (would need an API token).
- Auto-generating the tech center from source. The verify check pins the
  route list; the rest is prose that changes when the architecture changes.

## Affected Boundaries

- `backend/src/worker.js` — `/stats` batch gains outcome, wave, population
  and table-count queries.
- `backend/src/dashboard.js` — three new sections, two-column layout, the
  charts above.
- `backend/test.sh` — new `/stats` keys.
- `scripts/verify_branch.sh` — route-inventory sync check.

## Design Notes

- Research (dashboard-design guides, game-analytics tools): inverted-pyramid
  layout, restrained accent color, visible "last updated", no layout shift on
  refresh, progression funnels as the core game metric — the wave histogram
  is exactly the progression view GameAnalytics-class tools lead with.
- Charts follow the dataviz skill: color assigned by job (categorical for
  outcome identity, sequential for wave magnitude), fixed slot order never
  cycled, text in ink tokens never series color, recessive grid, tooltips on
  every mark, table fallback (populations and DB numbers are tables/tiles).
- Outcome slot order is canonical death→victory→quit→close, assigned once;
  a filterless page cannot repaint survivors, satisfying color-follows-entity.

## Deploy Steps

Same as the previous slice: `npx wrangler deploy` (DASH_PASS already set).

## Merge Notes (for `master` at merge time)

- Extends proposed D203 rather than a new decision: same page, same auth,
  same no-new-service constraint.
- **project-overview.md → Features** line becomes: "Live-ops dashboard —
  leaderboard, activity, telemetry (outcomes, waves, populations), feedback
  inbox, mailing list, DB status and a stack reference, behind a password."
