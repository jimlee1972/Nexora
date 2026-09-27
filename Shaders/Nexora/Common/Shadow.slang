implementing "Nexora.Common";

public float NexoraApplyShadowBias(float receiverDepth, float normalBias, float slopeBias,
                                   float nDotL)
{
  float slope = 1.0 - saturate(nDotL);
  return receiverDepth - max(normalBias, 0.0) - slope * max(slopeBias, 0.0);
}

public float NexoraEvaluatePcf4(float receiverDepth, float4 shadowDepths, float bias)
{
  float4 comparisons = step((receiverDepth - max(bias, 0.0)).xxxx, shadowDepths);
  return dot(comparisons, float4(0.25, 0.25, 0.25, 0.25));
}

public uint NexoraSelectShadowCascade(float viewDepth, float4 cascadeSplits, uint cascadeCount)
{
  uint count = min(max(cascadeCount, 1U), 4U);
  uint cascade = 0U;
  if (count > 1U && viewDepth > cascadeSplits.x)
    cascade = 1U;
  if (count > 2U && viewDepth > cascadeSplits.y)
    cascade = 2U;
  if (count > 3U && viewDepth > cascadeSplits.z)
    cascade = 3U;
  return min(cascade, count - 1U);
}

public float3 NexoraApplyShadowTint(float3 lighting, float shadowVisibility,
                                    float3 shadowTint)
{
  return lighting * lerp(saturate(shadowTint), float3(1.0, 1.0, 1.0),
                         saturate(shadowVisibility));
}
