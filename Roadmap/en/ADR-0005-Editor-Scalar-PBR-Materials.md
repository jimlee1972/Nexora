# ADR-0005: Editor scalar PBR material assets

Date: 2026-10-08. Status: implementation direction; acceptance is recorded separately.

## Decision

Extend ADR-0004's application-owned material conversion into Editor authoring. Renderer retains
material validation and tangent generation; Presentation retains shared native PBR pipelines,
bindings and protecting-frame resource lifetime. Editor imports immutable owning CPU material
assets, publishes them on the authoring thread and builds ephemeral draw palettes. It performs no
native GPU work during import, catalog publication or document editing.

The initial `.nmaterial` source is a bounded, editable, locale-independent whitespace-token format:

```text
NEXORA_MATERIAL 1
base_color 0.8 0.2 0.1
metallic 0
roughness 0.5
occlusion 1
emission 0 0 0
```

Tokens occur exactly in this order; arbitrary whitespace is allowed, comments/additional tokens
are rejected. Source bytes are capped at 64 KiB. Base color, metallic, roughness and occlusion are
finite values in [0,1]; emission is finite linear RGB in [0,65504]. Only opaque PBR is supported.
The scalar fields are canonical; the typed asset also carries their exact derived Renderer
`MaterialSchema`. Publication rejects divergent parameters, unsupported shader/profile/features and
invalid scalar values; no native descriptor,
texture, arbitrary shader, graph or IBL policy is serialized. Successful imports retain persistent
UUID metadata and owning payloads through indexing, background/synchronous reimport and Content
Undo. Cancellation, unsupported source versions, invalid values and stale publication retain the
last good live artifact. Typed workspace material count is capped at 4096.

## References and compatibility

Do not reinterpret `Runtime::MaterialComponent::shader`. Its complete legacy 64-bit value is
preserved. Instead, `editor.material.asset` is an Editor-owned opaque document component with
reserved TypeId `0x45444d41544c0001`. Its 17-byte payload stores byte version 1, followed by the
UUID high and low uint64 words in explicit little-endian order. Zero UUID, wrong type/name,
truncation and unsupported versions remain unresolved; their owning bytes remain saved and may
not be overwritten by this version's assignment UI. Existing scene containers and stable C/Zig
schemas are unchanged. A future runtime/cook consumer needs its own explicit integration.

The catalog resolves UUID only within the current nonzero project generation; snapshots own
material data across reimport/unload. Inspector assignment requires one selected entity with a
live Mesh Renderer, writable project/content, matching entity/document/project generations and
the same live typed payload in Content and catalog. One accepted assignment uses the existing
opaque-component Undo transaction. Multi-selection and removing the reference are separate work.
Missing references stay stored and are shown honestly.

## Drawing and remaining gates

Scene View converts valid scalar assets to at most 64 borrowed `SceneMaterial` slots, reserving
slot zero for a neutral fallback. A material UUID occurs once per frame palette. Budget overflow
uses the fallback without rewriting authored references. Valid materials use white instance tint,
preserving authored color; ground, proxies and gizmos retain their existing tint. No resolved
materials preserves the legacy Lambert path. PBR uses Renderer-generated tangents, including the
stable degenerate-UV fallback, and exact affine instance matrices.

Editor declares its direct Renderer dependency in the module graph and CMake. Consumers rebuild
for the added C++ payload/catalog/UI boundary. This slice does not establish persistent per-asset
GPU geometry residency, texture/shader authoring, Game View materials, shipping material assets,
physical-GPU output or complete ED-M2 multi-DPI authoring acceptance.
