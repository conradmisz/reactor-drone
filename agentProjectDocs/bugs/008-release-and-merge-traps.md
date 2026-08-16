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
