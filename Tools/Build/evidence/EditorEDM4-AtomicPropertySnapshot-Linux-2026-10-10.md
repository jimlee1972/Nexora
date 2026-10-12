# ED-M4 atomic document property snapshots — Linux, 2026-10-10

Beads: `nexora-pmb.1.9`. The final integration replays only the atomic property
feature onto accepted Main `5ecb6871319449581739da8c5770d5d96c0ca3f8`, including
accepted isolated ownership and Content index budgets. Both owner and snapshot
contracts are retained. The old green but conflicted head is not merge authority
for this new integration.

Graphical Linux Development configure/build passed (314 steps). **241/241** tests
passed, zero skips, **601.65 s**. Minimal Monolithic Shipping configure/build passed
(5 steps). Focused atomic property snapshot test passed **1/1**, **0.02 s**.
Changed Markdown validation and the 16-test documentation regression suite pass.

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

Actual saved documents exercise a complete mixed property replacement: names,
hierarchy/storage order, local TRS, Euler 720 degrees, Camera/Light/Mesh and unknown
opaque data. A disabled Camera retains its nondefault stored FOV 75. Existing
generation keys, selection and clipboard remain intact; the saved baseline becomes
dirty and returns clean after one Undo. Redo restores exact candidate bytes.
Interleaved ordinary transform and rename edits keep coherent independent histories.

No-op input preserves pending Redo. Unauthorized, corrupt, over-budget, foreign
generation and valid identity-changing sources reject without mutation/history loss.
An external Runtime change rejects snapshot Undo, preserving bytes and cursor;
restoring that value permits the same Undo/Redo to succeed. Clipboard still pastes
its original source after snapshot replay.

Official parsing stages complete data before the existing atomic Runtime replacement;
all document/history storage exists before publication. No Runtime API change or
filesystem publication is introduced. Same fully tracked entity/node identity sets,
scene name/persistence and current owning PreparedSave are required. Limits are
4096 nodes, 8 MiB source/prepared/runtime bytes and 1024-byte UTF-8 NUL-free names.
History has no total memory quota. Successful replacement expires entity/node-name
borrows while preserving document pointers/generation keys. The host serializes owners
and supplies project writer/recovery/Play authority. Structural additions/deletions,
prefab field selection and graphical revert/rebase remain separate work. Public C++
consumers rebuild; stable C/Gameplay ABI/module graph are unchanged. Full Editor
milestones remain **0/8**. Current-head hosted checks remain required before merge.

## Accepted native-input Main integration

Only the atomic snapshot code and its evidence replay onto accepted Main
`5f5079cec33e8f788e0ff10cb6cf5fe2779abc79` (PR 500 native frame admission), retaining
accepted PrefabDocumentSession and Content index budgets. The replayed feature head is
`c7b4be7e76b565f01204e31462fbc744d4585b20`; previous green/conflicted and unpublished
intermediate heads do not authorize the new integration.

Fresh full graphical/Cryptography/Slang/Zig/Showcase/native ProjectPlayer Development
configure/build passed **313 steps** and **241/241 in 590.36 s**, zero skips. The atomic
snapshot fixture repeated successfully in **0.01 s**. Minimal Monolithic Shipping
reconfiguration/build passed **5 steps**, excluding Editor and proving minimal
profile/link compatibility only. Pinned ImGui/Vulkan cache overrides are unchanged.
Documentation tests passed **16/16 in 0.765 s** and changed-document links passed.
Published current-head hosted checks and
an independently clean current Main remain mandatory before ordinary squash merge.
The separately tracked latent in-memory Euler preservation fix remains its own
placement/source-rebase dependency scope; no expanded latent-hint acceptance is
claimed by this canonical snapshot gate. Full Editor milestones remain **0/8**.

## Stable comparison and native Save Main integration

The same three own feature/evidence commits replay onto accepted Main
`89bb06ec2ff30f081702599f83107ed13d707e99`, including accepted stable UUID comparison
and native project-upgrade/Scene Preview input fixes. README and both Roadmaps retain
all Main and snapshot contracts. Replayed pre-evidence head is
`692a006dd1a2ce058b4ce9679056493a221a6423`; old current-head checks are historical.

Fresh complete graphical/Cryptography/Slang/Zig/Showcase/native ProjectPlayer configure
and build passed **356 steps**, followed by **242/242 in 593.90 s**, zero skips.
Minimal Monolithic Shipping configure/build passed **14 steps**, rebuilding Runtime
and AI dependencies with Editor excluded; this is minimal profile/link compatibility.
No product-code or assertion changes were needed for the replay. The separate latent
Euler preservation dependency remains open. Fresh published-head hosted CI and a clean
current-Main guard are required before ordinary squash merge. Full milestones remain **0/8**.
