# ADR-0008: Optional immutable Editor mesh geometry residency

## Decision

Add `SceneDrawData::retainMeshGeometry` and a default-false
`ISurface::SupportsSceneGeometryResidency()` capability, forwarded by `RenderSurface`. Editor
Scene/Game submissions request the optimization only when supported. Vulkan supplies the first
native adapter; DX12/Metal retain existing draws and explicitly reject an unsupported request.
Public C++ consumers rebuild. Stable C/Zig, serialized asset identities, shaders and module
dependencies do not change.

An adapter still validates the complete current submission before any cache admission. For each
batch, its referenced contiguous vertex interval and locally rebased index slice form an owning
immutable key. A fingerprint rejects candidates quickly; exact vertex/index bytes authorize hits.
Combined upload placement, instances and materials are not identity. Equal content can share a
buffer across asset IDs, while reimported and frozen Play versions can coexist. No asset can render
from an ID alone or from a missing current CPU payload. Submesh ranges may occupy separate entries.

## Ownership and bounds

The first adapter handles up to 64 batches per submission with 64 resident entries. Larger draws
use the legacy combined upload. Duplicate batch ranges share an admission. Resident native memory
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
Run full Linux Development/Shipping and hosted CI. DX12/Metal adapters, broader residency policy
and full target-host ED-M2 acceptance remain open.
