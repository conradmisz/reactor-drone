---
id: 008
title: Release-process and merge traps found shipping v2.1.0
status: fixed (CI) / documented (the rest)
severity: medium
area: release, docs
opened: 2026-08-15
---

## Symptom

Shipping v2.1.0 needed three manual interventions that a green CI run implied
were unnecessary, and the merge silently corrupted the decision log.

## The traps

1. **CI published a DRAFT.** `release.yml` hardcoded `draft: true`, so the
   assets uploaded but every `releases/download/<tag>/…` URL 404'd. Deploying
   the Worker on "CI is green" would have pointed every player's update button
   and every website button at a 404. **Fixed** — `draft: false`.

2. **Publishing does not move "Latest".** After v2.1.0 went public, v2.0.0 kept
   the badge, so `/releases/latest` and the top of the releases page still
   served the OLD build. **Fixed** — `make_latest: true`. Neither fix has been
   exercised by a real run yet; the next tag is the test.

3. **The website does not deploy on git push.** thebrainstormlabs.com is
   Cloudflare Pages via a MANUAL `wrangler pages deploy site/`. A push updates
   the repo and nothing else — the live page kept serving v2.0.0 links until
   deployed by hand.

4. **`version.hpp` is verified, not stamped.** `release.yml` greps it for the
   tag's number, so tagging vX.Y.Z without bumping `GAME_VERSION` fails the
   Windows job. It is also what the updater compares against: a build reporting
   an old version would offer players an update to the version they are running.

5. **The two decision logs use different formats.** `feature/distribution`
   writes `### D195 — …`; `visual-overhaul` wrote `- **D195 — …**`. Grepping for
   one form reports the other branch's log as EMPTY. That is how the merge
   "fixed" the D194 collision and concluded the rest were clear, when in fact
   D194-D206 all collided. Renumbered to D207-D219; next free id is D220.

## Ruled out

- Not a permissions or token problem: the release job had `contents: write` and
  succeeded. The draft was a deliberate (now removed) workflow setting.
- Not CDN cache on the website: the origin itself was serving the old file
  because the deploy had never run.

## What to do next release

Tag, then verify in this order before announcing: assets return 200
anonymously -> release is Latest -> Worker `/version` -> the site page. Do not
trust "CI green" as "shipped".

---

## v2.2.0 (2026-08-16) — what the fixes actually did, plus one new trap

**Both CI fixes survived their first real run.** `draft: false` and
`make_latest: true` had never been exercised when they were written down above.
On v2.2.0 the release published outright and Latest moved on its own — no
hand-publishing, no manual badge. They are now proven, not merely committed.

**NEW TRAP — `/version` lags a successful `wrangler deploy` by ~10 s.** After a
deploy whose own output listed `env.RELEASE_VERSION ("2.2.0")`, the endpoint
still served `2.1.0`. It was propagation, not a failed deploy. Re-read before
debugging; a cache-buster query is not needed, only patience.

**Ruled out** for that one: not edge caching (a `?cb=` buster and a
`Cache-Control: no-cache` header returned the *same* stale value while it was
stale, then all three flipped together), and not a wrong binding (the deploy
output showed the new value).

## The release order that worked, end to end

1. Bump `GAME_VERSION` in `CPP/game/version.hpp` and **push it before the tag** —
   CI greps version.hpp against the tag (`release.yml`, "Tag matches version.hpp").
2. Push the `v*` tag. Watch all four jobs, not the run status.
3. Verify assets 200 **anonymously** -> Latest -> `/version` -> the live site.
4. Bump `backend/wrangler.jsonc` `RELEASE_VERSION` + `INSTALLER_URL`, then
   `npx wrangler deploy`. **After** the release exists, or `installer_url` 404s.
5. Update the three hardcoded filenames in
   `~/Documents/projects/brainstormlabs/site/reactor-drone/index.html`, then
   `npx wrangler pages deploy site --project-name brainstormlabs`. Pages does
   not deploy on push.

Both `wrangler deploy` and `wrangler pages deploy` ran fine under the agent's
auto mode — they are not blocked.

**Bug-id collision, third occurrence.** The engine-suite merge brought its own
`bugs/003-path-property-test-flake.md` against this branch's
`003-verification-traps.md`; renumbered to `bugs/010`. Check `ls
agentProjectDocs/bugs/` after every merge — the id is in the filename AND the
frontmatter, and both need moving.
