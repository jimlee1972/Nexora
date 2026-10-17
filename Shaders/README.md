# Shared Slang shader library

`Nexora/Common.slang` is the importable public module. Helper implementation files are grouped
under `Nexora/Common/` and are compiled into the same Slang module. Slang 2026.18 accepts the
compound import path while the primary declaration uses the string form `module "Nexora.Common";`.
Import it with:

```slang
import Nexora.Common;
```

## Current helpers

| Function | Contract |
| --- | --- |
| `NexoraSafeNormalize` | Returns zero for near-zero vectors instead of producing NaNs. |
| `NexoraSrgbToLinear` / `NexoraLinearToSrgb` | Converts RGB values with the piecewise sRGB transfer function. |
| `NexoraRec709Luminance` | Computes luminance from linear Rec. 709 RGB. |
| `NexoraAcesApproximate` | Applies a compact ACES-fitted tone-map curve and saturates output to display range. |
| `NexoraFresnelSchlick` | Schlick Fresnel term using reflectance at normal incidence. |
| `NexoraEvaluateDirectBrdf` | Evaluates one linear-space Cook–Torrance direct-light contribution with GGX distribution and Schlick-GGX geometry. |
| `NexoraEvaluatePbr` | Combines metallic/roughness direct lighting, diffuse irradiance, prefiltered specular IBL, BRDF LUT data, AO, and emission. |
| `NexoraEvaluateStylizedPbr` | Applies an art-directed ramp, light tint, and shadow tint on top of the PBR direct-light result. |
| `NexoraEvaluateFaceSdfShadow` / `NexoraEvaluateAnimeHairHighlight` / `NexoraEvaluateAnimeRim` | Anime character face, hair, rim, and outline helpers. |
| `NexoraEvaluateVegetationWind` / `NexoraEvaluateVegetationTransmission` | Instanced vegetation wind, alpha-cutout, and back-light transmission helpers. |
| `NexoraEvaluateWaterSurface` | Shallow/deep color, depth fade, Fresnel, SSR/environment reflection fallback. |
| `NexoraEvaluatePcf4` | Four-tap PCF visibility and explicit normal/slope shadow bias. |
| `NexoraSelectShadowCascade` | Selects a stable cascade index from view depth and up to four split distances. |
| `NexoraEvaluateSsao` / `NexoraApplyBloom` / `NexoraEvaluateFxaa` | Portable post-process building blocks for SSAO, bloom, and FXAA. |
| `NexoraTransformUiPosition` / `NexoraEvaluateNineSliceUv` / `NexoraCompositeUiColor` | Screen/world UI coordinates, nine-slice atlas UVs, clip alpha, and straight-alpha UI output. |
| `NexoraEvaluateUnlit` / `NexoraEvaluateUnlitColor` | Backend-neutral unlit/emissive shading model output with explicit alpha. |
| `NexoraSkinPosition` / `NexoraTransformInstance` / `NexoraSelectForwardPlusDepthSlice` | Skinning, instancing, and Forward+ light-grid support helpers. |

All lighting helpers expect linear-space colors. `NexoraEvaluatePbr` expects a material normal in
tangent/world space, metallic in `[0, 1]`, perceptual roughness in `[0, 1]`, an irradiance sample,
a prefiltered reflection sample, and the two-term BRDF LUT result. The library intentionally keeps
texture sampling and resource binding in the owning shader entry point so the same math works with
bindless and tier-1 descriptor paths. It does not choose a backend descriptor API or create a
second Renderer for stylized, anime, vegetation, water, or UI shading.

`CommonSmoke.slang` preserves the original small contract. `ShaderLibrarySmoke.slang` exercises the
full math surface, `PbrSmoke.slang` is a real vertex/fragment PBR entry with TextureCube,
Texture2DArray, normal/ORM/emission maps, IBL and BRDF-LUT resources, and `UiSmoke.slang` is a real
vertex/fragment UI entry with atlas sampling, clip, nine-slice, and explicit binding sets. When
`NEXORA_ENABLE_SLANG=ON`, all three smoke shaders cross-compile to SPIR-V and Metal source, plus
DXIL on Windows. The regular `build.shader_contract` also checks the module's public surface and
smoke-shader coverage when Slang is disabled.

