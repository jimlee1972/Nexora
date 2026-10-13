# ED-M2 bounded Vulkan mesh geometry residency

Beads: `nexora-owg.1.6`. Contract:
[ADR-0008](../../../Roadmap/en/ADR-0008-Editor-Mesh-Geometry-Residency.md).

The optional Vulkan adapter retains immutable mesh content across frame slots and combined-upload
reordering. Complete current descriptors are still validated; exact owning canonical bytes authorize
hits. No missing asset can render from an identity-only lookup. Instances/materials remain fresh,
and main/shadow/reflection select the same retained buffer. Editor Scene/Game request the feature
only through the surface capability; unsupported adapters preserve legacy draws.

Portable `window_presentation.scene_geometry_residency` checks canonical index rebasing, exact
comparison despite fingerprint collision, owning payloads, 64-entry/32 MiB admission bounds,
current unsubmitted leases, LRU plans with unchanged ownership until commit, abandoned admission,
released resources and idempotent teardown.

The expanded real `window_presentation.vulkan_scene_upload_reuse` fixture traces all geometry
and instance buffer bindings through native submit/fence/device-idle calls. It checks unchanged
mesh uploads after reorder, native pixels for changed geometry and fresh instances/materials,
allocation/mapping failure fallback and retry, legacy/cached/legacy prefix-key conservation,
32 MiB native allocation pressure with real requirements/accounting, staged peak at most 40 MiB,
eviction only after completion, injected idle failure preserving prior ownership, resize hits and
zero retained CPU/native allocations after teardown. These are cache-specific bytes, not total
GPU memory or device-local VRAM. Existing frame upload/target/texture budgets remain separate.

Linux cloud results:

- `cmake --preset linux-development`: passed.
- `cmake --build --preset linux-development`: passed, 437 actions.
- `ctest --preset linux-development --output-on-failure`: 248/248 passed, no skips, 835.46 seconds.
  Portable residency, real Vulkan tracing and PBR pixels, and actual Editor Scene/OBJ fixtures ran.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed, 9 actions.
- Targeted portable/native residency tests passed (2/2, 2.86 seconds); common Vulkan PBR residency
  passed (6.76 seconds) with its original 105-submit/shadow/reflection pixel guards unchanged.
- Touched C++ format, `git diff --check`, changed Markdown/link/bilingual validation and
  `python3 Tools/Build/TestDocumentationCI.py`: passed (16 documentation tests).

Hosted results are recorded in the associated PR and Beads; Linux cloud execution alone does not
claim Windows/macOS native execution. The shared PBR fixture requests residency only when supported.
DX12/Metal residency adapters and full graphical target-host acceptance remain open.
