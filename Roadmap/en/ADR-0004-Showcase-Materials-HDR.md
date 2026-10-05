# ADR-0004: Native Showcase materials and HDR

Date: 2026-10-05. Status: implementation direction for VIS-M1; pixel/host acceptance is required.

## Decision

Keep Renderer/shared shader policy in `Shaders/Nexora/Common.slang`; native Presentation adapters
own GPU bindings, targets, barriers and fence retirement. Showcase owns scene content and converts
Runtime UUID/version resources into borrowed per-submission material slots. No native handles or
backend shading policy enter Runtime, and no shared PBR math is copied into Showcase/backend strings.

First establish bounded per-batch opaque base-color/texture materials on the existing Lambert path.
Then integrate a shared Slang PBR entry (normal/ORM/emission and vertex tangents), followed by IBL
resources and linear HDR color targets with a tone-map composition pass. These are separate acceptance
slices; multiple colored Lambert batches alone do not complete VIS-M1.

## Submission and compatibility

Append a material slot to `SceneMeshBatch` and a borrowed material span to `SceneDrawData`; empty
materials retain global legacy light/color/texture behavior. An empty batch span draws the whole
upload with slot zero. Validate all ranges, finite opaque colors and texture references before
recording scene commands. Initial bounds: 64 materials, 4096 batches/instances, existing geometry
budgets. Bad descriptors do not consume the acquired frame's scene submission.

Runtime resource identities remain UUID/version based. Draw slots are ephemeral indexes, not asset
identities or GPU cache keys. Application-assigned texture IDs represent immutable generations;
replacement uses a new ID, with old native resources retained until every protecting frame completes.
Resize/recovery/shutdown drains work before releasing descriptors and targets. Public C++ layouts
require all consumers to rebuild; C/Zig wire and persistent scene/content schemas remain compatible.

## PBR/color design for subsequent slices

Tangents use XYZ direction plus handedness; normal maps use +Y tangent-space convention. ORM is
R=occlusion, G=perceptual roughness, B=metallic. Base/emission color textures decode from sRGB once;
normal/ORM/BRDF data stay linear. Missing maps get explicit white/flat-normal/neutral-ORM/black-emission
resources. Material transforms and inverse-transpose normals retain exact affine instance semantics.

IBL sources and derived irradiance/prefilter/BRDF LUT carry source licensing, conversion parameters
and content hashes through the bundle. Use tier-1 explicit bindings and bounded resources. The native
scene output is linear floating-point HDR; exposure/ACES/bloom precede the single display transfer
conversion and UI composition. Legacy scene copy remains available. HDR render-target precision does
not imply that the monitor/swapchain negotiated HDR10.

Use frame-owned uploads/targets, immutable texture generations and transactional allocation. Do not
release resources on scene/material changes while submitted commands still reference them. Shared
material validation, native independent material pixels, normal/roughness/orbit cases, missing-map
fallbacks, resize/reload/lifetime and backend comparisons are acceptance evidence; shader compilation
alone is insufficient. Hardware visuals and the final GTX 960 budget remain separate VIS-M3/M6 gates.

Shared direct-light PBR and hardware sRGB filtering are integrated. The IBL slice adds bounded
linear RGBA16F resources, source/license/conversion metadata dependencies in the Runtime bundle,
seven native sampled resources and an 80-byte private material packet. Public C++ consumers rebuild;
NXAB and stable C/Zig schemas remain unchanged. Floating HDR scene targets/composition remain open.
