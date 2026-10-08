# Cooked static-scene asset contract

`CookedSceneAssets.h` provides Runtime-owned C++ payloads for a bounded static scene package. It is
compiled with the existing `NEXORA_ENABLE_ASSET_PIPELINE` feature and uses the existing Runtime to
Renderer dependency. It requires no Editor, source asset files, filesystem operations, native window,
GPU allocation or gameplay module. The stable C/Zig ABI and World scene schema are unchanged.

All calls are synchronous and retain no input borrow. Returned DTOs, encoded bytes and Renderer
adapters own their data. Independent calls can run concurrently; a caller serializes mutation of an
input DTO while a codec reads it. Invalid, oversized, unsupported, truncated or nonfinite content
returns `foundation::ErrorCode::InvalidArgument` without publishing a partial result. Ordinary
allocation exceptions propagate. An outer package loader owns hashing, asset dependency closure,
aggregate geometry budgets, UUID/resource collision admission and replacement of its active World.

## Shared mesh identity

`runtime::MeshResourceId(AssetUuid)` exactly preserves the previous Editor 64-bit derivation: rotate
the high UUID word by 23, XOR the low word, apply the two existing mixing multipliers and final XOR,
then map a nonzero UUID's zero result to one. Zero UUID yields zero. `editor::MeshResourceId` is a
compatible re-export of the same function, avoiding ambiguous argument-dependent overload lookup.
The fixed UUID `12345678-9abc-def0-fedc-ba9876543210` still produces `0xb0f7765a695fb756`.
Different full UUIDs can collide in 64 bits; a package must reject such collisions rather than
discarding the full UUID or resolving by truncated identity.

## Payload types and wire

Canonical RuntimeBlob types are `nexora.mesh.v1`, `nexora.scalar-pbr.v1` and `nexora.scene.v1`.
The payload codec has its own schema version one; NXAB's existing envelope is separate. Integers
are explicitly little-endian unsigned values. Floats are IEEE-754 binary32, encoded through their
bit representation rather than native struct layout. No padding or host-native pointers occur in
the wire. Every decoder rejects trailing bytes and unknown magic/schema. Counts and declared byte
lengths are checked against hard limits and actual remaining input before reserve/resize/copy.
Every checked multiplication uses already-bounded counts or division/subtraction against remaining
bytes, so a malicious count cannot overflow a length calculation.

| Payload | Schema-1 layout |
| --- | --- |
| Mesh | `NXCMESH\0` (8 bytes), version u32, vertex count u32, index count u32; each vertex's position XYZ, normal XYZ, UV XY, tangent XYZW as twelve f32; indices u16 |
| Scalar PBR | `NXCPBR\0\0` (8 bytes), version u32; base-color RGB, emission RGB, metallic, roughness, occlusion as nine f32 |
| Scene | `NXCSCNE\0` (8 bytes), version u32, World snapshot byte length u32, binding count u32, opaque count u32; raw World text; bindings; opaque records |
| Scene binding | entity u64, mesh UUID high/low u64, material-present u32, material UUID high/low u64, mesh resource u64 (52 bytes) |
| Opaque record | entity u64, type ID u64, name byte length u32, payload byte length u32; raw name then raw payload (24-byte prefix) |

Bindings encode in increasing entity ID order; opaque records encode in increasing `(entity,type)`
order. Unordered owning inputs produce the same canonical order without changing input storage.
Decoders reject noncanonical/duplicate order. Material-present is exactly zero or one; absent
material UUID words must be zero. UUID word order is high then low; each word's bytes are little-endian.
World snapshot and opaque name/payload bytes are preserved exactly, without Unicode normalization.
The same owning input yields identical bytes; the codec does not rewrite semantically equivalent
World whitespace or floats into a new textual scene representation.

## Mesh and scalar material admission

