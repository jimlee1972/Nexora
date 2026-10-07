# ED recovery presence: Linux evidence

Date: 2026-10-07. Parent: `0b38d8b91ba4b0e87d6be45e3a51c89b22821713` (layout budget).

Recovery detection previously queried whether the journal was a regular file. An occupied
directory or dangling alias therefore looked absent, allowing existing recovery-gated UI/export
and application shutdown actions to proceed. HasRecoveryJournal now inspects the leaf entry
without following aliases and reports any occupied or uninspectable path as pending.
When the leaf appears missing, the metadata parent is inspected too, because Windows may report
a blocked file parent as a missing path. Missing metadata and valid directory aliases remain
compatible; dangling parent aliases remain pending. Only a
missing path (or an unopened/empty workspace root) reports no pending recovery. Recovery reading
still rejects unsafe metadata without publishing a workspace. This changes no public method,
object layout, C SDK wire contract, module dependency or test registration.

Discard remains an explicit writer-only action removing one entry, never recursively. Nonempty
directories fail and remain pending with their contents preserved. Empty directories and leaf
aliases can be explicitly discarded; alias targets are untouched. An absent/uninspectable journal
is not automatically cleaned up by the presence query. This is the existing single-writer model,
not a filesystem race guarantee or newly added permission grant.

Workspace-budget tests verify writer/read-only detection for directories, valid/dangling aliases,
rejected recovery preserving the committed workspace, blocked CSV/JSON export, read-only discard,
nonrecursive discard, target preservation and successful explicit resolution. Replacing the
metadata parent with a regular file verifies fail-closed handling of an uninspectable path;
restoring it preserves the workspace and clears pending state. Missing metadata and valid/dangling
metadata parent aliases are also covered. Unopened and missing paths remain
false. The real Window pointer/button Profiler test now covers a regular journal at 1x DPI and
an occupied recovery directory at 2x DPI, retaining the existing modal export gate.

Environment/isolated tools and socket/Vulkan setup match
[layout budget](EditorEDM7-LayoutBudget-Linux-2026-10-07.md). Final commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development -R 'editor.workspace_budget|editor.profiler'
ctest --preset linux-development
```

Configure/build passed with the graphical shell and Slang enabled. Focused tests: **4/4 passed**,
1.68 seconds. Full CTest: **123/123 passed**, none skipped, **204.40 seconds**, with lavapipe and
Khronos core/synchronization validation. `git diff --check` passed. No linkage boundary changed.
Generated output and local tools remain uncommitted.

Source SHA-256:

- `Engine/Editor/src/ProjectWorkspace.cpp`: `91bde3b7b9a3a0ec635c5f2b3cb8b31f9c1af7c4852e029eceb67cc968bff83d`
- `Tests/Editor/WorkspaceBudgetTests.cpp`: `bfa05b41f6219019274f5e5fe29e85c1c2b99af0a136e6997e2f31f0e9e510ff`
- `Tests/EditorImGui/ProfilerExportTests.cpp`: `b3edc381b202a579fcfd6eaae3ffde17320bbfb9f2c641e2ac4fb33d457ef0fb`

Broader corrupt-document recovery/production-hardening and physical-host acceptance remain open.
Full graphical milestone acceptance remains **0/8**. Hosted checks must pass before merge.
