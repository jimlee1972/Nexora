# ED-M4 owning prefab document isolation — Linux, 2026-10-10

Beads: `nexora-pmb.1.4`. Prerequisites: stable wrapped assets and history-preserving
SaveDocument (`nexora-pmb.1.1`, `nexora-pmb.1.3`).

On accepted main `638e982e64a937d97d94c804a1becb4b2ed55301` plus those prerequisites,
graphical Linux Development configure/build completed (275 full build steps).
**237/237** tests passed, zero skips, **590.26 s**. Minimal Monolithic Shipping
configure/build succeeded (5 steps), with Editor stripped. Existing pinned ImGui
and Vulkan dependency sources were reused without changing dependency versions.

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

Focused isolation/asset tests passed **2/2**, **1.25 s**, after 30 build steps.
The new isolation fixture uses actual project creation, wrapped asset publication
and close/reopen. It compares exact caller source bytes and generation throughout
separate World/document edits, including preserved unknown provider data.

Create produces an unsaved owning draft. Open validates stored wrapped data;
variant creation requires a saved current source and distinct UUID. Actual
variant save inherits stable node/field IDs and never changes its base file.
Successful saves retain document pointer/generation, selection, clipboard and
Undo/Redo. Dirty owner replacement/close requires explicit discard. A source
changed by foreign publication rejects save and variant creation without clearing
history or advancing the previous baseline.

Read-only scopes can open/inspect/close but cannot mutate or publish. Recovery
blocks admission/edit/save while safe close remains possible. Workspace rebinding
invalidates observations and writes. Identity callbacks cannot close, replace,
reenter save or obtain editable access to the active owner. A throwing callback
preserves the draft and releases its busy guard without publication.

Calls serialize on the authoring owner; the workspace outlives the session.
Document borrows expire on successful replacement, close or destruction, and the
nonwrapping session generation changes at those boundaries. Session destruction
during one of its own calls is unsupported. This is the Core isolation owner,
not graphical controls, a nested materializer, revision archive or override/rebase
engine. Public C++ consumers rebuild; stable C/Gameplay ABI and module dependencies
are unchanged. Full Editor milestones remain **0/8**. Published-head hosted checks
are independently required before merge.
