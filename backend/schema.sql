CREATE TABLE IF NOT EXISTS players (
  id   TEXT PRIMARY KEY,
  name TEXT NOT NULL UNIQUE COLLATE NOCASE
);
CREATE TABLE IF NOT EXISTS scores (
  player_id TEXT NOT NULL REFERENCES players(id),
  score     INTEGER NOT NULL,
  ts        INTEGER NOT NULL DEFAULT (unixepoch())
);
CREATE INDEX IF NOT EXISTS idx_scores_player ON scores(player_id);
CREATE TABLE IF NOT EXISTS runs (
  id         INTEGER PRIMARY KEY,
  ts         INTEGER NOT NULL DEFAULT (unixepoch()),
  player_id  TEXT    NOT NULL,
  session    TEXT    NOT NULL,
  version    TEXT    NOT NULL,
  difficulty TEXT    NOT NULL,
  prestige   INTEGER NOT NULL,
  ship       INTEGER NOT NULL,
  outcome    TEXT    NOT NULL,
  wave       INTEGER NOT NULL,
  score      INTEGER NOT NULL,
  dur_s      INTEGER NOT NULL,
  body       TEXT    NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_runs_ts ON runs(ts);
CREATE TABLE IF NOT EXISTS feedback (
  id         INTEGER PRIMARY KEY,
  ts         INTEGER NOT NULL DEFAULT (unixepoch()),
  subject    TEXT    NOT NULL,
  body       TEXT    NOT NULL,
  tags       TEXT    NOT NULL DEFAULT '',
  from_name  TEXT    NOT NULL DEFAULT '',
  player_id  TEXT    NOT NULL,
  pilot      TEXT    NOT NULL DEFAULT '',
  version    TEXT    NOT NULL,
  platform   TEXT    NOT NULL,
  session    TEXT    NOT NULL,
  in_run     INTEGER NOT NULL,
  wave INTEGER, score INTEGER, ship INTEGER, prestige INTEGER, difficulty TEXT
);
CREATE INDEX IF NOT EXISTS idx_feedback_ts ON feedback(ts);

-- Mailing list. `token` is the unsubscribe credential mailed out with every
-- send: one per address, never reused, so a leaked link unsubscribes exactly
-- one person. `source` records where the signup came from ('web' | 'game').
CREATE TABLE IF NOT EXISTS subscribers (
  email  TEXT PRIMARY KEY COLLATE NOCASE,
  token  TEXT NOT NULL UNIQUE,
  source TEXT NOT NULL DEFAULT 'web',
  ts     INTEGER NOT NULL DEFAULT (unixepoch())
);
