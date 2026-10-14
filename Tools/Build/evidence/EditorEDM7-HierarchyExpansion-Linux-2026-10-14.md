# ED-M7 scoped hierarchy expansion decisions — 2026-10-14

Beads: `nexora-0ua.1.10`. Base: merged PR589/main `551d5eec`.
Supporting cloud slice only; full ED-M7 and physical 0/8 acceptance remain open.
[Raw before/after samples, host metadata and candidate C++ hashes](EditorEDM7-HierarchyExpansion-Linux-2026-10-14.json).

## Change and contract

Every submitted hierarchy widget previously searched the complete expanded-key vector again.
Projection also allocated a separate expanded-ID hash set despite already having a current
ID-to-snapshot-slot lookup. Current full entity/document NodeKeys now resolve through that lookup
into call-local link flags; projected rows copy the decision and drawing reuses it. Stale or
foreign scope keys cannot mark a slot. Keyboard expansion changes rebuild rows before clipping;
actual arrow collapse keeps a guarded event-time vector erase lookup. Filtered rows remain flat,
and read-only view expansion never grants source-write authority.

Link flags are call-local; copied decisions and pointers belong to the returned frame-local rows.
No persistent projection cache or additional name/World/native borrow is added. Authoritative
expansion-key view state and existing ImGui storage keep their existing lifetimes. Returned row
pointers retain the existing caller-snapshot/name borrow lifetime. Lookup/link arrays,
pruning, selection and other shell work still scale with input. Removing the expanded-key hash set
eliminates those separate hash-node allocations by construction, while each link gains a flag;
this is not a measured total STL/process memory reduction. Private test-access methods observe
actual rendered branch ImGui storage and root-arrow positions without new runtime telemetry.
Matching test consumers rebuild; public host layout, stable C/Gameplay ABI, module dependencies,
project/scene formats and Shipping boundaries are unchanged.

## Independent UI and fault evidence

New `NexoraHierarchyNavigationTests --expansion` cases pass the old renderer and candidate at
1x/2x with Ctrl/macOS behavior settings. They observe actual widget storage, not a recomputed
row projection. Cases cover descendant expansion, hidden-row absence, stale entity/document
keys, valid keys among stale keys, same-ID reload, keyboard Right/Left, actual mouse arrows,
read-only access, detach/reattach and unchanged saved source/empty history. Distinct arrow clicks
are separated by25 mock frames so they do not intentionally trigger Hierarchy Rename.

A controlled fault forced the rendered expansion to false while logical rows still traversed
expanded descendants. The new test failed with `actual rendered branch expansion disagrees with
current scoped state`. Correct source was restored/rebuilt; UI cases and both benchmark paths
passed afterward. No assertion or deadline was weakened.

## Scoped CPU measurements

Same VM: Linux6.18.44, AMD EPYC9V74,5 visible CPUs/4 CPU quota; GCC14.2.0 Development,
graphical Editor/SDK/Slang/Vulkan enabled, headless off. Original eight flat/deep start-view cases
use1280x900 logical dimensions,1x/2x DPI, two fresh project/World/document/host cycles, six warmups
and twelve measured CPU draws each. All original100k-row/clipping/source/history/zero-retained-
ImGui assertions remain intact.

The optional `--benchmark-end` adds four deep-chain end-view cases. Initial six settlement frames
establish scroll range, followed by six post-scroll warmups and twelve samples. Selection stays
empty and the final root must be submitted. The identical diagnostic fixture runs against actual
baseline551d renderer and restored candidate. Standard CTest case count/deadlines are unchanged.
This end viewport exercises near-tail expanded-key searches that start-view samples can miss.
All twelve before and after cases pass with fewer than128 submitted widgets and zero retained
ImGui payload after every teardown. Loading/native GPU/presentation and total memory are excluded.

| Dataset | Cycle | DPI | Before p50 ms | After p50 ms | Before p95 ms | After p95 ms |
| --- | --- | --- | --- | --- | --- | --- |
| flat-roots | 0 | 1 | 836.921 | 823.938 | 934.390 | 887.535 |
| flat-roots | 0 | 2 | 844.432 | 849.870 | 921.877 | 881.699 |
| expanded-chain | 0 | 1 | 930.841 | 918.884 | 964.042 | 941.620 |
| expanded-chain | 0 | 2 | 952.008 | 949.776 | 1059.940 | 1017.457 |
| flat-roots | 1 | 1 | 878.576 | 847.301 | 970.824 | 871.506 |
| flat-roots | 1 | 2 | 862.729 | 833.808 | 949.882 | 918.083 |
| expanded-chain | 1 | 1 | 932.711 | 924.593 | 1234.294 | 1009.126 |
| expanded-chain | 1 | 2 | 946.893 | 933.753 | 999.430 | 993.287 |
| expanded-chain-end | 0 | 1 | 1074.353 | 882.216 | 1105.448 | 914.383 |
| expanded-chain-end | 0 | 2 | 1059.158 | 928.702 | 1187.814 | 1026.082 |
| expanded-chain-end | 1 | 1 | 1031.077 | 931.525 | 1217.496 | 984.048 |
| expanded-chain-end | 1 | 2 | 1047.315 | 942.101 | 1102.885 | 1004.985 |

All twelve p95 observations and eleven p50 observations are lower; start-view flat cycle0/2x p50
is higher. End-view p50 is1031–1074ms before and882–942ms after. These are same-host observations,
not a universal improvement or production frame-time budget. ImGui payload excludes World,
document, STL and process memory; no native GPU calibration or physical/long-duration soak follows.

## Final validation

- Targeted existing deep hierarchy, SelectAll and keyboard navigation:3/3,50.49s;
  deep48.00s and navigation2.27s. Four new UI behavior/DPI combinations pass old/new/restored code.
- Final `cmake --preset linux-development`, `cmake --build --preset linux-development`,
  `ctest --preset linux-development --output-on-failure`: passed294 build actions,255/255 tests, no failures/skips,803.73s; deep46.11s,100k lifecycle155.73s,15display tests227.44s.
  The test process uses the verified Xvfb display and supported socket grant.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed5 build actions; Monolithic/native Vulkan with Editor/ImGui/SDK disabled.
- Four changed C++ files clang-format23; docs/EN-ZH/links/raw24records/diff/rootREADME:
  passed: clang-format23,16 documentation tests, EN/zh-TW entries, relative links,24 raw records and unchanged concise root README.

Final-head hosted CI is required before merge. Full production scale/frame/memory/GPU/soak and
physical-host acceptance remain open.
