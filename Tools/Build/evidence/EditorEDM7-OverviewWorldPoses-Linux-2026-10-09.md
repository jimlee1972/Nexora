# Owning scene world poses — Linux cloud evidence, 2026-10-09

Runtime resolves a complete current scene in an owning, storage-order pose/matrix vector using
indexed iterative ancestry. Each entity is visited once with expected O(n) work/storage, even
for reversed storage and deep chains. Exact affine origins/matrices retain the existing scalar
rotation/component-scale approximation under shear. Invalid transforms, non-finite composition,
duplicate/zero IDs, cycles, missing parents and unavailable lifecycle states reject the whole
observation. Returned storage retains no World borrow or persistent cache.

SceneDocument filters one owning observation to tracked nodes while retaining untracked-parent
influence. Top-down markers, multi-selection/whole-scene framing and prospective drag ancestry
avoid repeated parent-chain lookup. Borrowed pose pointers exist only within that draw call.
The accompanying name-only saved-state fast path preserves the serializable signature fields.

## Executed acceptance

- Runtime compares scalar TRS/matrix results on reversed, mirrored, rotated and sheared small
  hierarchies; it checks every result in actual 100,000-node forward/reversed deep and flat scenes.
  Missing/unloading/unloaded, invalid IDs/transforms, dangling/cyclic parents and affine overflow
  reject without mutating input. Empty live scenes and owning observations are verified.
- Real ImGui top-down fixtures load 100,000 tracked nodes, execute settled draws at 1x and 2x,
  observe the exact final leaf origin and distant-root culling, replace the document, and verify
  stale marker pruning, retained observations, original bytes and empty authoring history.
  The untracked-parent case verifies that only tracked children enter the document observation.
  The fixtures resolve all nodes; they do not submit 100,000 overlapping label widgets or GPU meshes.
- Initial focused Runtime/ImGui acceptance passed 2/2 in 38.14 seconds. On accepted CI-cleanup
  main 9b25a515, the complete 403-step graphical/native build passed. Final full confirmation
  passed 213/213 with zero skips in 449.27 seconds; its new Runtime and ImGui cases passed in
  1.48 and 36.39 seconds respectively. Minimal Shipping built all 74 steps; final confirmation
  was cached. Graphical shell, Slang, Zig gameplay, Showcase, project Player and native
  presentation were enabled.

The first full integration passed 211/213 in 439.76 seconds. Existing native Preview scale-Undo
and Center plane-Undo readbacks failed. Their unchanged isolated diagnostic passed 2/2 in
51.25 seconds, followed by the complete successful confirmation. No assertions or deadlines were
relaxed. Original failure/diagnostic/confirmation logs remain in the execution workspace. This
does not claim to eliminate every native synthetic-input timing failure.

~~~sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_BUILD_SHOWCASE=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON \
  -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
~~~

## Final accepted deep-hierarchy integration

Rebased onto accepted main `ceb409da50a4900c9fedfc476db33f18ed03600a`, retaining both deep
hierarchy and overview registration, fixtures and bilingual entries. The 305-step incremental
graphical/native build passed. The complete gate passed **214/214**, zero skips, **497.18s**;
minimal Shipping passed its **14-step** incremental build. Exact commands and options above were
repeated. This final result includes the existing native scene-file, preview, center and authored
mesh acceptance without helper changes or relaxed assertions/deadlines in this branch.
Fresh hosted checks for the final head remain required before merge.

This rebuild-required public C++ API preserves scene serialization and stable C/Zig gameplay ABI.
Complete frame/memory budgets, production asset scale, lifecycle soak and physical-display
acceptance remain open. Linux cloud execution does not certify other hosted/physical platforms.
Full Editor milestone completion remains 0/8.
