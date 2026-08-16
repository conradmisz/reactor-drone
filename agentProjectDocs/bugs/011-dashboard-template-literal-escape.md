---
id: 011
title: Live dashboard hangs on "connecting..." — a template-literal escape killed the whole client script
status: resolved
severity: high
resolved: 2026-08-16
---

## Summary

The deployed live-ops dashboard rendered its shell and then sat on
`connecting…` forever, on every browser and device, showing no data and no
error. Reported from a phone; reproduced in a desktop-class incognito tab.

Root cause: `backend/src/dashboard.js` is **one big JS template literal**
(`export const DASHBOARD_HTML = ` + backtick). The clear-buttons commit
(`cb92348`, 2026-08-15) added

    if (!confirm('Are you sure you want to do this?\n\n' + label + ...

The `\n` escapes were written for the *browser*, but the template literal
consumed them and emitted **real line breaks inside a single-quoted JS
string**. The served page therefore contained a SyntaxError, so the browser
discarded the ENTIRE `<script>` — no polling, no error handler, no timers.
The `connecting…` placeholder is the page's authored initial text, and the
one-second ticker that would have replaced it with `no data` never registered.
That is why the failure looked like a hang rather than a fetch error.

Fix: double-escape (`\\n`) so the literal emits a real escape sequence into the
client script. Comments in that file must avoid a bare escape too — the first
attempt at a warning comment broke the script the same way.

## Ruled out (each actually tested, including the negatives)

- **Production D1 missing tables.** Suspected first, because the branch notes
  said the remote migration was never run. `wrangler d1 execute --remote` shows
  all five tables (`players`, `scores`, `runs`, `feedback`, `subscribers`) and
  every column `/stats` queries. Not it.
- **Worker down / bad deploy.** `/version` returns 200 with `2.2.0`.
- **Auth misconfigured.** `/dashboard` and `/stats` both return a correct 401
  with `www-authenticate: Basic realm="reactor-drone-ops"`.
- **`/stats` server path broken.** `verify_branch.sh` section 5 (local wrangler
  dev + local D1) passes `test.sh`, which asserts `/stats` returns `totals`
  JSON under valid Basic credentials.
- **iOS standalone-PWA Basic-auth credential jar.** Strong suspect (home-screen
  web apps do not reliably reuse Basic credentials for `fetch`). Disproved: the
  same failure reproduces in a plain incognito Safari tab.
- **Stale cache / the `cache-control: public, max-age=300` on an authenticated
  page.** Disproved by a cache-busted URL in incognito. **Still a real
  latent bug** — an authenticated response must not be marked `public`. Not
  fixed here; see below.
- **Missing DOM ids.** All twelve ids the script resolves exist in the HTML.
- **ES6 syntax an older Safari might reject.** The client script is
  deliberately ES5; no arrow functions, `const`/`let`, or optional chaining.

## Why every existing check stayed green

Nothing in the stack ever *parsed* the client script. `test.sh` asserts
`/dashboard` returns 200 and contains its title string; a page whose script is
100% broken passes that. This is the same shape as `bugs/003`: a check that
cannot observe the failure it is supposed to guard.

## Resolution

- `backend/src/dashboard.js`: escapes double-escaped, with a comment stating the
  rule (D226).
- `scripts/verify_branch.sh` section 4: extracts the served `<script>` exactly
  as a browser receives it (by importing the module) and runs `node --check`
  on it. **Verified the guard fails on the broken version and passes on the
  fixed one** — not merely that it passes now.

## Still open

`GET /dashboard` is served `cache-control: public, max-age=300` while sitting
behind Basic auth. `public` on an authenticated response permits shared caches
to store it. Should be `private, no-store`. Untouched here to keep this fix to
one cause.
