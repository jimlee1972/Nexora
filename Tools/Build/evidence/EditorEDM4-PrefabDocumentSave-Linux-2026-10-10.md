# ED-M4 history-preserving prefab document save — Linux, 2026-10-10

Beads: `nexora-pmb.1.3`. Prerequisite: stable wrapped prefab assets (`nexora-pmb.1.1`).

On accepted main `638e982e64a937d97d94c804a1becb4b2ed55301`, graphical Linux
Development configure/build completed (277 full build steps). **236/236** tests
passed, zero skips, **592.27 s**. Minimal Monolithic Shipping configure/build
succeeded (5 steps), with Editor stripped. Existing pinned ImGui and Vulkan
dependency sources were reused without changing dependency versions.

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_BUILD_SHOWCASE=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON \
  -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
```

The focused prefab graph/save test initially passed 1/1 in 1.28 s; after
formatting, its rebuild and focused gate passed 1/1 in 1.23 s. The extended
fixture uses an actual project and wrapped asset files. It covers initial save,
same-asset revision update, stable node/field IDs and exact unknown opaque bytes;
unchanged save preserves revision and pending Redo. Generation, selection,
clipboard and existing Undo/Redo survive confirmed publication. New variants
inherit identities and require the exact current base revision.

Occupied staging directories, stale expected revisions, read-only access and
recovery freeze preserve source files and the previous document baseline. An
identity callback that changes the live document rejects before publication;
another that clears the caller's previous-asset storage does not invalidate the
owning snapshot. Nil factory identities reject without acknowledging a save.

Only confirmed wrapped publication updates the owning saved signature and opaque
baseline. A late document/project change retains any actual publication but
returns an explicit failure without acknowledging the changed document. Calls
serialize on the authoring owner; the workspace and document remain alive for
the call. This is not graphical isolation, nested materialization, a revision
archive or a multi-file crash journal. These remain separate tasks. Stable
C/Gameplay ABI and module dependencies do not change; C++ consumers rebuild.
Full Editor milestones remain **0/8**. Published-head hosted checks remain required.
