# Architecture Context

<!-- Keep under ~120 lines when filled. -->

## Stack

| Layer     | Technology                  | Role   |
| --------- | --------------------------- | ------ |
| Framework | [e.g. Next.js + TypeScript] | [Role] |
| UI        | [e.g. Tailwind + shadcn/ui] | [Role] |
| Auth      | [e.g. Clerk]                | [Role] |
| Database  | [e.g. Prisma + PostgreSQL]  | [Role] |
| [Layer]   | [Technology]                | [Role] |

## System Boundaries

- `[folder]` — [What this folder owns and is responsible for]
- `[folder]` — [What this folder owns and is responsible for]
- `[folder]` — [What this folder owns and is responsible for]
- `[folder]` — [What this folder owns and is responsible for]

## Storage Model

- **[Storage type e.g. Database]**: [What lives here —
  e.g. metadata, ownership, relationships]
- **[Storage type e.g. Blob/File Storage]**: [What lives
  here — e.g. generated files, media, large artifacts]

## Core Entities

[The data model at a glance. Agents must not invent
schema — if an entity or field is not listed here or in
the actual schema files, add it here first.]

| Entity     | Key Fields                     | Relationships              |
| ---------- | ------------------------------ | -------------------------- |
| [e.g. User]    | [id, email, name]          | [has many Projects]        |
| [e.g. Project] | [id, ownerId, name, status] | [belongs to User]         |
| [Entity]   | [fields]                       | [relationships]            |

## Auth and Access Model

- [How authentication works — e.g. Every user signs in
  via Clerk]
- [How ownership works — e.g. Every project has a single
  owner]
- [How access control works — e.g. Only the owner or a
  collaborator can mutate project resources]

## Environment and Setup

- Env vars: [where they are documented — e.g. .env.example
  lists every required variable]
- Local services: [what must be running — e.g. Postgres
  via docker compose up]
- Seed data: [how to get a working local dataset —
  e.g. npm run db:seed]
- Secrets: [where they come from — never committed]

## Invariants

1. [Rule the codebase must never violate — e.g. Request
   handlers do not run long-lived background work]
2. [Invariant two]
3. [Invariant three]
4. [Invariant four]
