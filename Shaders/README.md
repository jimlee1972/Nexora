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
