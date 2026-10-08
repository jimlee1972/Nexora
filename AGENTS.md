# Nexora Codex instructions

## Communication

- Reply in Traditional Chinese.
- Keep code, identifiers, commit messages, and pull-request titles in English.

## Required validation

Use the Linux preset in Codex Cloud. Before completing an implementation change, run the full gate:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

Run `linux-shipping` as well when a change affects linkage boundaries. Do not claim that Windows,
macOS, Android, or iOS validation ran in the Linux cloud environment.

## Repository conventions

- Read the relevant README and roadmap document before changing an architectural boundary.
- Keep `Roadmap/en/` and `Roadmap/zh-TW/` synchronized.
- After every content change, review all roadmap documents for affected delivered functionality.
  Mark every newly completed item with the green `✅` symbol (never `[x]`), keep incomplete items
  unmarked, and record detailed completion status and acceptance evidence in the relevant roadmap,
  module documentation, or evidence files. Do not claim completion without acceptance evidence.
- Keep the repository-root `README.md` (the GitHub README) concise and reader-facing. Roadmap
  coverage there must stay at the level of a brief overall progress or major milestone summary,
  with links to `Roadmap/` for details. Update that summary only when the overall progress or a
  major milestone changes; completing a supporting task does not require a README update.
  Do not append per-task completion entries, implementation histories, test counts/timings,
  CI build numbers, commit hashes, or detailed acceptance logs to the GitHub README. Put those
  details in the relevant roadmap, module documentation, or evidence files instead. Before
  finishing a documentation change, check that the GitHub README has not accumulated such
  detailed progress entries. Follow this rule unless the user explicitly requests otherwise.
- Declare module dependencies in `Config/Modules/modules.json`; optional modules require a feature
  option.
- Update the applicable contract README when ownership, lifetime, threading, error, or deferred-work
  behavior changes.
- Format only touched C++ files with `.clang-format`; do not reformat unrelated files.
- Do not commit `CMakeUserPresets.json` or generated build output.

## Delivery

- Inspect the final diff and validation results before committing.
- Use an English conventional commit message.
- After committing, prepare a pull request with an English title and a body that summarizes the
  change and lists the exact validation commands and results.

See `CLAUDE.md` for the longer architecture and milestone guidance shared by cloud coding agents.

## Automated Pull Request Workflow

For every development task in this repository:

1. Follow all existing AGENTS.md and CLAUDE.md instructions.
2. Start from the latest remote main branch.
3. Implement the requested changes.
4. Run all relevant builds, tests, and validation.
5. Update related documentation when necessary.
6. Commit changes to a feature branch.
7. Push the branch and create a PR targeting main.
8. Enable GitHub auto-merge using squash merging:
   gh pr merge --auto --squash
9. Never bypass required CI checks or branch protection.
10. Never automatically merge PRs created by other contributors.
11. If auto-merge is unavailable, report the reason and PR URL.
12. Report the PR URL, test results, and merge status.

Do not require the user to repeat these instructions.

Only create or merge PRs for tasks that actually change
repository files. Do not merge if validation fails.
