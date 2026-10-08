# Nexora Beads workflow

Beads v1.3.1 stores the shared task graph in Dolt. The `origin` Dolt remote uses
`git+https://github.com/jimlee1972/Nexora.git`; shared data lives at `refs/dolt/data`.

This repository and published Beads tasks and shared memory are public. Never store
secrets, personal private information, or private notes.

## Start and synchronize

In a fresh clone, run `bd bootstrap` to adopt the existing remote database.
Do not replace shared history with a new database.

At the start of every task, run `bd prime`. Before reading the task graph, run
`bd dolt pull`, then use `bd ready`, `bd list`, or `bd show <id>`.

After changing tasks, dependencies, or shared memory, run `bd dolt push`.
If synchronization fails, report the exact error; do not force or change remotes.

## Local runtime configuration

`DOLT_ROOT_PATH` must point to a writable per-task directory outside the repository.

Keep this environment setting local. Track configuration and integration files only;
embedded databases, runtime state, and optional JSONL exports are not Git deliverables.

## Validation

```bash
bd setup codex --check
bd dolt remote list
bd doctor --check=artifacts
bd doctor --check=conventions
bd doctor --check=pollution
git ls-remote origin refs/dolt/data
```

In v1.3.1, the full `bd doctor` is unsupported in embedded mode; use the three
specific checks above alongside pull/push and remote-ref verification.

See the repository-root `AGENTS.md` and `CLAUDE.md` for the existing project rules.
