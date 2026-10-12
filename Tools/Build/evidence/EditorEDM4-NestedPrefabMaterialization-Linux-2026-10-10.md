# ED-M4 transactional nested prefab materialization — Linux, 2026-10-10

Beads: `nexora-pmb.1.7`. Source builds on document-save `0da9d4b9`, revision
history `ba90979d`, atomic forest import `97827a9e` and accepted Crypto/Inspector
main `99db9d22`. Graphical isolation bindings are a separate branch.

Graphical Linux Development configure/build passed (45 focused and 292 full
build steps). **241/241** tests passed, zero skips, **612.09 s**. Minimal
Monolithic Shipping configure/build passed (5 steps). Focused materialization,
revision history, forest import and asset tests passed **4/4**, **1.37 s**.

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_CRYPTOGRAPHY=ON -DNEXORA_ENABLE_SLANG=ON \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
```

The actual-document fixture materializes a variant with two independently scoped
placements of distinct revisions of one nested asset. The base is validated but
does not become a duplicate instance. Nested roots attach to the stable parent
node, preserving local transform, authored Euler 720 degrees, names, child parents
and unavailable opaque payloads. Returned stable UUID/scope mappings identify
distinct current-generation entities. One Undo restores exact previous bytes
and selection; Redo restores all entities and IDs. The preexisting user clipboard
still pastes its original content.

Unauthorized, stale observation/revision, corrupt source and cyclic graph inputs
reject without mutation or consuming Redo. Existing graph/forest gates cover
missing sources, depth/instance/node/byte budgets and atomic parser/clone failure.
All result storage and translation indices exist before the single live import.
Publication performs no filesystem IO and retains no source/document/World borrows.
Calls serialize on the authoring owner; the host supplies project/Play/recovery
authority. Temporary staging histories are not a total memory budget. Persistent
instance metadata, graphical instancing/override/revert/apply/rebase remain open.
Public C++ consumers rebuild; stable C/Gameplay ABI and module graph are unchanged.
Full Editor milestones remain **0/8**. Exact published-head hosted gates are required.
