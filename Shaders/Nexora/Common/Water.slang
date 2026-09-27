implementing "Nexora.Common";

public float NexoraEvaluateWaterDepthFade(float sceneDepth, float surfaceDepth, float fadeDistance)
{
  float distance = max(sceneDepth - surfaceDepth, 0.0);
  return saturate(distance / max(fadeDistance, 1.0e-4));
}

public float3 NexoraEvaluateWaterColor(float3 shallowColor, float3 deepColor, float depthFade)
{
  return lerp(shallowColor, deepColor, saturate(depthFade));
}

public float NexoraEvaluateWaterFresnel(float3 normal, float3 viewDirection, float ior)
{
  float eta = max(ior, 1.0001);
  float f0 = (eta - 1.0) / (eta + 1.0);
  f0 *= f0;
  return f0 + (1.0 - f0) * pow(1.0 - NexoraSaturateDot(normal, viewDirection), 5.0);
}

public float3 NexoraEvaluateWaterReflection(float3 ssrColor, float ssrWeight,
                                            float3 environmentColor)
{
  return lerp(environmentColor, ssrColor, saturate(ssrWeight));
}

public float3 NexoraEvaluateWaterSurface(float3 shallowColor, float3 deepColor,
                                         float sceneDepth, float surfaceDepth, float fadeDistance,
                                         float3 normal, float3 viewDirection, float ior,
                                         float3 ssrColor, float ssrWeight,
                                         float3 environmentColor)
{
  float depthFade = NexoraEvaluateWaterDepthFade(sceneDepth, surfaceDepth, fadeDistance);
  float3 base = NexoraEvaluateWaterColor(shallowColor, deepColor, depthFade);
  float3 reflection = NexoraEvaluateWaterReflection(ssrColor, ssrWeight, environmentColor);
  return lerp(base, reflection, NexoraEvaluateWaterFresnel(normal, viewDirection, ior));
}
