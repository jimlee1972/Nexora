# Deep graphical hierarchy — Linux cloud evidence, 2026-10-09

Actual ImGui hierarchy traversal uses an explicit work vector instead of native recursion.
Reversed pushes retain the existing parent-first and sibling order. Expansion pruning indexes
the current generation keys once; name-only saved-state metadata avoids repeated World lookup.
No authoring snapshot cache or persistent entity/name pointer is introduced.

## Executed acceptance

A real Editor-scene file contains 100,000 tracked nodes. The deep case expands a 99,998-node
chain with two additional root siblings at both 1x and 2x. A separate flat case has 100,000 roots.
The test verifies exact IDs, generations, depths, child states, full visible-row count, clipped
widget submission, actual Home/End selection/reveal, collapse/re-expand, filter and replacement.
Owned row observations retain their original generation while stale expansion/request keys prune.
Source bytes, saved baseline and empty Undo/Redo history remain unchanged.

- Focused deep/flat ImGui acceptance passed 1/1 in 53.36 seconds.
- On accepted plugin-lifecycle main d202bb15, complete graphical Development passed 209/209,
  zero skips, in 463.10 seconds. Minimal Shipping built all 74 steps.
- Full configuration enabled graphical shell, Slang, Zig gameplay, Showcase, project Player and
  native presentation. Existing hierarchy keyboard and Select All controls remain in the gate.

The fixture selects the production 3D-preview layout and explicitly focuses the real Hierarchy
tab after initial docking. It does not submit native GPU geometry. An initial top-down attempt
exposed separate repeated parent-chain work in overview markers; that is tracked and tested by
the bulk-world-pose change. The initial unfocused-tab fixture was corrected to use the existing
settled-layout/focus convention; row assertions were retained.

## Scope

These are deep-tree correctness and representative cloud execution results. Complete frame/memory
budgets, production asset scale, long lifecycle soak and physical-display acceptance remain open.
The Linux cloud does not substitute for hosted Windows/macOS or physical device execution.

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
