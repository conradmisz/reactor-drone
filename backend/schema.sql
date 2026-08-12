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
