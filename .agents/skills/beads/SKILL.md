---
name: beads
description: Use when working in a repository that uses bd or Beads for durable project task tracking, issue dependencies, blocker management, multi-session handoff, or shared work memory. Trigger when the user asks to find ready work, claim or close tasks, create follow-up work, inspect blockers, recover project context, or choose between local planning and persistent project tracking.
---

# Beads

Use Beads as the shared project task system. Local plans, scratch files, and personal memories are useful, but they are not the durable source of truth for project work.

## Start each task

Use the repository's pinned Beads CLI v1.3.1. If `bd` is missing or the version differs, stop and report it.

Keep the database outside the repository. Choose one writable, task-specific `DOLT_ROOT_PATH` and `BEADS_DIR`, record them in an external environment file, and source that same file before every `bd` command in the task. Reuse the paths when resuming the task; do not allocate a new path per command.

Before injecting Beads context or reading the task graph, run `bd dolt pull`. If it reports that the local database is missing, follow the safe bootstrap steps below. If pull fails for another reason, stop and report the error.

### Adopt existing shared history

This applies to any checkout without a local database, including an existing clone after the repository's first Beads rollout:

1. From the repository root, run `git ls-remote --exit-code origin refs/dolt/data`. Require the existing ref.
2. Run `bd bootstrap --dry-run` and inspect that the plan adopts the existing database from this GitHub origin. Stop if the ref is absent or the plan would create a new database.
3. Run `bd bootstrap --yes`, then `bd dolt pull`.

Never run `bd init`, `bd init --force`, overwrite or rebuild shared history, or select a different database to recover a missing local database.

After synchronization succeeds, run `bd prime` and read its complete output. Before every later task-graph read, including `bd ready`, `bd list`, `bd show`, or dependency queries, run `bd dolt pull` again.

## Core CLI workflow

Use the `bd` CLI when shell access is available.

1. Find work:

```bash
bd ready
bd list --status=open
bd list --status=in_progress
```

2. Inspect before editing:

```bash
bd show <id>
```

3. Claim work atomically:

```bash
bd update <id> --claim
```

4. Create durable follow-up work when implementation reveals new tasks:

```bash
bd create "Short title" --description="Why this exists and what needs to be done" --type=task --priority=2
```

5. Close completed work:

```bash
bd close <id> --reason="Completed"
```

After any change to tasks, dependencies, or shared memory, run `bd dolt push` with the same task paths. If push fails, report the error and preserve local changes for recovery. Do not claim synchronization succeeded until push completes.

## What belongs in Beads

Use Beads for:

- shared project tasks
- blockers and dependencies
- discovered follow-up work
- work that must survive thread reset, compaction, or handoff
- status that another person or agent should be able to resume

Use agent-local planning tools only for the current turn's execution checklist. Do not treat them as shared project state.

## Rules

- Do not create markdown TODO files as the source of truth when Beads is available.
- Do not use `bd edit`; it opens an interactive editor. Use `bd update` flags instead.
- Prefer `--json` when parsing `bd` output programmatically.
- Nexora and its Beads sync data are public. Never store secrets, credentials, personal data, or private notes in tasks, comments, or shared memory.
- Do not auto-close or mutate tasks unless the work is actually complete.