## Native Showcase PBR entry

`Engine/Presentation/shaders/scene_pbr.slang` imports this shared library for actual native
Presentation draws. Its four explicit tier-1 material maps use hardware sRGB base/emission views;
+Y normal and ORM are linear. `GeneratePbrShaders.py` uses pinned Slang 2026.18 to embed SPIR-V and HLSL/Metal source,
with an optional Slang-enabled regeneration check. Slang remains the authoring source: HLSL is
its generated DX12 backend output, which the native adapter compiles through `D3DCompile`.
Generated HLSL is stored in adjacent C++ raw literals bounded to 8 KiB each for MSVC; concatenation
preserves every shader-source byte. `window_presentation.hlsl_embedding` checks that bound and
reproducible storage even without a Slang compiler. Native adapters own geometry/material resources
and frame lifetimes. Three additional bounded linear resources supply diffuse irradiance,
roughness-prefiltered latlong specular and the BRDF LUT. Environment coordinates use +Y rotation,
U wrap/V clamp; the LUT clamps both axes. Material perceptual roughness spans prefilter levels;
shared direct BRDF math owns its numerical roughness floor. `NexoraEvaluatePbr` consumes actual
environment samples. Legacy targets apply ACES/display transfer in the fragment; HDR targets
preserve linear radiance and `scene_tonemap.slang` applies exposure, shared ACES and one display
transfer in Main before UI. `GenerateToneShaders.py` uses the same pinned Slang 2026.18 embedding
and regeneration-check route; native tone pipelines contain no duplicated color math.
No PBR math is maintained in backend-specific handwritten shader strings.

`NexoraExtractBloom` preserves linear HDR hue while rejecting radiance below a bounded threshold.
Native tone entries own the bounded neighbor sampling and call shared extraction/addition, ACES,
color grade and display transfer. Backend adapters only bind the same 32-byte scalar packet;
no bloom or grading math is duplicated in native source strings.

Native PBR vertex/shadow entries call shared `NexoraEvaluateVegetationWind`, and both color/shadow
fragments use `NexoraEvaluateVegetationAlpha` with the same texture and cutoff. Thin-leaf back lighting
uses `NexoraEvaluateVegetationTransmission` in linear HDR. Adapter code binds the shared 240-byte
packet and base texture; it maintains no backend-specific wind/cutout/transmission math.

## GPU-driven Metal artifact

`GPUDriven.slang` now cross-compiles to `GPUDriven.metal` alongside SPIR-V and Windows DXIL when
`NEXORA_ENABLE_SLANG=ON`. It uses the same four canonical storage slots: input 0, visible instances
1, indirect commands 2 and statistics 3, with the shared 36-byte indirect-command ABI. It is a
separate single-thread compute kernel, preserving the triangle graphics artifact and baseline.
`build.shader_crosscompile` requires the nonempty compute Metal artifact on every host; native
macOS tests additionally compile and execute it. Set `NEXORA_SLANG_COMPUTE_METAL_PATH` to request
this artifact at device construction. Missing/invalid requested artifacts fail without falling back
to the embedded compute smoke. Linux generation does not certify Metal execution or performance.

## Loaded DXIL profile checks

The native RHI triangle's separate vertex/fragment artifacts use `sm_6_6`; the GPU-driven compute
artifact uses `sm_6_0`. D3D12 now reads actual entry-stage/profile metadata when loading requested
artifacts and compares each selected stage with a pipeline's requested DXIL minimum before PSO
allocation. Device support alone cannot admit newer-profile requirements against older bytecode.
An absent optional artifact still follows the existing FXC baseline when no DXIL minimum is
requested. Invalid or wrong-stage explicitly requested artifacts throw instead of silently falling
back. No profile/toolchain baseline changes here.

