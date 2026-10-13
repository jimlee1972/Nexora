# ADR-0008: Optional immutable Editor mesh geometry residency

## Decision

Add `SceneDrawData::retainMeshGeometry` and a default-false
`ISurface::SupportsSceneGeometryResidency()` capability, forwarded by `RenderSurface`. Editor
Scene/Game submissions request the optimization only when supported. Vulkan, DX12 and Metal
provide native adapters; other surfaces retain the default-false capability and unsupported request.
Public C++ consumers rebuild. Stable C/Zig, serialized asset identities, shaders and module
dependencies do not change.

An adapter still validates the complete current submission before any cache admission. For each
batch, its referenced contiguous vertex interval and locally rebased index slice form an owning
immutable key. A fingerprint rejects candidates quickly; exact vertex/index bytes authorize hits.
Combined upload placement, instances and materials are not identity. Equal content can share a
buffer across asset IDs, while reimported and frozen Play versions can coexist. No asset can render
from an ID alone or from a missing current CPU payload. Submesh ranges may occupy separate entries.

## Ownership and bounds

The adapters handle all 4,096 batches per submission with 64 resident entries. A fixed 64-range
memo bounds admission/key work and preserves each success/failure result across every binding.
Additional distinct ranges use legacy geometry individually; prior ranges remain resident.
Slot metadata uses 4 KiB and each batch scans at most 64 memo entries without heap allocation.
Duplicate batch ranges share an admission. Resident native memory
allocation requirements are capped at 32 MiB, with an additional staged candidate capped at 8 MiB.
Keys own separate CPU payloads bounded by their native allocations. These are cache-specific
limits; existing frame uploads, targets and textures have independent budgets. Host-visible
coherent immutable buffers are not a device-local VRAM claim.

Allocation, mapping, copying and GPU completion precede eviction/publication. LRU planning does
not mutate ownership. A resource already selected by the current unsubmitted draw is leased and
cannot be evicted; prior submitted users must complete before retiring any other victim. Failed
admission preserves residents and uses the validated legacy geometry for that batch. Resize drains
frame resources but retains immutable mesh entries. Shutdown drains and releases every key/buffer.

Instances, materials, lighting and tone data are copied each draw. When all batches are resident,
the frame buffer omits its combined geometry prefix. Writing instance data there clears any old
allocation-bound prefix key, preventing a false hit when switching back to legacy uploads.
Main, shadow and reflection draws bind the same selected immutable geometry; native indices are
locally rebased while public index ranges remain absolute in the borrowed submission.

## Validation and remaining work

Portable tests cover canonical layout independence, exact comparison despite a hash collision,
owning payloads, entry/memory bounds, LRU and unsubmitted leases, abandoned plans and teardown.
Real Vulkan tracing/pixels cover reorder, changed geometry, fresh instances/materials, allocation
failure fallback/retry, legacy switching, budget eviction, completion failure, resize and cleanup.
DX12 uses owning upload buffers with allocation requirements; Metal preflights buffer requirements
and counts admitted `allocatedSize`. Fresh palette/shadow data precede DX12 instances; Metal waits
only submitted buffers and preserves current UI retirements. The shared admission helper bounds work
and reuses duplicate success/failure results. Hosted native DX12 PBR and Metal pixel fixtures request
residency without changing their visual guards. Run full Linux Development/Shipping and hosted CI.
Broader residency policy and full target-host ED-M2 acceptance remain open.
