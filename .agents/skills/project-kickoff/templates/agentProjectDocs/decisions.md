# Decisions Log

Append-only record of significant technical decisions.
Never rewrite or delete past entries — if a decision is
reversed, add a new entry that supersedes it. Read this
before revisiting any settled design question.

Format:

## [YYYY-MM-DD] — [Short decision title]

- **Decision:** [What was decided]
- **Why:** [The reasoning — constraints, trade-offs]
- **Alternatives rejected:** [What else was considered
  and why it lost]
- **Supersedes:** [Link to earlier entry, if any]

---

## [YYYY-MM-DD] — [e.g. Use Postgres over SQLite]

- **Decision:** [e.g. PostgreSQL via Prisma for all
  persistent data]
- **Why:** [e.g. Need concurrent writes and JSON columns;
  deployment target provides managed Postgres]
- **Alternatives rejected:** [e.g. SQLite — no concurrent
  writer support on the chosen host]
- **Supersedes:** —