The portable reader bounds-checks container/program metadata and bitcode ranges; it does not
validate LLVM, signatures, compiler provenance or preview opt-in. Windows native CI inspects the
real compiled SM6.6/SM6.0 files and tests PSO admission; synthetic Linux metadata tests do not prove
SM6.9/6.10 feature execution. Full compiler/artifact readiness and advanced native gates stay open.
See the [RHI artifact contract](../Engine/RHI/README.md#dxil-artifact-stage-and-profile-admission).


## Bounded tessellation artifacts

With Slang enabled, Tessellation.slang includes the canonical triangle's frame/vertex interface,
then defines a three-point, factor4 bowed patch and a baked16-triangle fallback. The build validates
actual hull/domain entry reflection and64-byte fallback frame layout and generates
Tessellation.reflection.json with source/include/artifact hashes and profile/topology identity.
The fallback SPIR-V has no Tessellation capability. Ordinary Triangle/GPUDriven artifacts keep
their existing profiles and behavior.

| Environment variable | Artifact / entry |
| --- | --- |
| NEXORA_SLANG_TESSELLATION_SPIRV_PATH | Tessellation.spv; tessControlMain and tessEvaluationMain |
| NEXORA_SLANG_PRETESSELLATED_SPIRV_PATH | PreTessellated.spv; preTessellatedVertexMain |
| NEXORA_SLANG_TESSELLATION_CONTROL_DXIL_PATH | Tessellation.control.dxil; hull SM6.0 |
| NEXORA_SLANG_TESSELLATION_EVALUATION_DXIL_PATH | Tessellation.evaluation.dxil; domain SM6.0 |
| NEXORA_SLANG_PRETESSELLATED_DXIL_PATH | PreTessellated.dxil; vertex SM6.0 |
| NEXORA_SLANG_PRETESSELLATED_METAL_PATH | PreTessellated.metal; preTessellatedVertexMain / fragmentMain |

DXIL build outputs remain Windows-only with the existing downstream compiler setup. A separately
staged official Linux DXC can also validate these compiler outputs using Slang's
`-dxc-path <directory-containing-libdxcompiler.so>`; that optional probe is not a project dependency
and does not run D3D12. Metal emits/executes the baked fallback and does not claim native hull/domain
support. Requested missing/wrong-stage artifacts reject instead of substituting another program.

## Bounded mesh artifact

MeshTriangle.slang has a real meshMain at SM6.5. It reads GPUDriven.slang's GPU-produced visible
count from uint statistics word4 and emits zero or three vertices/one triangle in one mesh group.
The existing optional Slang build emits DXIL only on Windows, plus actual mesh entry/frame64-byte/
SRV0 reflection and a source/artifact identity manifest; ordinary Triangle is the cross-backend
raster fallback. Baseline profiles and external dependencies are unchanged. Actual selected stage/
profile admission precedes native PSO allocation. Compilation/reflection or fallback output does
not prove mesh execution, general meshlets, amplification, indirect mesh dispatch or Ultimate.

## Optional Work Graph node library

With NEXORA_ENABLE_D3D12_WORK_GRAPHS, SDK DXC directly compiles WorkGraphVisibleExpansion.hlsl at
lib_6_8. Slang's existing targets/profiles remain unchanged. One broadcasting producer fans out up
to eight consumer records, writing canonical five-word visible instances for the fixed bootstrap
recipe; GPUDriven.slang is the known-visible compute/indirect fallback. A separate testing-only
library has a real twelve-byte input ABI so native admission must reject it before publication.
The generated manifest records requested profile, compiler version and source/binary hashes without
claiming native execution. RHI separately checks actual library profile, native program/node names,
eight-byte entry ABI, output root signature and backing requirements. General graph libraries,
graphics nodes, SM6.9/6.10 and Runtime cook/program assembly remain outside this slice.

`BoundedRays.slang` is an independent optional Windows Slang recipe: a three-entry SM6.3 ray
library and an SM6.5 inline query. `RayReference.slang` supplies lower-SM actual compute traversal
(SPIR-V/MSL and Windows DXIL6.0). Baseline shader targets/profiles remain unchanged. Real library/
query reflection must match acceleration structure t0, RayInput32 at t1 and Hit16 at u1, including
field offsets. The build emits compiler/source/artifact provenance plus private exact-byte arrays
for runtime recipe identity; native execution remains explicitly pending in generated metadata.
A separately compiled same-profile changed recipe exercises runtime identity rejection. Native
state objects additionally require actual exports and shader identifiers; compilation/reflection
is not DXR proof. These fixtures do not implement DXR1.2, SER or general cooked ray authoring.

`SamplerFeedback.slang` independently compiles an optional Windows SM6.5 MIN_MIP write kernel;
`PackSamplerFeedback.slang` uses SM6.0 to widen the native4x4 R8_UINT decode on the GPU.
`SamplerFeedbackReference.slang` supplies actual lower-profile SPIR-V/MSL/DXIL compute reference.
The existing Slang option controls these outputs. Real reflection checks feedback mode, root and
16-byte requests/constants; private exact-byte arrays and compiler/source/artifact provenance
identify the fixed recipe. Precise float mode and safe Metal math preserve invalid UV rejection.
The paired64x64/seven-mip texture is metadata-only; these shaders do not sample its color data or
implement application texture streaming. Native clear/write/decode and enabled execution require
separate target-host acceptance; compilation/fallback output is not that proof.

## Optional DXC modern recipes

The default-off Windows preview build compiles `ModernLongVector.hlsl` as cs_6_9 and
`ModernLinearAlgebra.hlsl`/`ModernGroupShared.hlsl` as cs_6_10 using separately pinned DXC1.10
Preview and its bundled `dx/linalg.h`. It does not raise canonical Slang profiles. The artifact
manifest records compiler/source/binary identities and sets native execution false. An exact-byte
private header pins the three fixed native recipes. A real65540-byte negative compilation must
fail the declared65536 shared-memory ceiling. `GroupSharedLimit` describes static storage, not
runtime dynamic allocation. CPU/GPU output proof belongs to the separate native test, not this
compiler manifest. General modern shader cook/reflection authoring remains open.

The optional SER recipe compiles `ReorderedRays.hlsl` as DXC lib_6_9, exports three fixed ray stages
and uses16-byte qualified payload fields. Its build tool checks real dumpbin binding/offset/stride
reflection and actual HitObject TraceRay/Reorder/Invoke instructions, then pins exact bytes and a
same-profile changed-payload negative. The manifest does not claim execution or actual thread
reordering; OPTIONS22 and native output tests provide independent evidence.

`ShadingRateWitness.hlsl` is a dedicated optional preview-test VS/PS6.4 pair that encodes hardware
SV_ShadingRate into R/G bytes. It verifies actual tier2 image selection separately from gradient
quality comparisons and permits finer rates at primitive edges. Canonical Slang profiles stay
unchanged; compilation alone is not native rate-image proof.

## Baseline equivalents of fixed modern compute

LongVectorReference.slang, LinearAlgebraReference.slang and GroupSharedReference.slang compile under
the existing precise DXIL SM6.0, SPIR-V glsl450 and Metal SM6.0 profiles. All expose `computeMain`
with one thread/group, t0 input and u1 output. The vector kernel uses eight scalar integers, matrix
arithmetic uses ordinary F32 operations, and the reduction uses scalar accumulation without large
shared storage. These implement equivalent bounded results, not the modern instructions. Baseline
profiles and external dependencies do not change; GPU parity/canaries and target CI remain required.
