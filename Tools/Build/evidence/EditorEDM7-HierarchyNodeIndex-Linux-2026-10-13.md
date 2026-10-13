# ED-M7 validated hierarchy metadata-slot reuse — 2026-10-13

Beads: `nexora-0ua.1.8`. Base: merged PR587/main7dd5e704. Supporting cloud allocation/correctness
slice only; full ED-M7, production frame/process-memory/native GPU/soak budgets and physical0/8
milestone acceptance remain open. [Raw scoped samples/host metadata](EditorEDM7-HierarchyNodeIndex-Linux-2026-10-13.json).

## Change and contract

SceneDocument::Nodes previously constructed a fresh per-node hash map on every snapshot. It now
retains only owned IDs and metadata slot indices, validates size and every current ID/slot before
reuse, then reads current metadata names/generations and current Runtime parent/storage order.
Same-count replacement/permutation is not a valid cache key on its own. Invalid indices rebuild a
fresh staged map and swap, releasing stale keys/capacity; allocation failure leaves document/World
unchanged. Cache storage scales with the last validated metadata input until the next rebuild.
No node/World/native pointer or returned view is cached. Snapshot vectors/scalars are copied,
while NodeView::name keeps its existing document string-view borrow and must not survive relevant
document mutation/destruction. Calls are serialized on the authoring owner thread. Name/identity/
Runtime changes, untracked entities, Undo/Redo/new/reload semantics and baseline/source remain
unchanged. Validation/copying and other hierarchy/frame work remain O(n); this is not full
constant-work virtualization or a production budget claim. Private SceneDocument layout grows,
so C++ consumers must rebuild; stable C/Zig, scene formats and module dependencies are unchanged.

## Scoped measurements

Same VM: Linux6.18.44, AMD EPYC9V74,5 visible CPUs, cgroup quota4 CPU equivalents; GCC14.2.0,
Development, graphical Editor/Slang/Vulkan enabled. This CPU measurement excludes GPU/presentation.
Nodes-only fixture:100k flat nodes,5 warmup calls,31 snapshots; load/baseline validation excluded.

| Nodes-only metric | Before ms | After ms |
| --- | ---: | ---: |
| p50 | 33.746 | 22.044 |
| p95 | 35.321 | 30.346 |
| max | 35.989 | 35.601 |

Existing real100k ImGui shell benchmark:1280x900 logical pixels,1x/2x framebuffer DPI,
flat roots/expanded99,998-node chain,2 fresh lifecycle cycles,6 warmup draws and12 timed frames.
All before/after cases kept100k logical rows with<128 hierarchy widgets and unchanged source/
baseline/Undo/Redo; every teardown reported zero ImGui retained allocation payload. This counter
excludes cache/World/STL/process memory. One flat-root p95 is worse after; results are observations
on this VM, not universal improvement or a frame-time acceptance threshold.

| Dataset | Cycle | DPI | Before p50 ms | After p50 ms | Before p95 ms | After p95 ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| flat-roots | 0 | 1 | 912.436 | 843.427 | 981.637 | 873.960 |
| flat-roots | 0 | 2 | 878.796 | 878.659 | 907.878 | 985.177 |
| expanded-chain | 0 | 1 | 1103.897 | 1071.016 | 1155.678 | 1107.821 |
| expanded-chain | 0 | 2 | 1145.948 | 1078.209 | 1219.984 | 1128.045 |
| flat-roots | 1 | 1 | 896.920 | 864.365 | 912.417 | 907.176 |
| flat-roots | 1 | 2 | 885.890 | 858.032 | 938.766 | 911.721 |
| expanded-chain | 1 | 1 | 1098.622 | 1068.440 | 1162.112 | 1159.070 |
| expanded-chain | 1 | 2 | 1112.225 | 1066.047 | 1335.997 | 1102.877 |

## Verification

- Core snapshot correctness passed on old/new implementations: fresh long names, Runtime order/
  parent, same-count deleted/replaced IDs, Undo/Redo, untracked Runtime entities, new scene,
  independently permuted metadata/Runtime same-count reload, generation and failed reload retention.
- Controlled count-only index reuse fault caused `snapshot cardinality changed`; full ID/slot
  validation was restored. Fault binaries/results are not delivery proof.
- Existing before/after100k ImGui scale/lifecycle benchmark:8 cases each passed with zero retained
  ImGui payload and no weakened widget/source/history assertions.
- `cmake --preset linux-development` and `cmake --build --preset linux-development`: passed353
  actions after targeted preparation, graphical Editor/SDK/Slang enabled, headless off.
- `ctest --preset linux-development --output-on-failure`:255/255 passed, no failures/skips,817.12s;
  snapshot semantics0.01s, deep hierarchy51.42s and100k scale/lifecycle167.75s. Environment tool
  handles were reset during the run; source/logs remained intact and the existing full gate
  progressed to its complete255-test successful summary without a duplicate run.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed5 actions,
  Minimal Monolithic/native Vulkan, Editor disabled.
- Changed C++ format23, DocumentationCI16, EN-ZH/relative links/rawJSON and unchanged concise root
  README/diff checks passed. Final-head hosted checks remain required before merge.
