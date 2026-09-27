implementing "Nexora.Common";

public float3 NexoraEvaluateUnlit(float3 color, float intensity)
{
  return max(color, 0.0) * max(intensity, 0.0);
}

public float4 NexoraEvaluateUnlitColor(float3 color, float alpha, float intensity)
{
  return float4(NexoraEvaluateUnlit(color, intensity), saturate(alpha));
}
