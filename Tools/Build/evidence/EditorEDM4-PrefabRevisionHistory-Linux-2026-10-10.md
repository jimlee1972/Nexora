# ED-M4 exact prefab revision history — Linux, 2026-10-10

Beads: `nexora-pmb.1.5`. Validated on document-save source
`ba8bf2b039bc05880000aed546011a18049bbcea`, including the stable prefab asset foundation.

Graphical Linux Development configure/build succeeded (192 full build steps).
**237/237** tests passed with zero skips in **594.19 s**. Minimal Monolithic
Shipping configure/build succeeded (5 steps). The focused prefab asset/history
gate passed **2/2** in **1.20 s** after 12 build steps.

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

The actual-project fixture retains superseded committed revisions and reopens
exact UUID/revision pairs, including two revisions of one asset in the same
nested closure. Stable node/field identities and unknown opaque bytes survive.
Read-only project access resolves historical graphs without publishing.

Occupied archive/current staging paths preserve the current source. Failed
publication retains only the already committed previous revision: editing before
retry does not poison a future revision. Stale writers, conflicting archive
content, oversized/corrupt files, hard links, POSIX directory aliases, recovery
freeze, duplicate exact references and graph cycles reject explicitly.
Existing invalid archive evidence never silently falls back to a current file.

Returned sources/graphs own their data. Publication and authoring serialize on
the project owner. Bounds remain 16 MiB per encoded asset, 64 exact sources and
64 MiB gathered graph bytes, with inherited depth/instance/node checks. These
are logical data budgets, not total process-memory or disk quotas. Retention
does not reconstruct previously overwritten legacy revisions, prune references,
or provide a multi-file power-loss journal. Graphical override/rebase and nested
materialization remain separate work. Stable C/Gameplay ABI and module graph
are unchanged; public C++ consumers rebuild. Full Editor milestones remain **0/8**.

## Accepted native-input Main integration

Only the retained-revision feature replays onto accepted Main
`5f5079cec33e8f788e0ff10cb6cf5fe2779abc79` (PR 500 native frame admission), retaining
accepted PrefabDocumentSession and Content index budgets. The own replay is
`36ea7b8c8ab372c79110226dd4d0173b3fd3093a`; old conflicted/unpublished heads do not
authorize this integration. Sources, tests and contracts for both ownership and
revision history remain registered.

Fresh full graphical/Cryptography/Slang/Zig/Showcase/native ProjectPlayer Development
configure/build passed **515 steps** and **241/241 in 590.10 s**, zero skips. The
actual revision-history fixture repeated successfully in **0.03 s**. Cold Minimal
Monolithic Shipping built **74 steps**. This
profile excludes Editor and does not accept full graphical Shipping. Pinned ImGui/
Vulkan dependency cache overrides are unchanged. Documentation tests passed **16/16 in 0.562 s** and changed-document links passed. Fresh published-head hosted checks and independently clean current Main remain
mandatory before ordinary squash merge. Full graphical milestones remain **0/8**.
