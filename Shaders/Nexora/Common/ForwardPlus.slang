implementing "Nexora.Common";

public float NexoraEvaluatePointLightAttenuation(float distance, float range)
{
  float normalized = saturate(1.0 - distance / max(range, 1.0e-4));
  return normalized * normalized;
}

public float NexoraEvaluateSpotLightAttenuation(float3 lightToPoint, float3 spotDirection,
                                                float innerCosine, float outerCosine)
{
  float cosine = dot(NexoraSafeNormalize(lightToPoint), NexoraSafeNormalize(spotDirection));
  return saturate((cosine - outerCosine) / max(innerCosine - outerCosine, 1.0e-4));
}

public uint NexoraSelectForwardPlusDepthSlice(float viewDepth, float nearPlane, float farPlane,
                                              uint sliceCount)
{
  uint safeCount = max(sliceCount, 1U);
  float normalized = saturate((log(max(viewDepth, nearPlane)) - log(max(nearPlane, 1.0e-4))) /
                              max(log(max(farPlane, nearPlane + 1.0e-4)) -
                                      log(max(nearPlane, 1.0e-4)),
                                  1.0e-4));
  return min(uint(normalized * float(safeCount)), safeCount - 1U);
}
