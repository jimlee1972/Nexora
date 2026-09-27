implementing "Nexora.Common";

public float NexoraEvaluateStylizedRamp(float nDotL, float offset, float scale, float softness)
{
  float remapped = saturate(nDotL * scale + offset);
  float width = max(softness, 1.0e-4);
  return smoothstep(0.5 - width, 0.5 + width, remapped);
}

public float3 NexoraApplyStylizedResponse(float3 lighting, float shadowFactor,
                                          float3 shadowTint, float3 lightTint,
                                          float contrast, float saturation)
{
  float3 tinted = lighting * lerp(shadowTint, lightTint, saturate(shadowFactor));
  float luminance = NexoraRec709Luminance(max(tinted, 0.0));
  float3 saturated = lerp(float3(luminance, luminance, luminance), tinted, saturation);
  return max((saturated - 0.5) * contrast + 0.5, 0.0);
}

public float3 NexoraEvaluateStylizedPbr(float3 normal, float3 viewDirection,
                                        float3 lightDirection, float3 baseColor,
                                        float metallic, float roughness, float3 radiance,
                                        float3 shadowTint, float3 lightTint,
                                        float rampOffset, float rampScale, float rampSoftness)
{
  float3 lighting = NexoraEvaluateDirectBrdf(normal, viewDirection, lightDirection, baseColor,
                                              metallic, roughness, radiance);
  float ramp = NexoraEvaluateStylizedRamp(NexoraSaturateDot(normal, lightDirection),
                                           rampOffset, rampScale, rampSoftness);
  return NexoraApplyStylizedResponse(lighting, ramp, shadowTint, lightTint, 1.0, 1.0);
}
