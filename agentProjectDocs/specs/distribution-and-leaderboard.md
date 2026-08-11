# Feature Spec: Distribution & Global Leaderboard

## Status

Approved (brainstormed + user-approved 2026-08-11; supersedes the draft in the
course repo's `feature/distribution` branch)

## User Story

As the developer, I want a Windows installer with one-click updates and a
global leaderboard, so other people can install, play, keep current, and
compete on my game.

## Requirements

1. Windows build cross-compiled from Linux (MinGW), official builds + Inno
   Setup installer produced by GitHub Actions on tag `v*`, published to
   GitHub Releases AND itch.io (`butler`).
2. Installer: wizard, Start Menu shortcut, uninstaller in Add/Remove
   Programs, upgrade-in-place. Unsigned (SmartScreen warning accepted).
3. One-click update: title screen checks a version endpoint; "Update
   available — press U": downloads new installer, runs it `/SILENT
   /NORESTART`, exits; installer relaunches the game. The installer IS the
   updater.
4. Global leaderboard on Cloudflare Worker + D1: unique usernames
   (case-insensitive), two views — Highest (max run score) and Cumulative
   (sum of run scores) — top 20.
5. Identity: no accounts. Client-generated UUID persisted locally (extends
   `MetaSave`); display name registered against it, renameable, scores
   follow the UUID.
6. Score auto-submitted where scores bank at run end.
7. Auth: static `X-Game-Key` header on submits (extractable from the binary
   — accepted; real anti-cheat out of scope).
8. Networking hard-OFF whenever `--stopframe` is set: headless runs, tests,
   and the replay canary never touch the network or the simulation.

## Acceptance Criteria

1. Given a tagged commit `vX.Y.Z`, when CI finishes, then a GitHub Release
   holds `ReactorDrone-Setup-X.Y.Z.exe` + a portable zip, and the itch.io
   channel is updated.
2. Given a Windows PC with an older version installed, when the player
   presses U on the update banner, then the game exits, upgrades silently,
   and relaunches at the new version; uninstall via Add/Remove Programs
   removes it.
3. Given a fresh install, when the game first launches online, then it asks
   for a pilot name; a taken name (any case) is rejected with a retry
   prompt; a free name registers and persists across launches.
4. Given submitted runs, when the leaderboard screen is opened, then
   Highest shows each player's max score and Cumulative their sum, both
   descending, ≤20 rows, switchable in-screen.
5. Given `--stopframe`, when the game runs, then zero network calls occur
   and the replay canary is byte-identical to pre-feature output.
6. Given the full ctest suite, when run on Linux, then all existing tests
   pass and the build stays warning-free (Lua `tmpnam` excepted).

## Out of Scope

- Code signing, real anti-cheat, HMAC, self-patching updater, macOS/Linux
  packaged releases, accounts/password recovery, pagination beyond top 20.

## Open Questions

None — resolved in brainstorm (backend=Cloudflare, both GH+itch, unique
names, one-click silent update).
