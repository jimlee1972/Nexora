implementing "Nexora.Common";

static const float kNexoraPi = 3.14159265359;

float NexoraDistributionGgx(float nDotH, float roughness)
{
  float alpha = max(roughness * roughness, 0.0025);
  float alphaSquared = alpha * alpha;
  float denominator = nDotH * nDotH * (alphaSquared - 1.0) + 1.0;
  return alphaSquared / max(kNexoraPi * denominator * denominator, 1.0e-7);
}

float NexoraGeometrySchlickGgx(float nDotV, float roughness)
{
  float k = (roughness + 1.0) * (roughness + 1.0) * 0.125;
  return nDotV / max(nDotV * (1.0 - k) + k, 1.0e-6);
}

public float3 NexoraFresnelSchlick(float vDotH, float3 reflectanceAtNormal)
{
  float factor = pow(saturate(1.0 - vDotH), 5.0);
  return reflectanceAtNormal + (1.0 - reflectanceAtNormal) * factor;
}

// Cook-Torrance direct-light BRDF. Inputs are linear-space colors and normalized directions.
public float3 NexoraEvaluateDirectBrdf(float3 normal, float3 viewDirection,
                                       float3 lightDirection, float3 albedo,
                                       float metallic, float roughness, float3 radiance)
{
  float3 n = NexoraSafeNormalize(normal);
  float3 v = NexoraSafeNormalize(viewDirection);
  float3 l = NexoraSafeNormalize(lightDirection);
  float3 h = NexoraSafeNormalize(v + l);
  float nDotV = NexoraSaturateDot(n, v);
  float nDotL = NexoraSaturateDot(n, l);
  float nDotH = NexoraSaturateDot(n, h);
  float vDotH = NexoraSaturateDot(v, h);
  if (nDotV <= 0.0 || nDotL <= 0.0)
    return float3(0.0, 0.0, 0.0);

  float metal = saturate(metallic);
  float rough = clamp(roughness, 0.05, 1.0);
  float3 baseColor = saturate(albedo);
  float3 f0 = lerp(float3(0.04, 0.04, 0.04), baseColor, metal);
  float3 fresnel = NexoraFresnelSchlick(vDotH, f0);
  float distribution = NexoraDistributionGgx(nDotH, rough);
  float geometry = NexoraGeometrySchlickGgx(nDotV, rough) *
                   NexoraGeometrySchlickGgx(nDotL, rough);
  float3 specular = distribution * geometry * fresnel /
                    max(4.0 * nDotV * nDotL, 1.0e-6);
  float3 diffuse = (1.0 - fresnel) * (1.0 - metal) * baseColor / kNexoraPi;
  return (diffuse + specular) * radiance * nDotL;
}
