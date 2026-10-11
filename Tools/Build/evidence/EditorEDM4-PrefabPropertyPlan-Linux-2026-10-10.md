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

## Fresh accepted Main integration

Only the property-plan feature was replayed onto accepted Main
`5f5079cec33e8f788e0ff10cb6cf5fe2779abc79`. The newer owning prefab document
session, its tests and contracts remain present; no prerequisite branch was merged.
Graphical Development rebuilt **208 steps**, and full CTest passed **241/241 in
589.79 s**, zero skips. The property-plan fixture repeated successfully in **0.01 s**.
Minimal Monolithic Shipping passed **5 steps**, proving profile/link compatibility;
Editor is excluded from this Shipping profile. Existing contract and bilingual
supporting roadmap scope remain unchanged; complete graphical milestones stay **0/8**.
Fresh published-head hosted checks and a clean current Main remain required.

## Latest accepted Main integration — 2026-10-11

Only this property-plan feature was replayed onto accepted Main
`89bb06ec2ff30f081702599f83107ed13d707e99`, retaining stable-UUID comparison,
its tests/contracts and the accepted native project-upgrade/Scene Preview fixtures.
Signed-native admission PR 484 merged separately while this gate ran; it is not
attributed to this starting source. Graphical Development rebuilt **316 steps**;
full CTest passed **242/242 in 595.88 s**, zero skips. Minimal Monolithic Shipping
passed **5 steps**, with Editor excluded. No product or assertion changes were
introduced by this integration replay. Bilingual supporting scope remains unchanged,
full milestones **0/8**. Fresh published-head hosted checks and a clean/current Main
guard remain required before ordinary squash merge.

Final documentation regressions passed **16/16 in 1.148 s**; changed-document links
and diff whitespace checks pass. Root README remains unchanged.
