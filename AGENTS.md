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
