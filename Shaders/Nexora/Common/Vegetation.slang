implementing "Nexora.Common";

float NexoraVegetationNoise(float3 position)
{
  return frac(sin(dot(position, float3(12.9898, 78.233, 37.719))) * 43758.5453);
}

public float3 NexoraEvaluateVegetationWind(float3 worldPosition, float3 windDirection,
                                           float time, float rootWeight, float bendWeight,
                                           float variation)
{
  float phase = dot(worldPosition, float3(0.071, 0.113, 0.173)) + time;
  float noise = NexoraVegetationNoise(worldPosition + variation.xxx);
  float wave = sin(phase + noise * 6.2831853);
  float3 direction = NexoraSafeNormalize(windDirection);
  return direction * wave * saturate(bendWeight) * (1.0 - saturate(rootWeight));
}

public float NexoraEvaluateVegetationAlpha(float alpha, float cutoff)
{
  return step(saturate(cutoff), saturate(alpha));
}

public float3 NexoraEvaluateVegetationTransmission(float3 normal, float3 lightDirection,
                                                   float3 transmissionColor, float thickness,
                                                   float shadowFactor)
{
  float backLight = saturate(dot(-NexoraSafeNormalize(lightDirection),
                                 NexoraSafeNormalize(normal)));
  float transmission = backLight * max(thickness, 0.0) * (1.0 - saturate(shadowFactor));
  return transmissionColor * transmission;
}
