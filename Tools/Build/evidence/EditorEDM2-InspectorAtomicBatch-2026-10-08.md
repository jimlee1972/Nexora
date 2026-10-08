# ED-M2 reflected Inspector atomic batch evidence

Date: 2026-10-08. Beads implementation task: `nexora-owg.2.1`.
Base: `69f2d85246e76628e12e7a68c4f91493a48ec3ca`; this evidence accompanies the
Inspector batch patch, rather than claiming the base already contains it.

## Delivered scope

`InspectorPropertyAdapter::ApplyBatch` validates a bounded selection and reflected descriptor,
then submits one owning request to the authoring transaction. The transaction owner still
validates live generations, access and component-specific value types, and must commit all
targets as one Undo unit or reject without changing state/history. The adapter does not own
Runtime storage or compensate for an incorrectly implemented transaction callback.

Legacy single-target `Apply` remains available. Legacy multi-target writes reject before invoking
any callback, eliminating the previous sequential-write path that could leave a partial edit.
No stable C/Zig wire, scene schema or module dependency changes.

`editor.inspector_atomic_batch` exercises a real `SceneDocument` with two transforms and opaque
component bytes. It verifies one-step Undo/Redo, later-target rejection with scene and Redo
preservation, caller-owned value-type rejection, legacy compatibility, owning request lifetime,
and empty/zero/duplicate/oversized/read-only/stale/ambiguous/nonfinite rejection. A mock writer
checks the 100,000-target request boundary; this is not a 100,000-node scene performance result.

## Cloud validation

Linux x86_64 managed cloud, GCC 14.2, CMake 3.31.6, Ninja
`1.11.1.git.kitware.jobserver-1`, Slang 2026.18, Xvfb 21.1.16 and Mesa 25.0.7
lavapipe. Tools and extracted dependencies were outside the repository. Xvfb tests ran with
network permission for local display sockets and the lavapipe ICD selected.

The Development cache was configured explicitly with
`NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON` and `NEXORA_ENABLE_SLANG=ON` before this patch.
The required preset gate retained those settings:

```bash
cmake --preset linux-development
cmake --build --preset linux-development -j 4
ctest --preset linux-development
```

Configure and build passed; full CTest **138/138 passed, zero skipped**, 209.21 seconds.
The focused check also passed **2/2**:

```bash
ctest --preset linux-development -R '^editor\.(inspector_atomic_batch|preview_contract)$' --output-on-failure
```

Shipping was configured and built after the patch:

```bash
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j 4
```

Both passed. This preset is Monolithic, disables testing and the graphical Editor, and excludes
the changed Editor implementation; the cached build reported no work to do. It is a Shipping
configuration check, not additional Inspector coverage. `git diff --check` and clang-format 19
checks passed for touched C++ files.

Source SHA-256 identifiers for the tested implementation:

| File | SHA-256 |
| --- | --- |
| `Engine/Editor/include/Nexora/Editor/SceneAuthoring.h` | `5cb28a72f0669348d2f963cff07c519769eadfe0be037b067cd2d2aa6b7cf8f0` |
| `Engine/Editor/src/SceneAuthoring.cpp` | `f221f2c30dd1398065f078b2f742c63395b7508bfcb6bef28a3d5a10bcc65c8b` |
| `Tests/Editor/InspectorBatchTests.cpp` | `d6295bd844761994fb1cfa7ed7a71547a88f36ee7a8a56517158811873a045f4` |

## Remaining acceptance

This delivers an Inspector transaction prerequisite. Complete reflected graphical widgets,
missing-plugin restoration and ED-M2 target-host acceptance remain open. No physical Linux
desktop, Windows/macOS/mobile, native per-monitor DPI or IME acceptance is claimed.
