# Shared Slang shader library

`Nexora/Common.slang` is the importable public module. Helper implementation files are grouped
under `Nexora/Common/` and are compiled into the same Slang module. Import it with:

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

`NexoraEvaluateDirectBrdf` expects normalized directions, linear albedo/radiance, metallic in
`[0, 1]`, and perceptual roughness in `[0, 1]`. It returns direct diffuse plus specular lighting;
it does not include ambient/IBL, shadows, normal-map decoding, material packing, or exposure.

`CommonSmoke.slang` imports the module and exercises each public helper. When
`NEXORA_ENABLE_SLANG=ON`, the existing shader target cross-compiles this smoke shader to SPIR-V and
Metal source, plus DXIL on Windows. The regular `build.shader_contract` also checks the module's
public surface and smoke-shader coverage when Slang is disabled.
