# ED-M7 bounded autosave recovery — Linux evidence (2026-10-09)

Beads slice: `nexora-0ua.2.1`. Base: `3b730fc57d40a8f57d94065e838edc510c566534` (latest origin/main at validation).
Host: Linux x86_64, GNU C++ 14.2.0, C++20, Ninja, Development Modular.
Graphical Editor shell OFF; existing Slang and Zig features ON. Native Vulkan tests use software
Vulkan/Xvfb. No physical display, Windows/macOS, installed IME or screen-reader acceptance is claimed.

## Delivered behavior

- Schema-1 recovery reads at most 127 header bytes, requires unsigned decimal revision/size fields,
  rejects overflow/signs/NUL/extra tokens and validates exact file/payload length before allocating.
- Non-regular files, valid/dangling file symlinks and oversized files reject without rewriting input.
  Binary/empty/64 MiB payloads and maximum uint64 revision remain writer-compatible.
- Tests exercise the exact 127-byte header boundary, one-byte excess, 2 MiB malformed token,
  short/trailing payloads, every truncation of a binary/max-revision journal, and failure preservation
  of source bytes/revision. Successful recovery clears a stale error.
- Mutation fixtures now seed an actual schema-1 workspace recovery journal, restore valid project
  metadata per input and open a writable recovery owner. Rejected recovery preserves live documents,
  committed bytes and journal input. Autosave mutations check unchanged source and failure revision.

## Existing-main integration repair required for validation

The initial full build on the base failed because the merged source omitted the `PreparedSave` class,
combined `PrepareSave` with `Save(path, written_bytes)`, and lost `CaptureRuntimeScene`'s definition.
This branch restores the existing class and capture implementation from the previously delivered
PreparedSave/owning-capture commits, retaining the exact written-bytes Save overload required by
external-save confirmation. It introduces no new schema, class contract or module dependency.
The independent `nexora-6if` task remains owned by its current assignee; this slice does not close it.

## Validation

All commands source `/workspace/.nexora/env.sh` for the already installed toolchain/runtime paths.

| Command | Result |
| --- | --- |
| `cmake --preset linux-development` | PASS |
| `cmake --build --preset linux-development` | PASS |
| `ctest --preset linux-development` | PASS, 124/124, zero skipped, 149.05 s |
| `cmake --preset linux-shipping` | PASS, Monolithic Minimal |
| `cmake --build --preset linux-shipping` | PASS, 74 steps; final incremental gate PASS |
| `git diff --check` | PASS |

The full suite includes `editor.autosave_journal`, `editor.parser_robustness` (17 parsers × 2,000
mutations), `editor.prepared_scene_save`, `editor.external_scene_save_contract` and
`editor.scene_runtime_capture`. Shipping Minimal verifies engine linkage and deliberately excludes
Editor; it is not an Editor Shipping execution claim. No sanitizer run is claimed.

Local logs are in `/workspace/work/nexora-editor/`: `configure-accepted.log`, `build-accepted.log`,
`tests-accepted.log`, `shipping-configure.log`, `shipping-build.log`, `shipping-final-build.log`.
The first base build failure is retained in `build.log`; the missing capture link failure is in
`build-repair.log`. Superseded gate logs are not acceptance evidence.

## Source identity

| File | SHA-256 |
| --- | --- |
| `Engine/Editor/src/SceneAuthoring.cpp` | `53b68f286aff0dae9fffe7b6471ef96ecaec3de1714e699799c4b46c04cf9be7` |
| `Tests/Editor/AutosaveJournalTests.cpp` | `f6bc636d879f0452a57d41ee120ca0a1b734d7b1c25adadf7239d6d11ad28f77` |
| `Tests/Editor/ParserRobustnessTests.cpp` | `7129bee2e28055e11e04a284032a31ee7f0b8d42951b49773fbeb52f2dcd271f` |
| `Engine/Editor/include/Nexora/Editor/EditorWorkspace.h` | `a32d47e88cfef7edc343c44c9c8240420f571ea2995f4e86aa68a5507c44d006` |
| `Engine/Editor/src/EditorWorkspace.cpp` | `5fe8425ecf0b67dfc3f687d468bd7e830c608e1a7aded10e36157203c20d6e01` |

## Scope and remaining gates

This is supporting recovery hardening, not full migration or graphical crash/recovery acceptance.
Calls remain serialized by the host; file preflight does not defend against concurrent substitution.
Review covered all roadmap documents; the two Editor Roadmaps carry the new supporting item, while
the focused ImGui plan and other roadmap milestone claims remain valid without changes. Root README
stays concise and unchanged. ED-M0–ED-M7 remain unaccepted; platform-only gates remain blocked.
