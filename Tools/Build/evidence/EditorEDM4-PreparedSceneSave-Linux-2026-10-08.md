# ED-M4 prepared scene-save snapshot: Linux evidence

Date: 2026-10-08. Beads task: `nexora-pmb.2.2`.
Base: `69f2d85246e76628e12e7a68c4f91493a48ec3ca`; this record accompanies the
prepared scene-save implementation, not the unmodified base.

`SceneDocument::PrepareSave` returns an owning, externally immutable snapshot of scene-file bytes,
document generation, logical content signature and opaque records. Preparation does no file IO and
changes no dirty baseline, selection or history. The existing 64 MiB scene-file limit is retained.
`SavePrepared` rejects changed document generation or serializable content before touching a path;
only successful single-file atomic replacement advances the clean baseline. Ordinary `Save` uses
these same preparation and publication operations.

Authoring calls remain serialized. The snapshot owns its strings and can be copied/retained without
borrowing World/entity storage. The host still owns workspace writer/recovery and destination
checks. Restoring the exact logical content through Undo may make a snapshot current again;
New/Reload invalidates it even when file bytes match. No persistence schema, stable C/Zig wire or
module dependency changes.

`editor.prepared_scene_save` verifies preparation has no IO/history/baseline side effects, owning
copy lifetime, successful deferred publication, edits without a generation change, opaque-only
changes, authored full Euler turns, rejection without file mutation, Undo/Redo preservation,
occupied staging preservation, dirty-state preservation after failed publication, ordinary Save,
Camera/opaque/Euler round trips, cross-document rejection, Reload/New identity and missing Runtime
scene failure. Existing scene-file and preview contracts also run in the full suite.

## Validation

Linux managed cloud, GCC 14.2.0, CMake 3.31.6, Ninja
`1.11.1.git.kitware.jobserver-1`; dependencies and tool configuration live outside the repository.
Development uses the preset defaults: graphical shell OFF, Slang OFF, Modular. Native Vulkan tests
ran on lavapipe/Xvfb with local display-socket permission.

```bash
cmake --preset linux-development
cmake --build --preset linux-development -j 4
ctest --preset linux-development
git diff --check
```

Configure and build passed. Full CTest
**84/84 passed, zero skipped**, **22.83 seconds**. The prepared-save and preview focused checks
passed **2/2**. clang-format 19 checks passed for touched C++ files. The added out-of-line public Editor APIs require the Shipping linkage gate. The initial
validation omitted it; the subsequent review identified this gap and the supplemental
`linux-shipping` configure plus 72-step Monolithic Minimal build passed. Editor/SDK are
stripped by that preset; this is not a claim that it exercises the Editor API. Root README
is unchanged.

| Tested source | SHA-256 |
| --- | --- |
| `Engine/Editor/include/Nexora/Editor/EditorWorkspace.h` | `effb7bcaa989b9ef52e02d8c1db0b93c3e03f233387689bd2a051ae02796012b` |
| `Engine/Editor/src/EditorWorkspace.cpp` | `5a8611de9a4fb9dd5c8f7127719cd983e488ec818c9f7f5ad58a6661ec9f3231` |
| `Tests/Editor/PreparedSceneSaveTests.cpp` | `3cf02de47de06c5a39dbf035319a9db32dbb83e14928da7c15b77be95d57f230` |

This is save-all staging groundwork. It does not provide coordinated multi-file publication,
crash recovery for a group of files, fsync durability, additive tabs or full ED-M4 acceptance.
Physical desktop and Windows/macOS/mobile acceptance were not run here.

## Integration with merged Inspector prerequisite

After PR #440 merged, this branch integrated main
`06b11369daf2f9d0da86098df5343f8e00e122cf`. Both prepared-save and Inspector atomic-batch test
registrations were retained when resolving the CMake conflict. The full preset configure, build
(`-j 2`) and CTest gate ran again: **85/85 passed, zero skipped**, **29.84 seconds**.
The same graphical OFF / Slang OFF settings apply and the prepared-save source hashes above
remain unchanged.

## Integration with measured memory and bounded Console

Integrated main `f21620387e5b27dfb9cce9b341100bb516cc8708` after PRs #443 and #444.
The test-registration conflict was resolved by retaining prepared-save, process-memory and
Inspector batch targets. Preset configure and the **127-step** build (`-j 2`) passed; full
`ctest --preset linux-development` passed **88/88, zero skipped, 22.20 seconds**.
Graphical shell and Slang remain OFF. Prepared-save implementation/test hashes are unchanged.
