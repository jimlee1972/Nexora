# Owning StaticView project producer

`StaticProjectExport.h` exposes `CookStaticProject(StaticProjectExportInput, error)`. This synchronous
Editor C++ prerequisite consumes an owning `SceneDocument::RuntimeSceneCapture`, explicit owning
imported OBJ geometry and scalar PBR materials, and full project/scene asset UUIDs. It returns owning
bytes accepted by the production Runtime StaticView package loader and optional standalone
`NexoraProjectPlayer --verify-package`. It uses the real `AssetCooker`, NXAB envelopes, shared
[cooked codecs](../Runtime/CookedSceneAssets.md) and
[package/BundleBuilder validation](../Runtime/ProjectPackage.md).

The producer performs no filesystem/source lookup, catalog lookup, publication, gameplay execution
or GPU work. It retains no input borrow and never mutates inputs or a previously loaded package.
Independent calls may run concurrently; callers serialize mutation of each owning input while it
is being read. Invalid, unsupported, unresolved or oversized candidates return `nullopt` and an
optional diagnostic; the diagnostic is cleared on entry. Ordinary allocation exceptions propagate.
Returned bytes and loaded render values remain valid after capture/import DTOs and their original
World, document and importer are destroyed. Public C++ consumers rebuild; C/Zig ABI and wire schemas
remain unchanged. Editor's existing asset-pipeline/Renderer dependencies are reused.

## Capture admission and identity

Project and scene asset UUIDs, capture scene ID and document generation must be nonzero. The source
scene ID is provenance: it is not serialized by World, and an isolated loader assigns its own local
scene ID. Node IDs/generations must be nonzero, unique, match the capture's document generation and
belong to this snapshot. Tracked nodes need not cover every Runtime entity. Every mesh renderer,
including an untracked entity, must resolve explicitly supplied geometry; authoring names and Euler
hints never enter the package.

The producer caps raw snapshot text at 64 MiB and its declared schema-3 entity count at 100,000 before
World parsing/allocation. World validates identities, transforms and hierarchy, and final shared
cooked-scene validation admits Camera/Light fields, bindings and opaque framing. Entity membership
is indexed once. Raw World text is preserved exactly, rather than rewritten through normalized
Runtime transforms. These are logical data/output limits, not peak allocation or RSS guarantees.

All supplied assets have nonzero full UUIDs unique across mesh/material/scene assets. Project UUID
belongs to a separate identity domain. Distinct mesh UUIDs with the same existing derived 64-bit
resource reject, including unused supplied meshes. A shared low UUID word is not a collision by
itself. There are at most 4095 supplied mesh/material assets plus the scene, with each supplied mesh
having 1–65,535 vertices and 1–1,048,576 indices. Aggregate supplied geometry is limited to 128 MiB
using the cooked 48-byte vertex and 2-byte index sizes, before conversion or tangent allocation.

Only assets referenced by scene bindings enter the exact package dependency closure. Unused supplied
assets are omitted; their identity/count/geometry bounds still apply. Used geometry is checked by
the real Renderer tangent generator and shared codec, including finite position/normal/UV,
nonzero normal, valid triangle indices and deterministic unit tangents/handedness. Imported bounds
are authoring metadata and are not serialized. Used materials must pass `ValidateMaterialAsset`,
including exact agreement between canonical scalar values and their reflected Renderer schema.
Textures, shader graphs and alternate surface/model/profile contracts reject.

## Material and opaque policy

Legacy `material.shader` retains every bit. Without a scalar UUID override it must be zero, which
selects the documented neutral material. The reserved opaque type `0x45444d41544c0001` or name
`editor.material.asset` is recognized even when the other field disagrees: both must match, with
exactly 17 bytes, version one and a nonzero full material UUID. It requires a mesh-renderer entity
and explicitly supplied scalar asset. Unsupported versions, aliases, missing assets and references
on a non-mesh entity reject rather than silently becoming inactive/neutral.

All other opaque records preserve entity association, full type ID, name and payload bytes and
remain inactive. Limits match capture/Runtime: 4096 records total, 64 per entity, 1 MiB per payload,
256-byte nonempty names and 16 MiB total names plus payloads. Duplicate types, zero IDs, CR/LF/NUL
in names, foreign entities and overflow reject before copying each payload. Legacy non-UTF-8 names
and arbitrary binary payload bytes remain valid. Metadata and asset input order are canonicalized
by the shared codecs/package encoder; unchanged raw World text produces identical package bytes.

## Publication boundary and acceptance

Owning inputs are evidence of captured content, not authorization or proof of current content.
Document generation identifies an object, not an edit revision. A future coordinator must check
workspace identity/write access, recovery state, current scene/imported-content revisions and
cancellation before atomic publication. The producer does not authorize stale catalog content,
write a manifest, choose an output path or mark dirty documents saved.

`editor.static_project_export` covers actual import/capture-to-package ownership and exact closure,
untracked Runtime renderers, mirror hierarchy, full legacy shader IDs, raw unknown components,
resource collisions, reserved material admission, invalid schema/geometry/metadata, exact mesh/index
and opaque limits, geometry accounting and a functional 100,000-entity World. Optional normal and
optimized `editor.static_project_export_cli` tests feed the generated production artifact to the
standalone player from a Unicode path, verify its report and unchanged bytes, and repeat cooking
from independent owning captures. [Delivery evidence](../../Tools/Build/evidence/EditorEDM6-StaticProjectExport-Linux-2026-10-09.md)
records executed gates. The optional [native Project Player](../../Apps/ProjectPlayer/README.md)
now renders admitted StaticView packages using the production native presentation path, with
[executed Linux acceptance](../../Tools/Build/evidence/EditorEDM6-NativeProjectPlayer-Linux-2026-10-09.md).
Gameplay compilation, GUI export/publication, deployment/signing and the complete ED-M6 milestone
remain open. Existing NXAB still limits this
package to little-endian hosts; FNV integrity is not authenticity.
