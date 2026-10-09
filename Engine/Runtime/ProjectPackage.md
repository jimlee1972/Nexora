# Static project package contract

`ProjectPackage.h` is a Runtime-only, allocation-owning consumer of the public cooked scene,
mesh and scalar PBR schemas. It does not depend on Editor, consult source files, load gameplay or
plugin code, compile an application, sign a release or certify native rendering. Its only
capability is **StaticView**. `NexoraProjectPlayer --verify-package` exercises the real consumer.

## Wire and integrity

Schema 1 is eight magic bytes `NXPROJ\0\1`, followed by little-endian `u32 schema=1`,
`u32 capability=1`, project UUID high/low `u64`, entry-scene asset UUID high/low `u64`, and
`u32 asset_count`. Each asset is `u64 blob_bytes` followed by the existing serialized `NXAB`
RuntimeBlob. Full UUIDs are strictly increasing by `(high, low)`; duplicates and zero IDs reject.
Blob dependency UUIDs also use increasing full-UUID order; encoding canonicalizes their set order
without changing the supplied RuntimeBlob or its payload bytes/hash.
The final `u64` is FNV-1a over every preceding byte, using the existing AssetPipeline basis
1469598103934665603 and multiplier 1099511628211. No trailing bytes are accepted.

The outer numeric fields are explicitly little-endian. Existing NXAB numeric fields currently use
native layout, so this package rejects non-little-endian hosts. Its FNV checksums detect accidental
corruption; they are not cryptographic authentication or a signature. The accepted NXAB schema is
1, with exact payload hashes verified through the real AssetCooker deserializer and BundleBuilder
integrity path. Consumers must not treat an untrusted package as authorization to execute code.

The package contains exactly one `nexora.scene.v1` entry scene, plus its referenced
`nexora.mesh.v1` and `nexora.scalar-pbr.v1` assets. Mesh and material blobs have no dependencies;
the scene dependency set must equal its distinct bound mesh/material UUIDs. Duplicate dependencies,
missing referenced assets, extra/unreferenced assets, extra scenes and unknown types reject.
Encoding validates first and sorts full UUIDs deterministically. Preserved World snapshot text is
not rewritten, preserving exact snapshot compatibility within the supported scene schema.

## Resolution and ownership

Loading creates an independent `World{Play}`, reads the actual scene snapshot and activates its
scene. Every mesh-renderer entity must have exactly one compatible binding. Mesh resource IDs must
match the public `MeshResourceId(full_uuid)` derivation and the saved World component. Two distinct
UUIDs producing the same resource ID reject; no last-writer-wins mapping or low-64-bit truncation
is allowed. All transforms, hierarchy, camera/light values, geometry and material payloads must
pass the cooked codec and World validators.
The candidate scene is indexed once by entity ID; parent-to-child adjacency and a nonrecursive
parent-before-child pass compute each exact `parent_matrix * local_TRS_matrix` once. Binding and
opaque resolution then use those indexes. Deep hierarchies do not repeatedly call World's
linear entity lookup or walk an ancestor chain per instance. The existing World snapshot reader
also validates parent existence/cycles using a memoized iterative walk; the cooked codec performs
its own bounded parsing and indexing before invoking that authority. These are algorithmic bounds,
not a promised frame-time or 100k-entity native performance acceptance result.

`LoadedStaticProject` owns the World, scene/opaque metadata and immutable typed assets. Its public
World access is const. Each `StaticRenderItem` supplies the exact column-major world matrix and
shared ownership of its mesh and optional scalar PBR material. Resolved mesh ownership remains
valid after replacing or destroying the loaded package. `WorldView`, `SceneData` and `RenderItems`
return read-only borrows that expire when the owning `LoadedStaticProject` is moved or destroyed;
copy a render item to retain its matrix and shared asset ownership. These are renderer-ready data, not GPU
handles, uploads or a native pixel acceptance result. Renderer adapters use the public cooked
schemas; native palette/upload limits remain the adapter's responsibility. The optional standalone
[native ProjectPlayer](../../Apps/ProjectPlayer/README.md) applies bounded NativePBR admission and
draws these assets without Editor/source content. The loader remains pure data resolution and
does not execute gameplay, admit GPU resources or certify native pixels.

The full saved `MaterialComponent::shader` integer is preserved. A bound scalar PBR material
explicitly supplies StaticView material semantics without changing that legacy ID. Without a
scalar binding, shader zero selects the documented neutral material; a nonzero shader rejects
because this consumer has no arbitrary legacy shader implementation. Unknown material-reference
versions or malformed/mismatching reserved `editor.material.asset` metadata reject. Recognized
references are metadata, not inactive plugin behavior. All other opaque records retain their
exact bytes/type/name/entity association and are reported as **inactive**. No claim is made that
their missing component behavior runs. A package can verify static data while containing inactive
components; this is not complete application/gameplay acceptance.

## Bounds and file access

Maximum disk package: 256 MiB; assets: 4096; serialized NXAB blob: 96 MiB; total decoded mesh
vertex/index storage: 128 MiB. Every NXAB type length is at most 64 bytes, hash length at most
16 bytes, dependency count at most 4096, and payload length must exactly match remaining bytes
before the existing deserializer can allocate. In-memory candidates use the same limits before
serialization. Cooked codec limits additionally constrain vertices, triangle indices, entities,
bindings and opaque records. Memory limits bound accepted data, not total peak allocations:
validation/serialization retains temporary owning copies and the real verified bundle.
Disk decoding additionally retains input bytes and deserialized RuntimeBlobs while constructing
typed assets, metadata and the isolated World; hierarchy indexes/matrices are temporary copies.
BundleBuilder builds a copied serialized bundle, and verification creates bounded deserialized
copies. Accordingly the 256 MiB file limit is not a total working-memory budget.
The 96 MiB blob allowance accommodates the scene codec's 64 MiB World snapshot, bounded opaque
metadata and up to 100,000 bindings. The owning consumer validates every item at that scale; the
CLI's 64-row report cap only bounds diagnostic output and never reduces admission/render-item
capacity. It does not impose native material-palette or aggregate upload limits.

`ReadStaticProjectPackage` only reads an explicitly provided regular package file. On POSIX it
walks directory descriptors with `openat/O_NOFOLLOW`, opens the leaf nonblocking, verifies the
actual descriptor with `fstat`, and reads in 16 KiB chunks while enforcing the live byte bound.
Symlink parents/leaves, FIFO/device/directory input, missing files and growth past the bound reject.
Other hosts check each path component and regular-file status before bounded streaming; that
fallback does not promise POSIX descriptor-anchored behavior during concurrent path replacement.
Files may contain spaces and Unicode path components. Loading performs no writes or discovery.

All codec/loader calls are synchronous and caller-thread owned. A caller can run them on a worker
using owning input; no authoring-thread borrow crosses the boundary. Failed validation returns
no candidate and an actionable error string, leaving any previously accepted package untouched.
Project writer access, recovery, generation/revision checks, cancellation, output publication and
process lifecycle remain the future Editor export/Build coordinator's responsibility.
