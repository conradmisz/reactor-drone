# Feature Spec: Mailing List

## Status

Done (implemented on `feature/distribution`; not yet deployed — see Deploy Steps)

## User Story

As a player, I want to leave my email so that I hear when Reactor Drone
actually ships, and I want to get off that list in one click.

## Requirements

1. Signup from two places: the pilot-name screen in-game, and the Reactor
   Drone page on the website. Both are optional and skippable.
2. Storage is the existing D1 database and the existing Worker — no second
   service, no third-party ESP while the list is this small.
3. Every subscriber row carries a unique unsubscribe token, generated at
   signup, so a mailed unsubscribe link needs no login and reveals nothing
   about any other address.
4. `GET /unsubscribe` only *offers* to unsubscribe; the removal happens on
   `POST`. Mail clients and link scanners prefetch URLs, and a GET that
   deleted the row would silently unsubscribe people who never clicked.
5. Responses must never disclose whether an address is on the list.

## Acceptance Criteria

1. Given a valid address, when it is POSTed to `/subscribe`, then a row exists
   and the reply is `{"ok":true}`.
2. Given an address already on the list, when it is submitted again, then the
   reply is identical to the first-time reply (200 `ok`) and no second row is
   created.
3. Given a malformed address (`nope`, `a b@example.com`), when submitted, then
   the reply is 400 `bad_email` and no row is created.
4. Given the browser form on a different origin, when it POSTs, then the
   preflight returns 204 and the reply carries `access-control-allow-origin`.
5. Given a subscriber's unsubscribe link, when it is *fetched* (GET), then the
   row still exists and a confirm page is served; when the confirm button is
   POSTed, then the row is gone.
6. Given an unknown or already-used token, when POSTed, then the same
   confirmation page is served (no oracle), and a malformed token yields a
   400 page rather than a 500.
7. Given the in-game pilot-name screen, when the player types an email and
   registers, then the address is submitted with `source: "game"`; when the
   player ESCs out instead, then nothing is submitted.
8. Replay determinism is unchanged: two headless runs of the same seed produce
   identical summaries.

## Out of Scope

- Actually *sending* mail. Addresses are collected now; export with
  `wrangler d1 execute` and send from a real ESP when there is something to
  announce. The unsubscribe endpoint exists so the first send can link to it.
- Double opt-in (needs a sender), rate limiting, and CAPTCHA. The email
  primary key dedupes; the worst case today is junk rows, deletable by hand.
- A subscribers view on `/dashboard`. Addresses are the one thing in this DB
  that must not be on a public page.

## Affected Boundaries

- `backend/schema.sql` — new `subscribers` table.
- `backend/src/worker.js` — `POST /subscribe`, `GET|POST /unsubscribe`,
  CORS on every JSON reply plus an `OPTIONS` preflight.
- `assets/GameData.json` — `name_entry` screen gains an email field.
- `CPP/game/main.cpp` — `PHASE_NAME_ENTRY` gains a second text buffer, TAB
  focus switching, and one fire-and-forget `/subscribe` POST.
- `~/Documents/projects/brainstormlabs` (separate repo) —
  `site/reactor-drone/index.html` signup section + `site/style.css`.

## Design Notes

- The email field lives on the existing pilot-name screen rather than a screen
  of its own: it is the one moment the game already asks the player to type,
  and a second screen would be a second thing to skip.
- `pending_subscribe` is fired only after a *confirmed* registration and its
  reply is never read — a signup failure must not hold the player at the name
  screen. It is declared at loop scope and abandoned at shutdown for the same
  reason as `pending_register`: a discarded future blocks in its destructor.
- Email validation in the game is a bare `@` check. Real validation is the
  server's, and the server's is deliberately loose too — the only check that
  proves an address works is mail arriving at it.

## Deploy Steps

1. `npx wrangler d1 execute reactor-drone-db --remote --file backend/schema.sql`
2. `npx wrangler deploy` from `backend/`
3. `BASE=https://reactor-drone-api.conradmiszczak.workers.dev KEY=... bash backend/test.sh`
4. From the site repo: `npx wrangler pages deploy site`

## Open Questions

- Which ESP for the first actual send (Buttondown, Listmonk, MailChannels)?
  Decide when the list is worth sending to; the export is one command either
  way.

## Merge Notes (for `master` at merge time)

- **Proposed decisions.md entry — D202, "Mailing list on the existing Worker".**
  Collect addresses in the D1 database the leaderboard already uses, with a
  per-row unsubscribe token, rather than adding a third-party ESP or a second
  service. *Rejected:* Mailchimp/Buttondown at signup time (an account and a
  monthly floor to hold an empty list); a signup Worker in the website repo (a
  second deploy target and a second D1 binding for one table); double opt-in
  (needs a sender, which does not exist yet). *Ceiling:* this is single opt-in
  with no rate limiting — the upgrade path is an ESP at the first real send,
  and the export is one `wrangler d1 execute` away.
- **project-overview.md → Features:** "Mailing list — signup in-game on the
  pilot-name screen and on the website, one-click unsubscribe page."
- No engine change; `ENGINE.md` untouched by design (the work is game-side
  `main.cpp` input handling plus backend, not frame order or a new system).

## Test Gotcha

`backend/test.sh` asserts exact leaderboard totals (`"score":350`), so it is
**not** idempotent against a dirty database. Re-running it locally without a
reset fails on `/top?mode=total`. Reset first:

    npx wrangler d1 execute reactor-drone-db --local \
      --command "DELETE FROM scores; DELETE FROM players; DELETE FROM subscribers;"

## Not Yet Verified

- **The in-game screen has never been seen rendered.** `name_entry` only
  appears when `net::enabled()`, which headless mode switches off, so the email
  field's position, the TAB caret and the wrapping of the longer hint line are
  all unjudged. This is exactly trap 4 in `bugs/003-verification-traps.md`: a
  save with `registered: false` plus live networking boots straight into this
  screen. Recipe — back up `saves/meta.json`, set `registered: false`, run
  windowed with `--screenshot N` (NOT `scrot`, which captures black on `:1`),
  restore the save. `scripts/drive_ui.py` already does the save shuffle.
  The undeployed `/subscribe` route will 404 during such a run; harmless, since
  the reply is never read.
- The website form has not been opened in a browser — the request contract is
  curl-verified against a local Worker, the DOM wiring is not.
- Nothing is deployed. The remote D1 has no `subscribers` table and the live
  Worker has neither route; the site is still the 2026-08-12 deploy.
