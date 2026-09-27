implementing "Nexora.Common";

public float4 NexoraSkinPosition(float4 position, float4 weights, float4x4 skin0,
                                 float4x4 skin1, float4x4 skin2, float4x4 skin3)
{
  float4 skinned = weights.x * mul(skin0, position) + weights.y * mul(skin1, position) +
                    weights.z * mul(skin2, position) + weights.w * mul(skin3, position);
  return skinned;
}

public float3 NexoraSkinNormal(float3 normal, float4 weights, float3x3 skin0,
                               float3x3 skin1, float3x3 skin2, float3x3 skin3)
{
  float3 skinned = weights.x * mul(skin0, normal) + weights.y * mul(skin1, normal) +
                   weights.z * mul(skin2, normal) + weights.w * mul(skin3, normal);
  return NexoraSafeNormalize(skinned);
}

public float4 NexoraTransformInstance(float3 position, float4x4 instanceTransform)
{
  return mul(instanceTransform, float4(position, 1.0));
}