Meshes contain one to 65,535 vertices, matching the actual Editor OBJ import limit, and one to
1,048,576 indices in complete triangles. Thus the largest accepted triangle-aligned index count is
1,048,575. Every u16 index addresses an existing vertex. Position/normal/UV/tangent components must
be finite; normal and tangent XYZ lengths must exceed `1e-12`, and tangent W is exactly minus or
plus one. Normalization is left to the rendering contract; no topology, source material, bounds or
texture file is inferred. A package's aggregate cooked vertex/index storage must not exceed
128 MiB. The package loader owns this aggregate limit, because a single payload cannot know other
assets in the package.

Scalar PBR is opaque, texture-free linear color. Base color, metallic, roughness and occlusion are
finite in `[0,1]`; emission RGB is finite in `[0,65504]`. Zero roughness remains supported by the
existing shader policy. Texture sets, alpha/masked modes, shader graphs and extension shaders require
distinct supported payload contracts rather than silently entering this format.

`ToRendererMesh` returns an owning existing `renderer::Mesh` with copied position/normal/UV/indices.
Tangents remain available in the owning CookedMesh for a native PBR consumer; the existing
Renderer mesh DTO has no tangent field. `ToRendererMaterial` returns a validated fixed opaque PBR
schema with shader ID `nexora.runtime.scalar-pbr`, the five scalar parameter groups and an explicit
Emission feature when needed; it resolves no textures. These CPU adapters do not create a native
shader or claim native pixel acceptance.

## Scene, bindings and preserved components

The raw snapshot is at most 64 MiB and must be a valid `NEXORA_SCENE 3` with at most 100,000 entities.
The codec preflights its count before Runtime allocates entities. Zero/UINT64_MAX entity identities
and UINT64_MAX parent identities reject to prevent World allocator wrap. The existing World loader
validates duplicate IDs, finite/nondegenerate transforms, parent membership and acyclic hierarchy;
it normalizes rotations in its temporary candidate. Serialized snapshot bytes remain unchanged.
Camera fields and Light intensity must be finite. Enabled Camera uses the existing Runtime **degree**
units, `0 < vertical_field_of_view < 180`, positive near plane and far greater than near. Enabled
Light intensity is nonnegative. No Camera is selected or activated by this codec.

Every mesh-renderer entity has exactly one binding. Bindings reference a nonzero full mesh UUID and
matching nonzero derived resource, which must equal the saved World's mesh resource. A present
material UUID is nonzero. Bindings cannot point to another entity or a non-mesh entity; package asset
existence and typed dependency closure are checked by the package loader. Entity lookups are indexed
once rather than performing a quadratic search for the 100,000-entity limit. The saved legacy
64-bit `material.shader` is preserved verbatim and is never reinterpreted as a UUID. A static loader
must explicitly define its supported override/unsupported shader policy.

Opaque records retain entity association, full type ID, name and raw bytes. They require an existing
entity, nonzero type, nonempty name of at most 256 bytes without CR/LF/NUL, and unique `(entity,type)`.
These name rules preserve the existing Editor unknown-component contract; legacy non-UTF-8 names
are not silently rewritten or rejected. Payloads are at most 1 MiB each; aggregate payload **plus
name** bytes are at most 16 MiB, with at most 4,096 records and 64 per entity. Opaque bytes may contain
any byte, including NUL. No component executes here. A StaticView consumer reports unknown components
as preserved-but-inactive, and separately validates the exact known material-reference version/type
and its agreement with bindings; preservation is not a gameplay consumer implementation.

## Acceptance boundary

`runtime.cooked_scene_assets` exercises bounded/corrupted framing, exact mesh and opaque limits,
nonfinite/index/material rejection, owning deterministic roundtrips, the existing Renderer validators,
real World hierarchy/component loading and real `AssetCooker`/NXAB/`BundleBuilder` consumption.
The enclosing delivery evidence records actual Development and Shipping commands/results.
This contract alone does not claim Editor cook/export UI, native static-player rendering, gameplay
source compilation, deployment/signing, complete ED-M6 acceptance or persistent GPU residency.
