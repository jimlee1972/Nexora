# ED-M4 stable prefab revision comparison — Linux, 2026-10-10

Beads: `nexora-pmb.1.8`. Validated on main
`64b20ea7b2fc4250af4900f6c1ace17001170464`, with stable prefab foundation and
accepted Crypto/Inspector fix. Graphical isolation and retained revision storage
are separate branches.

Graphical Linux Development configure/build passed (16 focused and 196 full
build steps). **239/239** tests passed, zero skips, **599.09 s**. Minimal
Monolithic Shipping configure/build passed (5 steps). Focused stable comparison
and asset tests passed **2/2**, **1.27 s**.

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

Actual independent Worlds/documents produce equivalent authored sources with
different serialized Runtime IDs. After stable metadata remapping, comparison
reports zero rows. Divergent local/remote names report one three-way conflict at
the stable node/field UUID path; position and exact unknown opaque payload changes
remain independent local rows. Reparenting reports stable parent UUID/root values.
Nested placement presence/source/attachment/order are retained as metadata rows.
Duplicate stable identities, corrupt present sources and unsafe display text
reject; absent sources retain deletion semantics.

All results own their data. Official scene parsing/field capture runs outside live
documents and never performs IO, mutation, Undo or baseline changes. Existing
SceneComparison logical source/value/snapshot/result budgets apply to expanded
UUID paths and opaque hex. These are not total allocator-memory limits. Containing
asset UUID/revision is provenance rather than an authored value; nested/base exact
references are compared explicitly. Conflict/choice rows are read-only inspection
hints, not a structurally validated merged source. Graphical diff/revert/apply/rebase
and transactional publication remain separate work. Public C++ consumers rebuild;
stable C/Gameplay ABI and module graph are unchanged. Full Editor milestones remain
**0/8**. Exact current-head hosted checks are required before merge.

## Accepted native-input Main integration

Only the stable comparison feature is replayed onto accepted Main
`5f5079cec33e8f788e0ff10cb6cf5fe2779abc79` (PR 500 native frame admission), retaining
accepted PrefabDocumentSession and Content index budgets. The replayed feature is
`4e42604557dd3b4081c68e0f3cfb6a78599edb88`; earlier unpublished replays and the old
hosted-green conflicted head do not authorize this integration.

Fresh full graphical/Cryptography/Slang/Zig/Showcase/native ProjectPlayer Development
configure/build passed **314 steps** and **241/241 in 590.08 s**, zero skips. The actual
stable comparison fixture repeated successfully in **0.02 s**. Minimal Monolithic
Shipping reconfiguration/build passed **5 steps**, proving minimal profile/link
compatibility without Editor. Pinned Dear ImGui/Vulkan local cache overrides are
unchanged. No property publication or public contract is added by the replay.
Documentation tests passed **16/16 in 0.901 s** and changed-document links passed;
fresh published-head hosted checks and
an independently clean current Main merge remain mandatory. Full milestones remain **0/8**.
