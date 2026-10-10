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
