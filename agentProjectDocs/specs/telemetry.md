# Feature Spec: Global Gameplay Telemetry

## Status

Approved (brainstormed + user-approved 2026-08-11)

## User Story

As the developer, I want anonymous, global-scale gameplay data (where runs
end, where players stand and die, what they buy, which screens they use) so
that balance and UX decisions are made from real play, not guesses.

## Requirements

1. **Per-run summary, not an event stream.** One `RunReport` JSON blob
   (~3–5 KB) collected during play and POSTed once at run end. No per-click
   or per-shot rows — every question asked of the data is a per-run
   aggregate, and D1's free tier is 100K rows/day.
2. **Report contents** — envelope: `v` (schema version), `game_version`,
   `player_id` (existing MetaSave UUID), `session_id`, `seed`, `ship`,
   `prestige`, `difficulty`, `outcome` (death|victory|quit|close), `wave`,
   `score`, `dur_s`, `resumed`. Sections:
   - `waves[]`: per wave started — hp, shield, seconds, damage_taken,
     units_held.
   - `death`: {x, y, wave, killed_by} (absent on non-death outcomes).
   - `heat`: per arena visited, a 32×32 u8 occupancy grid (base64),
     incremented from the player transform every 250 ms.
   - `econ`: units earned/spent, `upg_counts[8]`, items equipped,
     consumables used.
   - `ui`: shop/gear/pause/mechanics/leaderboard open counts,
     minimap+shake settings at run end.
   - `combat`: shots, hits, dashes, bombs.
3. **Client module** `CPP/game/telemetry.{hpp,cpp}`: plain struct + free
   functions owned by `main.cpp` scope. Nothing enters the ECS; no
   ComponentStorage instantiation (Invariant 6). Write-only observation:
   zero RNG draws, zero config writes (Invariant 4).
4. **Transport**: `net::post_json` to `POST /telemetry` with the existing
   `X-Game-Key`, fired from `bank_run_score` (the single run-end site).
   Future held and polled like `pending_register`; never discarded as a
   temporary (8 s destructor block); drained before exit. Failure = report
   silently dropped. No retry queue.
5. **`/score` wired at the same site** — the endpoint exists but the client
   never calls it today; the leaderboard currently receives no data.
6. **Worker + D1**: one `/telemetry` route appending to a `runs` table —
   indexed columns for GROUP BY targets (player_id, version, difficulty,
   prestige, ship, outcome, wave, score, dur_s, ts), full JSON in a `body`
   TEXT column read via `json_extract`. New report fields need no
   migration. Per-route body cap 16 KB. Same validation discipline as
   `/score` (types, ranges, game key).
7. **Consent**: on by default; one disclosure line on the first-launch name
   screen; `ANALYTICS` toggle in Options beside SCREEN SHAKE / MINIMAP
   (`SettingsSave` gains a third bool, same garbage-tolerant defaults);
   toggle off ⇒ no POST. `PRIVACY.md` ships in the installer. No PII —
   the only identifier is the existing anonymous UUID.
8. **Headless/replay unaffected**: collection may run but `net::enabled()`
   already kills the POST under `--stopframe`; replay canary must stay
   byte-identical.

## Acceptance Criteria

1. Given a completed run (death, victory, quit, or window close), when the
   run banks, then exactly one row lands in `runs` whose indexed columns
   match the run and whose `body` parses with all sections present.
2. Given a death at a known position, when the report is decoded, then
   `death.{x,y,wave,killed_by}` match and the occupancy grid's hottest bin
   is where the player sat.
3. Given ANALYTICS off in Options, when a run ends, then no `/telemetry`
   or `/score` request is made and the run still banks locally.
4. Given a leaderboard-eligible run online, when it banks, then `/score`
   receives it and `/top` reflects it.
5. Given a malformed/oversized `/telemetry` body or bad game key, when
   POSTed, then the Worker returns 400/401 and writes no row.
6. Given `--seed 42 --stopframe 3000` twice, then summaries are
   byte-identical and zero network calls occur.
7. Given the full ctest suite, all tests pass, build warning-free (Lua
   `tmpnam` excepted).
8. Given `analytics/report.py` against live D1, then it renders per-arena
   death + occupancy heatmap PNGs and prints outcome split, wave-reached
   histogram, upgrade purchase rate vs. survival, and UI funnel counts.

## Out of Scope

- Offline retry queue (add when online-vs-total run counts diverge).
- Per-event streams, real-time dashboards, a `/stats` endpoint, A/B
  infrastructure, per-player analysis UIs, GDPR data-deletion tooling
  (the UUID is anonymous; deleting `meta.json` orphans the data).

## Affected Boundaries

- `CPP/game/`: new `telemetry.{hpp,cpp}`; hooks in `main.cpp`
  (bank_run_score, arena/wave/shop/UI sites); `settings_save.hpp` third
  bool; options screen row; first-launch disclosure line.
- `backend/`: `worker.js` `/telemetry` route, `schema.sql` `runs` table,
  `test.sh` cases.
- New `analytics/report.py` (wrangler + Pillow, both already present).
- No engine changes; no new components.

## Task Breakdown

1. Backend: `runs` table + `/telemetry` route + `test.sh` cases; deploy.
2. Client: `telemetry.{hpp,cpp}` (RunReport, grid binning, JSON
   serialisation) + `test_telemetry.cpp`.
3. Hooks in `main.cpp`: envelope, waves, death, heat, econ, ui, combat;
   POST + `/score` from `bank_run_score`; future drain on exit.
4. Consent: SettingsSave bool, Options row, name-screen line, PRIVACY.md.
5. `analytics/report.py`.
6. Verify: ctest, warning grep, replay canary ×2, live end-to-end run.

## Open Questions

None.
