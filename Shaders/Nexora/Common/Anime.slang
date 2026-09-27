implementing "Nexora.Common";

public float NexoraEvaluateAnimeRamp(float nDotL, float rampOffset, float rampScale)
{
  return saturate(nDotL * rampScale + rampOffset);
}

public float NexoraEvaluateFaceSdfShadow(float faceLightSide, float sdfSample,
                                         float threshold, float softness)
{
  float orientedSdf = faceLightSide >= 0.0 ? sdfSample : 1.0 - sdfSample;
  float width = max(softness, 1.0e-4);
  return smoothstep(threshold - width, threshold + width, orientedSdf);
}

public float3 NexoraEvaluateAnimeDiffuse(float3 lightColor, float3 midColor, float3 shadowColor,
                                         float nDotL, float rampOffset, float rampScale)
{
  float ramp = NexoraEvaluateAnimeRamp(nDotL, rampOffset, rampScale);
  float3 lower = lerp(shadowColor, midColor, smoothstep(0.25, 0.55, ramp));
  return lerp(lower, lightColor, smoothstep(0.55, 0.85, ramp));
}

public float3 NexoraEvaluateAnimeHairHighlight(float3 baseColor, float3 highlightColor,
                                               float highlightMask, float flowAmount,
                                               float3 viewDirection, float3 tangent)
{
  float flow = pow(NexoraSaturateDot(viewDirection, NexoraSafeNormalize(tangent)),
                   max(flowAmount, 1.0));
  return lerp(baseColor, highlightColor, saturate(flow * highlightMask));
}

public float3 NexoraEvaluateAnimeRim(float3 rimColor, float3 normal, float3 viewDirection,
                                     float3 lightDirection, float rimPower, float rimMask,
                                     float intensity)
{
  float viewRim = pow(saturate(1.0 - NexoraSaturateDot(normal, viewDirection)),
                      max(rimPower, 1.0));
  float backlight = 1.0 - NexoraSaturateDot(normal, lightDirection);
  return rimColor * viewRim * backlight * saturate(rimMask) * max(intensity, 0.0);
}

public float3 NexoraExtrudeOutlinePosition(float3 position, float3 smoothedNormal, float width)
{
  return position + NexoraSafeNormalize(smoothedNormal) * max(width, 0.0);
}
