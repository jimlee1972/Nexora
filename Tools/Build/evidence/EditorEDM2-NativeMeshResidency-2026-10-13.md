# ED-M2 DX12 and Metal immutable mesh residency

Beads: `nexora-owg.1.7`. Contract:
[ADR-0008](../../../Roadmap/en/ADR-0008-Editor-Mesh-Geometry-Residency.md).

DX12 and Metal implement the existing optional geometry residency capability. Editor Scene/Game
therefore request the same canonical immutable content on all three desktop native adapters.
No new public descriptor, stable C/Zig field, asset format, shader input or module dependency.

DX12 owns staged upload buffers, checks `GetResourceAllocationInfo` requirements before allocation,
and waits for a successfully completed queue fence before retiring any LRU victim. Fresh palette
and shadow constants remain before the frame's instance/tone region. Main/shadow/reflection bind
canonical geometry separately from fresh frame instances. Omitting the old geometry prefix clears
its allocation-bound key before later legacy reuse.

Metal preflights heap buffer requirements and counts each admitted buffer's `allocatedSize`.
Shared buffers own copied geometry; only submitted inflight buffers are waited/released for eviction.
Retirements referenced by the current unsubmitted UI recording remain protected. Resize retains
immutable entries; draining/abandoning the current recording releases all cache keys/native buffers.
Native allocations are cache-specific observations, not total VRAM or driver-internal overhead.

All adapters share bounded admission for 64 batches/entries and the 32 MiB resident/8 MiB candidate
limits. Duplicate ranges reuse success or failure without repeated native admission; larger or
unsupported draws use legacy geometry. Portable tests cover these admission contracts, exact
canonical keys, LRU/budget/current leases, abandoned plans and owning resource teardown.

The shared native PBR fixture requests supported residency and periodically selects legacy mode
without changing any of its 105-submit pixel/shadow/reflection guards. The Metal native fixture
requests supported residency throughout Scene/UI, materials, shadow, reflection, wind, depth,
resize and abandoned-recording checks, with cached/legacy transitions in its steady frame loop.
Both verify cache activity and zero owned native/CPU payloads after teardown. Existing Vulkan
tracing/fresh-marker pixels preserve actual allocation/mapping/completion failure and budget proof.
Real DX12/Metal hardware OOM is not induced or claimed by these fixtures.

Linux cloud validation:

- `cmake --preset linux-development`: passed.
- `cmake --build --preset linux-development`: passed, 437 actions.
- `ctest --preset linux-development --output-on-failure`: 248/248 passed, no skips, 841.92 seconds.
  Portable binding/ownership contracts, actual Vulkan tracing/PBR and Editor Scene/Game fixtures ran.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed, 9 actions.
- Targeted portable/Vulkan tests: 3/3 passed (9.92 seconds).
- Changed C++ format, changed Markdown/link/bilingual checks, `git diff --check` and
  `python3 Tools/Build/TestDocumentationCI.py`: passed (16 documentation tests).

Actual DX12 and Metal runtime results are recorded in the associated PR/Beads and must come from
hosted Windows/macOS job logs, not from Linux compilation. Broader residency policy,
production authored material/shader workflows and full graphical target-host acceptance remain open.
