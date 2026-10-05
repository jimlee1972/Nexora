implementing "Nexora.Common";

public float NexoraEvaluateSsao(float3 normal, float3 viewDirection, float occlusionSample,
                                float power)
{
  float facing = NexoraSaturateDot(normal, viewDirection);
  return pow(saturate(occlusionSample * lerp(0.75, 1.0, facing)), max(power, 1.0e-4));
}

public float3 NexoraApplyExposure(float3 hdrColor, float exposure)
{
  return max(hdrColor, 0.0) * exp2(exposure);
}

// Preserve hue while rejecting radiance below the linear HDR threshold.
public float3 NexoraExtractBloom(float3 hdrColor, float threshold)
{
  float3 color = max(hdrColor, 0.0);
  float peak = max(max(color.x, color.y), color.z);
  return color * max(peak - max(threshold, 0.0), 0.0) / max(peak, 1.0e-4);
}

public float3 NexoraApplyBloom(float3 hdrColor, float3 bloomColor, float intensity)
{
  return max(hdrColor, 0.0) + max(bloomColor, 0.0) * max(intensity, 0.0);
}

public float3 NexoraApplyColorGrade(float3 color, float3 lift, float3 gain,
                                    float saturation, float contrast)
{
  float3 graded = max(color + lift, 0.0) * max(gain, 0.0);
  float luminance = NexoraRec709Luminance(graded);
  graded = lerp(float3(luminance, luminance, luminance), graded, saturation);
  return max((graded - 0.5) * contrast + 0.5, 0.0);
}

public float3 NexoraApplyDistanceFog(float3 color, float3 fogColor, float distance,
                                     float density)
{
  float visibility = exp(-max(distance, 0.0) * max(density, 0.0));
  return lerp(fogColor, color, saturate(visibility));
}

public float3 NexoraEvaluateFxaa(float3 center, float3 north, float3 south,
                                 float3 east, float3 west, float subpixelQuality)
{
  float3 average = (north + south + east + west) * 0.25;
  float edge = length(center - average);
  float blend = saturate(edge * max(subpixelQuality, 0.0));
  return lerp(center, average, blend);
}
