# ED-M4 identity-remapped prefab property candidates — Linux, 2026-10-10

Beads: `nexora-pmb.1.10`. Validated on main
`64b20ea7b2fc4250af4900f6c1ace17001170464` with accepted prefab foundation.

Graphical Linux Development configure/build passed. Focused property-plan and
asset tests passed **2/2**, **1.24 s** (5 build steps). Full Development tests
passed **239/239**, zero skips, **596.39 s**. Minimal Monolithic Shipping
configure/build passed (incremental build had no remaining work).

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

Actual Worlds allocate different serialized IDs for equivalent stable node UUIDs.
The owning candidate remaps entity/parent and document metadata IDs into target
IDs while retaining the target scene name/persistence. Names with leading spaces,
local TRS, authored Euler 720 degrees, hierarchy, Camera/Light/Mesh stored data and
exact unavailable opaque bytes survive official parsing and canonical preparation.
Stable node set mismatches, malformed sources and structural additions reject.
Planning does not mutate either live document, perform IO or consume history.

Both sources require official validation, at most 4096 fully tracked nodes and
8 MiB logical scene/candidate/runtime buffers. These are not total allocator
memory bounds. Base/nested references remain metadata outside document properties.
The owning result grants no mutation authority: callers recheck workspace/source
and document observations before an atomic property transaction. Graphical revert,
field selection, structural reconciliation and reference rebase remain open.
Public C++ consumers rebuild; stable C/Gameplay ABI is unchanged. Full Editor
milestones remain **0/8**; current hosted checks are required before merge.
