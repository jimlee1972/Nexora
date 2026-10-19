# ED-M7 hierarchy phase diagnostics: Linux evidence

Date: 2026-10-19. Base: `a0ec95a9358e01870f671367cf88821ab4cdff79` (PR #649).
Beads: `nexora-0ua.1.15.1`; parent investigation `nexora-0ua.1.15` remains open.
This delivers execution diagnostics, not a timeout root cause, performance improvement or ED-M7
acceptance. Editor milestone acceptance remains **0/8**.

## Reason and scope

On PR #649's unchanged head `25e5645f2f4d7680a074e2b5ab1504c9714573cb`, the first Windows push
job [117857618367](https://github.com/jimlee1972/Nexora/actions/runs/39189020443/job/117857618367)
timed out `editor.deep_hierarchy` at 180.01 seconds. The same-head PR job
[117857707451](https://github.com/jimlee1972/Nexora/actions/runs/39189046902/job/117857707451)
passed it in 100.09 seconds, and unchanged push retry
[117867580043](https://github.com/jimlee1972/Nexora/actions/runs/39189020443/job/117867580043)
passed it in 65.53 seconds. This proves timing variance; it does not identify runner contention,
a slow operation or a production defect. The original failure is retained.

The existing executable now flushes phase starts and completion/exception elapsed milliseconds to
stderr. Each line identifies correctness/benchmark run, dataset, DPI and cycle (`-1` outside benchmark
cycles). Constructor subphases distinguish source generation, workspace creation, file write, scene
load and UI setup; outer phases distinguish settle/expansion, row checks, keyboard navigation,
collapse/filter/reload, source/history verification, measured samples, destruction and allocator checks.
Outer fixture-create includes member initialization; correctness fixture-destroy also includes
subsequent local-container destruction. Nested phases overlap: do not sum them as exclusive costs.

The logger uses a steady clock and exception unwinding to mark a failed active phase. An abort or
hard timeout cannot emit a completion; the last flushed start identifies active work rather than
its cause. It prints once per phase transition, never once per node or measured frame. Benchmark
stdout remains the existing schema-1 JSONL; no diagnostic formatting touches its stream. Phase
output sits outside each BeginFrame/DrawProductShell/EndFrame sample timer.

Only this test executable and documentation change. CTest registration, 180-second deep deadline,
300-second Linux/600-second Windows serial benchmark deadline and default timeout scale 1 are
unchanged. The 100,000-node fixtures, DPI cases, cycles, six warmup frames, twelve samples,
source/history assertions, clipping and zero-ImGui-retention checks remain intact. No product API,
ABI, persisted format, module graph or linkage contract changes.

## Local validation

Host: Linux x86_64, GCC 14.2, CMake 4.4.4, Ninja 1.13.2, clang-format 23.1.3. Development is
Modular with graphical Editor and 2D ON, Slang OFF; the existing external environment provides
X11/Vulkan development files and Clang 19. No local Xvfb/xdotool or physical GPU acceptance ran.

```bash
source /workspace/work/editor-roadmap/tools.env
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=4 ctest --preset linux-development
build/linux-development/Tests/EditorImGui/NexoraDeepHierarchyTests --benchmark-end \
  > /workspace/work/editor-roadmap/hierarchy-end.jsonl \
  2> /workspace/work/editor-roadmap/hierarchy-end.stderr
python3 Tools/Build/TestDocumentationCI.py
clang-format --dry-run --Werror Tests/EditorImGui/DeepHierarchyTests.cpp
git diff --check
```

Configure/build passed. Full CTest: **284 registered, 272 passed, 12 skipped, zero failures**, in
**305.84 seconds**. Skips are the twelve native Vulkan tests unavailable on this host; no skipped
test is counted as acceptance. `editor.hierarchy_scale_lifecycle` passed in **219.90 seconds** and
`editor.deep_hierarchy` in **68.71 seconds**, retaining their default deadlines. These are current
host observations, not before/after performance evidence. Shipping is not required for this
standalone test-only change; no linkage boundary changed.

A scratch parser checked all eight benchmark schema-1 records (two cycles, two datasets, 1x/2x),
unchanged 100k nodes/six warmups/twelve finite sorted samples, bounded nonzero widget counts and
zero retained ImGui bytes. It verified every flushed start has one finite nonnegative done record:
**96 benchmark phase pairs and 39 correctness/topology phase pairs** across the full gate.

The separate `--benchmark-end` run also passed all four bottom-viewport cases (two cycles,
1x/2x). A scratch parser verified **four pure stdout schema-1 JSONL records** and **52 balanced
stderr phase pairs**, including scroll-end warmup, unchanged samples and zero retained ImGui bytes.

Selected benchmark phase ranges across eight cases, in milliseconds:

| Phase | Minimum | Maximum |
| --- | ---: | ---: |
| Scene load | 2593.810 | 4064.950 |
| Expansion and six settle frames | 5746.550 | 9957.780 |
| Twelve frames and sample assertions | 12681.400 | 25048.400 |
| Fixture destruction | 9.097 | 19.040 |

These aggregate phase ranges include phase-boundary logging and surrounding assertions; they are
separate from the existing individual CPU-frame samples. They do not set production frame budgets.

Two controlled smoke runs used captured stdout/stderr and a task-owned temporary directory:

- A nonexistent `TMPDIR` made fixture construction throw. Exit status was 1, stdout was empty,
  and stderr retained `phase=fixture-create status=failed elapsed_ms=...` plus the original error.
- A Python subprocess deadline killed the ordinary executable after 0.5 seconds. Captured stderr
  ended with `run=correctness dataset=expanded-chain dpi=1 cycle=-1 phase=scene-load status=start`.
  No successful completion was invented. The task-owned scratch directory was removed afterwards.

Documentation routing tests passed **16/16**. Changed Markdown link/bilingual validation, touched-C++
formatting and diff checks passed. The root README remains unchanged and concise. Other roadmaps
were reviewed for affected delivered behavior; only the paired Editor supporting entry changes.

Source SHA-256 (`DeepHierarchyTests.cpp`):
`4dae06622feca6405ef8a73d2c213555282b9e849c4fd0180263620d03671025`.
The unchanged registration file SHA-256 (`Tests/EditorImGui/CMakeLists.txt`) is
`816ad4f726c3c4b11d05eedb7a9ff870b0b1a26e15464b8d07b17129260080dc`.

Windows/macOS were not executed locally. Hosted source-head CI results belong in the PR and Beads;
the parent Windows timing investigation remains open even if those runs pass. Production CPU/frame,
process/GPU memory, long-duration soak and physical-host acceptance remain open.
