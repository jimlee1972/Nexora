# ADR-0006: Runtime-owned cooked static projects

Status: accepted for the bounded StaticView prerequisite; complete ED-M6 remains open.

## Context

The portable build frontend publishes caller-supplied artifact manifests. It does not cook an
authored Editor project or provide a consumer for arbitrary project geometry/materials. A Showcase
package cannot establish that workflow. Shipping must consume public Runtime data without Editor.

## Decision

Define owning schema-1 mesh, scalar PBR and scene codecs under the asset-pipeline feature, plus a
bounded StaticView package and optional Runtime-only `NexoraProjectPlayer --verify-package`.
Reuse AssetCooker/NXAB and BundleBuilder integrity validation. Preserve full asset UUIDs, the
existing mesh resource derivation, World snapshot version 3, legacy shader IDs and opaque bytes.
Reject resource collisions, unsupported material versions, unresolved dependencies and nonzero
legacy shaders without an explicit scalar binding. Other opaque components remain inactive.

Codec fields are explicitly little-endian; existing NXAB makes this package little-endian-host
only. FNV checksums detect corruption, not authenticity. No stable C/Zig ABI changes are made;
public C++ consumers rebuild. Bounds and ownership are specified in the
[codec](../../Engine/Runtime/CookedSceneAssets.md) and
[package](../../Engine/Runtime/ProjectPackage.md) contracts. Hierarchy resolution is iterative and
indexed, with exact matrices including mirror/shear; no recursive ancestor walk per binding.

## Consequences and acceptance

The Editor now provides bounded owning Runtime capture and a pure `CookStaticProject` producer
from that capture plus explicitly supplied owning imported OBJ/scalar PBR assets. Its exact closure
uses the shared codecs/cooker/package validation; no source lookup or publication occurs. See the
[producer contract](../../Engine/Editor/StaticProjectExport.md). Full UUID/resource collisions,
unsupported reserved bindings, missing assets and nonzero legacy shaders without a scalar override
reject. The scene text and inactive opaque bytes remain unchanged.

The real CLI can verify an owning package after source content is removed. It reports StaticView
and inactive components explicitly. It does not render a native window, load gameplay, compile,
deploy or sign an application. Current-state/stale/cancel checks, atomic publication,
native resource admission and Build/deploy/log UI are dependent work.

Keep every full ED milestone unmarked. Delivery requires full Linux Development, Monolithic Shipping,
optional-feature stripping and actual CLI consumption; commands/results belong in delivery evidence.
