implementing Nexora.Common;

public float3 NexoraSafeNormalize(float3 value)
{
  float lengthSquared = dot(value, value);
  return lengthSquared > 1.0e-12 ? value * rsqrt(lengthSquared) : float3(0.0, 0.0, 0.0);
}

public float NexoraSaturateDot(float3 left, float3 right)
{
  return saturate(dot(left, right));
}
