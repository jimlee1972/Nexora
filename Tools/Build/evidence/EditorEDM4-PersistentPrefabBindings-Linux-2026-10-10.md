# Persistent prefab placement bindings — Linux, 2026-10-10

Beads `nexora-pmb.1.18`; review-only foundation `e4bd1345` combines nested
materialization PR502 and atomic property snapshots PR491. Never merge the
foundation; retarget only the feature after prerequisites and require fresh CI.

Graphical Linux Development with cryptography/Slang/Zig/showcase/native Player,
pinned ImGui and Vulkan overrides, four managed CPUs.

- Initial complete build:316 steps passed.
- Expanded test build:105 steps passed.
- Focused bindings/atomic properties/assets/forest/prepared save/parser gates:
  **6/6 passed,13.78s**; bound/repeated placement fixture **7.95s**.
- Final focused gates after bound-forest rejection and remapped-ID byte-budget
  guards: **6/6 passed,13.47s**.
- Final full Development configure/build and CTest: **242/242 passed,607.65s**,
  zero failures or skips. Shipping configure/build passed (no pending work).
- Actual nested graph materialization retains two independent scoped revisions
  of one source; bound and detached variants run the same hierarchy/TRS/Euler720/
  unknown payload/stable identity/one Undo/Redo/clipboard acceptance.
- Bindings survive actual SceneFile Save As, coordinated Save All, clean reopen,
  property replacement, ordinary rename and deletion/Undo/Redo. Runtime capture
  excludes Editor records; bound-scene asset capture and property-only changes
  to source context reject. NewScene clears and reopen restores metadata.
- Corrupt legacy header/duplicate placement/duplicate mapping/nil UUID/negative
  revision/dangling target reloads reject without changing content/baseline.
- 128 actual six-node placements succeed;129 rejects while complete history works.
- Ordinary forest import rejects bound sources without changing the target;
  bound instantiation stages the metadata and remapped-ID budget before import.

Scene format4 applies only to nonempty Editor bindings; old formats remain
accepted and Runtime schema/C ABI/module graph stay unchanged. Structural deletion
explicitly detaches the affected entire placement; Paste/Duplicate are detached.
No power-loss journal, history-memory quota, graphical instantiate or automatic
source reconciliation is claimed. Full Editor milestones remain **0/8**.

## Windows Save All fixture reader lifetime

The inherited bound-scene fixture copied saved bytes but retained its input file
handle across the later coordinated atomic replacement. Hosted Windows desktop and
mimalloc runs of the expanded prefab integration exposed this as a Save All failure;
Linux permits replacement while the reader remains open.

The fixture now closes the reader immediately after copying the owning saved bytes.
Preparation, publication, saved baseline, placement metadata, Undo and exact original
bytes retain all previous assertions, now with separate failure messages. Product
atomic publication and rejection of actual busy readers are unchanged. Beads
`nexora-pmb.2.8` tracks the cross-platform verification. Previous full-suite timings
above describe the earlier feature head; fresh corrected-head validation is required.

Corrected-head full graphical Development build passed **316 steps** and
**242/242 tests in 600.40 s**, zero skips. Bound nested materialization/Save All
passed in **7.76 s**, native project upgrade in **11.88 s**, and native Scene
Preview in **28.06 s**. Minimal Monolithic Shipping passed **5 steps**.
This validates the owning persistent-bindings composition, with its existing
review-only prerequisite foundation; fresh Main replay and hosted Windows checks
remain necessary before merge. No new GUI/input feature is claimed by this fix.
