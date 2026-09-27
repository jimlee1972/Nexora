import Nexora.Common;

[shader("compute")]
[numthreads(1, 1, 1)]
void commonMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
  float3 normal = NexoraSafeNormalize(float3(0.0, 0.0, 1.0));
  float3 brdf = NexoraEvaluateDirectBrdf(normal, normal, normal, float3(0.8, 0.4, 0.2),
                                         0.1, 0.45, float3(2.0, 2.0, 2.0));
  float3 fresnel = NexoraFresnelSchlick(1.0, float3(0.04, 0.04, 0.04));
  float3 mapped = NexoraAcesApproximate(brdf + fresnel * 0.01);
  float3 srgb = NexoraLinearToSrgb(mapped);
  float luminance = NexoraRec709Luminance(NexoraSrgbToLinear(srgb));
  if (dispatchThreadId.x == 0 && luminance < 0.0)
    return;
}
