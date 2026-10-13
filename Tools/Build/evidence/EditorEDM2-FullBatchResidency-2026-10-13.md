# ED-M2 full-batch immutable mesh residency

Beads: `nexora-owg.1.8`. Contract:
[ADR-0008](../../../Roadmap/en/ADR-0008-Editor-Mesh-Geometry-Residency.md).

The valid 4,096-batch submission previously fell back entirely above 64 batches, even when every
batch reused one unchanged mesh. Private slot bindings now cover 4,096 entries (4 KiB) while a fixed
64-range memo bounds native admission/key construction. Each batch scans at most 64 memo records;
duplicate successes/failures resolve throughout the submission, with individual legacy fallback
for further distinct ranges. GPU cache limits, current-draw leases and completion retirement do not
change. No public API/ABI, asset format, shader input or module dependency change.

Portable contracts prove 4,096 identical ranges use one native admission, 65 distinct ranges invoke
at most 64 admissions, and memoized success/failure remains valid after a further range falls back.
Legacy draws reset every binding. The Vulkan tracing/fresh-marker pixel fixture submits all 4,096
batches with clipped repeated instances, checks fresh visible transforms/materials, unchanged mesh
uploads/native ownership and no combined-prefix uploads. Shared PBR and Metal native pixel fixtures
submit more than 64 opaque repeated batches and require real cache hits while retaining visual
guards. Hosted Windows/macOS execution must be confirmed from job logs.

Linux cloud results:

- `cmake --preset linux-development`: passed.
- `cmake --build --preset linux-development`: passed, 437 actions.
- `ctest --preset linux-development --output-on-failure`: 248/248 passed, no skips, 851.65 seconds.
  Portable full-binding contracts, real Vulkan 4,096-batch tracing/pixels and Editor fixtures ran.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed, 9 actions.
- Targeted portable/Vulkan tests: 3/3 passed (9.68 seconds).
- Changed C++ format, `git diff --check`, Markdown/link/bilingual checks and
  `python3 Tools/Build/TestDocumentationCI.py`: passed (16 documentation tests).

Current-head hosted results are recorded in the associated PR and Beads; Linux results alone do not
claim Windows/macOS execution. Broader
residency policy, authored material/shader workflows and full graphical ED-M2 acceptance remain open.
