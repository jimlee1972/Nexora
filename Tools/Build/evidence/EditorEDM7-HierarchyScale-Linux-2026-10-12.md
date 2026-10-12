# ED-M7 hierarchy CPU scale and lifecycle measurements: Linux evidence

Date: 2026-10-12. Beads: `nexora-0ua.1.6`. Base: accepted main
`32043f59`. This supporting gate does not accept ED-M7, production frame/memory budgets,
native GPU/asset scale, long-duration soak or physical-host workflows.

Host: Linux 6.18.44 x86_64, AMD EPYC 9V74, five available CPUs, GCC 14.2.0,
CMake 4.4.4, Ninja 1.13.2, Slang 2026.18 and clang-format 23.1.3. Development is Modular,
graphical shell and Slang ON, Zig OFF. The preset's Development C++ optimization flags are
empty; these are unoptimized CPU observations, not Shipping performance projections.
Native integration tests use local Xvfb and Mesa lavapipe 25.0.7 separately from this benchmark.

## Measured scope and reproducibility

`NexoraDeepHierarchyTests --benchmark` creates fresh project/World/SceneDocument/ImGui owners
for flat roots and an expanded 99,998-node chain plus two root siblings, each totaling 100,000
nodes. Two cycles cover 1x/2x framebuffer DPI with the same 1280x900 logical viewport;
explicit assertions protect that metadata. Each case settles six frames and times twelve
complete BeginFrame/DrawProductShell/EndFrame CPU draws with a steady clock. Fixture construction,
load, source-preservation checks and teardown are outside the frame timer, but inside the
300-second CTest deadline. The test uses the 3D-preview layout without native GPU submission.

Every measured frame requires all 100k logical hierarchy rows, positive real ImGui geometry and
fewer than 128 submitted hierarchy widgets. Every case preserves exact saved bytes, clean state
and empty Undo/Redo. An aligned malloc-backed ImGui allocator counts payload bytes and live
allocations; every teardown requires zero bytes/count and no current context. This counter excludes
allocation headers, World/document/STL memory, process RSS and GPU memory. It cannot prove those
owners leak-free. The existing independent context-lifetime and overview-pose gates remain intact.

The [raw schema-1 JSON lines](EditorEDM7-HierarchyScale-Linux-2026-10-12.jsonl) retain all twelve
sorted frame samples per case. Percentiles use nearest rank, `ceil(fraction * count) - 1`;
with twelve samples p95 equals max. CTest's serial property excludes concurrent tests from the
measurement. No separate build or benchmark ran concurrently with the accepted gate.

| Cycle | Dataset | DPI | p50 ms | p95/max ms | ImGui peak payload bytes |
| --- | --- | --- | --- | --- | --- |
| 0 | Flat roots | 1x | 870.032 | 910.548 | 321789 |
| 0 | Flat roots | 2x | 878.986 | 930.141 | 420093 |
| 0 | Expanded chain | 1x | 1086.521 | 1116.706 | 302787 |
| 0 | Expanded chain | 2x | 1091.773 | 1100.744 | 401091 |
| 1 | Flat roots | 1x | 886.946 | 978.242 | 321789 |
| 1 | Flat roots | 2x | 862.544 | 1097.649 | 420093 |
| 1 | Expanded chain | 1x | 1104.696 | 1278.092 | 302787 |
| 1 | Expanded chain | 2x | 1111.918 | 1214.267 | 401091 |

All eight cases submitted at most 44 hierarchy widgets. Flat cases produced at most 4256 vertices,
deep cases 3724. All eight teardowns retained zero ImGui payload bytes/allocations. The gate passed
in **170.47 seconds**; the unchanged deep hierarchy correctness gate passed in **52.22 seconds**.
These roughly one-second CPU frames are a baseline for further profiling, not acceptable
production interactivity or evidence of a production frame-time budget.

Source SHA-256:

- `Tests/EditorImGui/DeepHierarchyTests.cpp`:
  `9bc81dd6147d903fd74485614ba8b49ed1902f43ebab79bdeeb91a03537d2c5c`
- `Tests/EditorImGui/CMakeLists.txt`:
  `53b41060a5fc13d5cebb474542cdf5d82a2bbd4e2321f1936c22b53db28e3d9d`

## Validation

The external task environment supplies local CMake/Ninja/Clang, X11 headers, Vulkan loader,
Xvfb/xdotool, Slang and lavapipe paths. Initial configure enabled graphical shell and Slang;
machine-specific paths and generated build output are not committed.

```bash
source /tmp/nexora-editor-20261012/env
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development
python3 Tools/Build/TestDocumentationCI.py
clang-format --dry-run --Werror Tests/EditorImGui/DeepHierarchyTests.cpp
git diff --check
```

The final configure and full build passed. Full Development passed **226/226**, zero failures or
skips, in **687.71 seconds**, including the benchmark and existing hierarchy results above and
the Linux Vulkan/Xvfb workflows. Documentation tests passed **16/16**; all four changed Markdown
documents passed link/bilingual validation; formatting and diff checks passed. Linux Shipping is not required:
only a test target links the already-existing Dear ImGui library; product linkage, ABI, ownership,
persisted formats and runtime source remain unchanged. Windows/macOS were not executed locally.
