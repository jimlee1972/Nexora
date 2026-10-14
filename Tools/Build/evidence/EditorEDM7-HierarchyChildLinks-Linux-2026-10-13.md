# ED-M7 contiguous hierarchy child links — 2026-10-13

Beads: `nexora-0ua.1.9`. Base: merged PR588/main `f4b2e82a`.
This supporting cloud slice does not complete ED-M7 or physical 0/8 acceptance.
[Raw samples and host metadata](EditorEDM7-HierarchyChildLinks-Linux-2026-10-13.json).

## Change and contract

Unfiltered `BuildHierarchyRows` previously constructed an ID hash map and a second
parent-to-child-vector hash map on every call. The latter required separate hash entries and
child-vector allocations for groups. A single current ID-to-snapshot-slot lookup now resolves
parents into a contiguous first-child/next-sibling index array. Reverse input prepending
preserves exact Runtime root/sibling order, even for children stored before parents. Missing
metadata parents still produce roots. Iterative index work visits expanded descendants without
native recursion and reports the same depth/child flags. Filtering is unchanged and returns
flat rows in snapshot order.

Every lookup, link and traversal stack is call-local. These work buffers retain no topology
cache, document/name/World borrow or native resource after return. Returned row pointers still borrow the
caller-owned Nodes snapshot, whose names retain their existing document string-view lifetime.
Serialized owner-thread rendering and failure behavior remain unchanged. This removes the
parent-group hash map and separate child vectors by construction, not by a measured total
allocator counter. The remaining lookup/expanded maps, snapshots, pruning and other shell work
still scale with input. Public layouts, stable C/Zig/wire formats, module dependencies and
Shipping boundaries are unchanged.

## Measurements

Same VM: Linux6.18.44, AMD EPYC9V74, 5 visible logical CPUs, 4 CPU equivalents by cgroup quota;
GCC14.2.0 Development with graphical Editor/SDK/Slang/Vulkan enabled and headless off.
The unchanged real100k shell benchmark uses 1280x900 logical dimensions, 1x/2x DPI, two fresh
project/World/document/ImGui host cycles, six warmup frames and twelve measured CPU draws per
case. All eight before and after cases pass original logical-row/clipping/source/history and
zero retained ImGui payload checks. The expanded chain has 99,998 nodes plus two root siblings.
Loading, native GPU submission/presentation and World/document/STL/process memory are excluded.
ImGui payload counters measure only that allocator, not the removed STL group allocations.

| Dataset | Cycle | DPI | Before p50 ms | After p50 ms | Before p95 ms | After p95 ms |
| --- | --- | --- | --- | --- | --- | --- |
| flat-roots | 0 | 1 | 857.110 | 868.615 | 906.677 | 895.129 |
| flat-roots | 0 | 2 | 885.077 | 837.947 | 925.920 | 908.595 |
| expanded-chain | 0 | 1 | 1068.049 | 949.780 | 1099.277 | 1016.039 |
| expanded-chain | 0 | 2 | 1066.370 | 945.570 | 1100.049 | 1021.518 |
| flat-roots | 1 | 1 | 859.967 | 824.220 | 909.922 | 870.926 |
| flat-roots | 1 | 2 | 865.772 | 897.090 | 913.008 | 997.941 |
| expanded-chain | 1 | 1 | 1093.687 | 928.244 | 1286.517 | 950.426 |
| expanded-chain | 1 | 2 | 1111.564 | 940.593 | 1223.368 | 967.654 |

All four deep-chain p50/p95 observations are lower, while flat-root cycle0/1x p50 and
cycle1/2x p50/p95 are higher. These same-host samples do not establish a universal frame-time
improvement, calibrated production budget, memory leak proof outside ImGui or physical soak.

## Validation

- New mixed-root/sibling/grandchild/orphan/reorder/reparent/Undo/filter/copied-row checks passed
  the old grouping implementation before replacement. The candidate additionally passes reversed
  metadata and child-before-parent input. Existing 100k deep/flat assertions remain intact.
- `editor.deep_hierarchy` and `editor.hierarchy_select_all`: 2/2,48.58s (deep48.35s).
  `editor.hierarchy_keyboard_navigation`: 1/1,1.24s.
- Controlled forward-prepend fault reversed sibling order; `--topology` failed with
  `mixed hierarchy root/sibling order, depth or key changed`. Correct source restored and rebuilt;
  topology/navigation/after benchmark passed before the final gate.
- An initial full CTest attempt lacked socket capability and an active display. Xvfb could
  not bind local sockets, so native tests skipped or failed with `Xvfb failed`; that attempt
  was interrupted and is not delivery proof. The same unchanged source is validated again
  with the supported socket grant, a verified Xvfb display and retained `-noreset` server.
- `cmake --preset linux-development`, `cmake --build --preset linux-development`,
  `ctest --preset linux-development --output-on-failure`: passed: initial build241 actions, final configure/build repeated,255/255 tests, no failures/skips,805.83s; deep50.40s,100k lifecycle157.02s,15 display tests227.44s.
- Changed C++ clang-format23/diff check; documentation, EN/zh-TW, relative links and raw sample
  checks: passed: clang-format23, diff,16 documentation tests, EN/zh-TW entry parity, relative links and all16 raw sample records. Root README milestone summary remains unchanged.

Final-head hosted CI is a separate delivery requirement. Full production frame/memory/GPU,
asset scale, long-duration soak and physical-host acceptance remain open.
