# ED-M1 strict descendant folder queries

Date: 2026-10-20 (Asia/Taipei). Task: `nexora-xke.5`.
Prepared separately from parent PR653 on tree `932f3ddf3ad54cb2c75e329ed2e6ae97c09e89a3`,
which includes main `bfc1111d4c80a65a6dc1e6a13ffbc5eef041441f`. PR653 subsequently merged as
`e7d9f3d790cf69a0ca1e2aa34ad9326c7300e7ff` after all 38 latest-head checks passed. Its squash
tree equals the validated parent tree. Rebased only this follow-up onto that latest main;
the resulting complete tree exactly equals pre-rebase commit
`ffa87f8fc1d1db2b73f90d6687a0e1c69d526d53`. Only this publication evidence was then updated;
the existing full gate therefore validates unchanged code and tests. The PR diff contains only
this independent eight-file follow-up. Editor milestone acceptance remains 0/8.

## Behavior and compatibility

`lexically_relative` returns a leading `..` for items outside the current folder. Previously,
ChildFolders used that component as a directory name, synthesizing a parent row from sibling or
ancestor assets. The query now rejects that component before deriving its immediate directory.
Current-folder files remain asset rows, true children are sorted/deduplicated in UTF-8 order,
and folder results remain independent of asset query/type filters. The generic model still
supports a scoped query at another valid relative root; this is component ancestry filtering.

The query returns owning values and does not mutate item/selection/filter/breadcrumb state,
revision or pending Undo. Graphical parent navigation remains explicit breadcrumbs and Alt+Up;
existing typed drag/drop, UUID/generation admission and read-only policy are unchanged. No
public signature, persisted format, stable C/Zig ABI, module dependency or linkage boundary changes.
The Editor and EditorImGui contracts and EN/zh-TW roadmap entries are synchronized; the root
README retains its concise unchanged milestone summary. Physical workflow acceptance remains open.

## Regression evidence

Portable coverage includes two real children (one UTF-8/marker name), duplicate child assets,
current-folder files, sibling and component-prefix siblings, unrelated relative roots, leaf and
missing-folder queries. It preserves both filters, selected UUID, item count, revision and one
pending Rename Undo. The actual ImGui scenario traverses marker siblings/nested leaves by pointer
and Tab/Enter at 1x/2x in writer and read-only sessions, verifies no synthesized parent-row target,
and still clicks owning parent breadcrumbs while preserving exact source/sidecar snapshots.

Before the implementation change, the new portable and graphical assertions both failed on the
parent code (0.15 s total): parent/sibling paths were present and the nested view exposed a parent
row. After rejecting the leading parent component, the unchanged assertions passed 2/2 in 1.44 s
(portable 0.05 s; graphical 1.38 s). No assertion, navigation route or deadline was removed.

The independent worktree enables Editor graphical shell and 2D, disables Slang, and reuses the
existing pinned Vulkan-Headers v1.3.296 and Dear ImGui v1.91.9b-docking source caches through local
FetchContent overrides. No machine-local preset or embedded Beads database is committed.

## Full local validation

Host: Linux 6.18.55 x86_64, GCC 14.2, CMake 4.4.4 and Ninja 1.13.2. The independent worktree
was configured without a machine-local preset using:

```bash
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_SLANG=OFF -DFETCHCONTENT_SOURCE_DIR_NEXORA_VULKAN_HEADERS=/workspace/Nexora/build/linux-development/_deps/nexora_vulkan_headers-src -DFETCHCONTENT_SOURCE_DIR_NEXORA_IMGUI=/workspace/Nexora/build/linux-development/_deps/nexora_imgui-src
cmake --preset linux-development
CMAKE_BUILD_PARALLEL_LEVEL=4 cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=4 ctest --preset linux-development --output-on-failure
```

Full gate: 285 registered, 273 passed, 12 native Vulkan environment skips, zero failures, 341.06 s.
Portable preview contract passed 0.06 s; actual folder/dependency controls 1.48 s; Content shortcuts
25.62 s; deep hierarchy 82.93 s; lifecycle 224.34 s. No compilation ran concurrently with CTest and
all committed deadlines remained unchanged. The skipped native GPU/display/backend cases do not
certify physical acceptance. No linkage boundary changed, so a Shipping gate is not required.

Documentation tests passed 16/16; changed-document routing/links, touched C++ formatting and
`git diff --check` passed. The root README remains unchanged.

| Retained log | SHA-256 |
| --- | --- |
| `descendant-red.log` | `ca42f3f3ab331d7b3046e7dfa7b906fa0b98ce1e7d5ade9b2c3f1a410b958d77` |
| `descendant-focused.log` | `1b0ae6fdfe49e57a460234876b9f5dfc7f40300a1e70959c5ca98507b7dfd57f` |
| `descendant-full-configure.log` | `459121a6276a138c21d3ae751c2fbf2546667fe23b1e4769e6d3e4f344f93b12` |
| `descendant-full-build.log` | `5c85cd30a5ee07098dc452feb1d057bd577f4e31ef1537586e02d7a01fc0dec8` |
| `descendant-full-ctest.log` | `65b25fbd71aadeb0a0ae667947ad1f286e790c59c37b00aebbe38a4a8c167532` |
