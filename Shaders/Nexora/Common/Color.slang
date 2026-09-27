implementing "Nexora.Common";

public float3 NexoraSrgbToLinear(float3 srgbColor)
{
  float3 low = srgbColor / 12.92;
  float3 nonnegativeColor = max(srgbColor, 0.0);
  float3 high = pow((nonnegativeColor + 0.055) / 1.055, 2.4);
  return lerp(low, high, step(0.04045, srgbColor));
}

public float3 NexoraLinearToSrgb(float3 linearColor)
{
  float3 color = max(linearColor, 0.0);
  float3 low = color * 12.92;
  float3 high = 1.055 * pow(color, 1.0 / 2.4) - 0.055;
  return lerp(low, high, step(0.0031308, color));
}

public float NexoraRec709Luminance(float3 linearColor)
{
  return dot(linearColor, float3(0.2126, 0.7152, 0.0722));
}

public float3 NexoraAcesApproximate(float3 hdrColor)
{
  float3 color = max(hdrColor, 0.0);
  return saturate((color * (2.51 * color + 0.03)) /
                  (color * (2.43 * color + 0.59) + 0.14));
}
