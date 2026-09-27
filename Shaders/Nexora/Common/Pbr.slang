implementing "Nexora.Common";

public struct NexoraPbrMaterial
{
  public float3 baseColor;
  public float metallic;
  public float roughness;
  public float3 normal;
  public float ao;
  public float3 emission;
  public float alpha;
};

public struct NexoraPbrEnvironment
{
  public float3 diffuseIrradiance;
  public float3 prefilteredSpecular;
  public float2 brdfLut;
};

public float3 NexoraDecodeTangentNormal(float3 encodedNormal, float3 tangent, float3 bitangent,
                                        float3 geometricNormal)
{
  float3 tangentNormal = encodedNormal * 2.0 - 1.0;
  float3 t = NexoraSafeNormalize(tangent);
  float3 b = NexoraSafeNormalize(bitangent);
  float3 n = NexoraSafeNormalize(geometricNormal);
  return NexoraSafeNormalize(tangentNormal.x * t + tangentNormal.y * b + tangentNormal.z * n);
}

public float3 NexoraComputePbrF0(float3 baseColor, float metallic)
{
  float metal = saturate(metallic);
  return lerp(float3(0.04, 0.04, 0.04), saturate(baseColor), metal);
}

public float3 NexoraFresnelSchlickRoughness(float nDotV, float3 f0, float roughness)
{
  float3 grazing = max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), f0);
  float factor = pow(saturate(1.0 - nDotV), 5.0);
  return f0 + (grazing - f0) * factor;
}

public float3 NexoraEvaluatePbrIbl(float3 normal, float3 viewDirection, float3 baseColor,
                                   float metallic, float roughness, float ao,
                                   NexoraPbrEnvironment environment)
{
  float3 n = NexoraSafeNormalize(normal);
  float3 v = NexoraSafeNormalize(viewDirection);
  float nDotV = NexoraSaturateDot(n, v);
  float metal = saturate(metallic);
  float3 albedo = saturate(baseColor);
  float3 f0 = NexoraComputePbrF0(albedo, metal);
  float3 fresnel = NexoraFresnelSchlickRoughness(nDotV, f0, saturate(roughness));
  float3 diffuseWeight = (1.0 - fresnel) * (1.0 - metal);
  float3 diffuse = environment.diffuseIrradiance * albedo * diffuseWeight * saturate(ao) /
                   kNexoraPi;
  float3 specular = environment.prefilteredSpecular *
                    (fresnel * environment.brdfLut.x + environment.brdfLut.y) * saturate(ao);
  return diffuse + specular;
}

public float3 NexoraEvaluatePbr(NexoraPbrMaterial material, float3 viewDirection,
                               float3 lightDirection, float3 directRadiance,
                               NexoraPbrEnvironment environment)
{
  float3 direct = NexoraEvaluateDirectBrdf(material.normal, viewDirection, lightDirection,
                                            material.baseColor, material.metallic,
                                            material.roughness, directRadiance);
  float3 ambient = NexoraEvaluatePbrIbl(material.normal, viewDirection, material.baseColor,
                                         material.metallic, material.roughness, material.ao,
                                         environment);
  return direct + ambient + max(material.emission, 0.0);
}

public float NexoraEvaluateAlphaTest(float alpha, float cutoff)
{
  return step(saturate(cutoff), saturate(alpha));
}
