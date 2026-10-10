# Atomic scene forest import — Linux acceptance, 2026-10-10

SceneDocument imports an owning source revision through the official parser/migration before a
single Runtime CloneEntityForest and document metadata transaction. The owner retains no source
World/document borrows; an owning source-ID/current-generation target map supports later prefab binding.

- Full graphical/native Linux Development configure/build/CTest with Slang, Zig, Showcase and
  native Project Player: **234/234**, zero skips, **582.45 seconds**; **271 final build steps**.
  Monolithic `linux-shipping` configure/build: **5 steps**, Editor excluded.
- Actual import and existing Editor Core focus: **2/2**, **0.07 seconds**. Ordered multi-root/child
  fixtures remap internal parents to fresh entities and preserve exact names, sibling order,
  nondefault Camera/Light, full-width unresolved Mesh/shader IDs, 720-degree authored Euler and
  unavailable opaque bytes. Actual scene reopen retains imported data and leaves source unchanged.
- Permission denial, corrupt/truncated input, foreign document generation, stale current content,
  8 MiB raw input and 4096-node ingestion limits preserve content/selection/history. Existing Redo
  survives rejection. One Undo removes the complete import and restores selection; Redo restores
  initialized identities/metadata. Copy and pending-Cut behavior survive import.
- Paste stages replacement metadata/root selection/history capacity before Runtime publication;
  exhausted authoring-generation space rejects rather than wrapping. Existing paste/duplicate/cut,
  opaque and full Editor/native regression contracts remain covered by the complete suite.
- Touched C++ clang-format and `git diff --check` passed. CI explicitly requires registration and
  execution of `editor.atomic_forest_import`.

Commands: repository `linux-development` with graphical shell, Slang, Zig, Showcase and Project
Player/native enabled, `cmake --build --preset linux-development -j4`,
`ctest --preset linux-development`, `cmake --preset linux-shipping`,
`cmake --build --preset linux-shipping -j4`; exact pinned ImGui/Vulkan source overrides are reused.
Windows/macOS/Android/iOS were not executed in this Linux cloud environment.

The caller enforces project writer/recovery/Play/modal authority; the Core transaction also requires
its explicit permission and exact PreparedSave. There is no file IO, GPU admission, native loading,
existing-parent attachment, arbitrary opaque reference interpretation or persistent prefab-instance
binding in this slice. Graphical prefab materialization remains open; progress stays **0/8**.
