# Reactor Drone — Privacy

Reactor Drone sends two kinds of data to its server (a Cloudflare Worker):

- **Leaderboard**: a random player ID generated on your machine, the pilot
  name you chose, and your run scores.
- **Gameplay analytics** (anonymous): per-run summaries — score, wave
  reached, difficulty, ship, where the drone flew and died (as a coarse
  32x32 grid), what was bought and used, which menus were opened, and
  aggregate combat counters. No account, no email, no hardware IDs, no
  IP-based profiles. Reports are keyed only by the same random ID.
- **Feedback reports** (only when you press send): the subject, message,
  tags and name you typed, plus game version, platform, and — if sent
  mid-run — the current wave/score/ship/difficulty. Keyed by the same
  random ID.

Opt out any time: SETTINGS -> ANALYTICS off. The leaderboard only receives
scores if you registered a pilot name (ESC on the name screen skips it).

Delete your data: the ID lives in `saves/meta.json`; deleting it severs the
link to past reports. For removal of server rows, open an issue on the
GitHub repository with your pilot name.
