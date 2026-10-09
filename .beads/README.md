# Nexora Beads workflow

Beads v1.3.1 stores the shared task graph in Dolt. The `origin` Dolt remote uses
`git+https://github.com/jimlee1972/Nexora.git`; shared data lives at `refs/dolt/data`.

This repository and published Beads tasks and shared memory are public. Never store
secrets, personal private information, or private notes.

## Start and synchronize

Keep the Beads database outside the repository, using one writable path per task.
Record the paths in an external environment file and source that same file for every
`bd` command in that task.

At the start of a task, run `bd dolt pull` before reading Beads context. If it reports
that the local database is missing, this applies to any checkout, including existing
clones after the first install:

1. Confirm `git ls-remote --exit-code origin refs/dolt/data` returns the existing ref.
2. Run `bd bootstrap --dry-run` and verify it adopts the existing database from this
   GitHub origin; stop if the ref is missing or the plan would create a new database.
3. Run `bd bootstrap --yes`, then `bd dolt pull`.

Never use `bd init`, `bd init --force`, or another database to recover missing
shared history. If pull or bootstrap fails for another reason, stop and report the
error rather than replacing data.

After pull/bootstrap succeeds, run `bd prime` and read its complete output. Before
later task-graph reads, run `bd dolt pull` again. After changing tasks, dependencies,
or shared memory, run `bd dolt push`. If synchronization fails, report the exact
error and preserve local changes for recovery.

## Local runtime configuration

`DOLT_ROOT_PATH` and `BEADS_DIR` must point to writable per-task paths outside the
repository. Keep those paths local to the task. Track configuration and integration
files only; embedded databases, runtime state, and optional JSONL exports are not
Git deliverables.

The project uses the Beads skill and explicit task instructions for Codex context.
Native Codex lifecycle hooks are intentionally not installed: they cannot reliably
share the task-specific external database paths used by this workflow.

## Validation

```bash
bd --version
bd dolt remote list
bd doctor --check=artifacts
bd doctor --check=conventions
bd doctor --check=pollution
git ls-remote origin refs/dolt/data
```

In v1.3.1, the full `bd doctor` is unsupported in embedded mode; use the three
specific checks above alongside pull/push and remote-ref verification.

See the repository-root `AGENTS.md` and `CLAUDE.md` for the existing project rules.
