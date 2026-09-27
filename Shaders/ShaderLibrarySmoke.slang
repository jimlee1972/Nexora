import Nexora.Common;

[[vk::binding(0, 3)]]
StructuredBuffer<float4> forwardPlusLights : register(t0);
[[vk::binding(1, 3)]]
Texture2D<float> forwardPlusShadowMap : register(t1);
[[vk::binding(0, 4)]]
RWStructuredBuffer<uint> forwardPlusOutput : register(u4);
[[vk::binding(1, 4)]]
RWStructuredBuffer<uint> forwardPlusShadowOutput : register(u5);

[shader("compute")]
[numthreads(1, 1, 1)]
void shaderLibraryMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
  float3 normal = NexoraSafeNormalize(float3(0.0, 0.0, 1.0));
  float3 tangent = NexoraSafeNormalize(float3(1.0, 0.0, 0.0));
  float3 bitangent = NexoraSafeNormalize(float3(0.0, 1.0, 0.0));
  float3 mappedNormal = NexoraDecodeTangentNormal(float3(0.5, 0.5, 1.0), tangent, bitangent,
                                                   normal);

  NexoraPbrMaterial material;
  material.baseColor = float3(0.8, 0.4, 0.2);
  material.metallic = 0.1;
  material.roughness = 0.45;
  material.normal = mappedNormal;
  material.ao = 0.9;
  material.emission = float3(0.01, 0.01, 0.01);
  material.alpha = 1.0;

  NexoraPbrEnvironment environment;
  environment.diffuseIrradiance = float3(0.2, 0.25, 0.3);
  environment.prefilteredSpecular = float3(0.3, 0.35, 0.4);
  environment.brdfLut = float2(0.8, 0.04);

  float3 pbrF0 = NexoraComputePbrF0(material.baseColor, material.metallic);
  float3 pbrFresnel = NexoraFresnelSchlickRoughness(0.8, pbrF0, material.roughness);
  float3 pbrIbl = NexoraEvaluatePbrIbl(material.normal, normal, material.baseColor,
                                       material.metallic, material.roughness, material.ao,
                                       environment);
  float alphaTest = NexoraEvaluateAlphaTest(material.alpha, 0.5);
  float3 pbr = NexoraEvaluatePbr(material, normal, normal, float3(2.0, 2.0, 2.0), environment);
  float stylizedRamp = NexoraEvaluateStylizedRamp(0.8, 0.0, 1.0, 0.05);
  float3 stylized = NexoraEvaluateStylizedPbr(
      normal, normal, normal, material.baseColor, material.metallic, material.roughness,
      float3(1.0, 1.0, 1.0), float3(0.25, 0.3, 0.4), float3(1.0, 0.95, 0.9), 0.0, 1.0, 0.05);
  float3 stylizedResponse = NexoraApplyStylizedResponse(
      stylized, stylizedRamp, float3(0.25, 0.3, 0.4), float3(1.0, 0.95, 0.9), 1.0, 1.0);
  float faceShadow = NexoraEvaluateFaceSdfShadow(1.0, 0.75, 0.5, 0.1);
  float animeRamp = NexoraEvaluateAnimeRamp(0.8, 0.0, 1.0);
  float3 anime = NexoraEvaluateAnimeDiffuse(float3(1.0, 0.8, 0.7), float3(0.8, 0.6, 0.5),
                                             float3(0.3, 0.25, 0.3), 0.8, 0.0, 1.0);
  anime += NexoraEvaluateAnimeHairHighlight(material.baseColor, float3(1.0, 0.8, 0.5),
                                              0.75, 2.0, normal, tangent);
  anime += NexoraEvaluateAnimeRim(float3(0.3, 0.5, 1.0), normal, normal, normal, 2.0, 1.0, 1.0);
  float3 outlinePosition = NexoraExtrudeOutlinePosition(float3(0.0, 0.0, 0.0), normal, 0.01);

  float3 wind = NexoraEvaluateVegetationWind(float3(2.0, 3.0, 4.0), float3(1.0, 0.0, 0.0),
                                              1.0, 0.1, 0.8, 0.5);
  float vegetationAlpha = NexoraEvaluateVegetationAlpha(0.9, 0.5);
  float3 transmission = NexoraEvaluateVegetationTransmission(normal, normal,
                                                              float3(0.5, 0.8, 0.6), 0.7, 0.2);
  float waterFade = NexoraEvaluateWaterDepthFade(10.0, 9.0, 4.0);
  float3 waterBase = NexoraEvaluateWaterColor(float3(0.1, 0.4, 0.5), float3(0.0, 0.03, 0.08),
                                               waterFade);
  float waterFresnel = NexoraEvaluateWaterFresnel(normal, normal, 1.333);
  float3 waterReflection = NexoraEvaluateWaterReflection(float3(0.6, 0.7, 0.8), 0.5,
                                                          float3(0.2, 0.25, 0.3));
  float3 water = NexoraEvaluateWaterSurface(waterBase, float3(0.0, 0.03, 0.08),
                                             10.0, 9.0, 4.0, normal, normal, 1.333,
                                             waterReflection, 0.5, waterReflection);

  float receiverDepth = NexoraApplyShadowBias(0.5, 0.001, 0.002, 0.8);
  float visibility = NexoraEvaluatePcf4(receiverDepth, float4(0.4, 0.6, 0.55, 0.7), 0.001);
  uint shadowCascade = NexoraSelectShadowCascade(12.0, float4(8.0, 20.0, 45.0, 100.0), 4U);
  float3 shadowed = NexoraApplyShadowTint(pbr, visibility, float3(0.4, 0.45, 0.6));
  float ssao = NexoraEvaluateSsao(normal, normal, 0.8, 1.5);
  float3 post = NexoraApplyExposure(shadowed, 0.5);
  post = NexoraApplyBloom(post, float3(0.1, 0.05, 0.02), 0.4);
  post = NexoraApplyColorGrade(post, float3(0.0, 0.01, 0.0), float3(1.0, 0.98, 1.02), 1.05, 1.0);
  post = NexoraApplyDistanceFog(post, float3(0.2, 0.25, 0.35), 30.0, 0.02);
  post = NexoraEvaluateFxaa(post, post * 0.98, post * 1.02, post * 0.99, post * 1.01, 0.75);

  float4x4 identity = float4x4(1.0);
  float3x3 identityNormal = float3x3(1.0);
  float4 skinnedPosition = NexoraSkinPosition(float4(0.0, 0.0, 0.0, 1.0),
                                                float4(1.0, 0.0, 0.0, 0.0), identity, identity,
                                                identity, identity);
  float3 skinnedNormal = NexoraSkinNormal(normal, float4(1.0, 0.0, 0.0, 0.0), identityNormal,
                                           identityNormal, identityNormal, identityNormal);
  float4 instancePosition = NexoraTransformInstance(float3(0.0, 0.0, 0.0), identity);
  float attenuation = NexoraEvaluatePointLightAttenuation(2.0, 10.0);
  attenuation *= NexoraEvaluateSpotLightAttenuation(normal, normal, 0.9, 0.5);
  uint depthSlice = NexoraSelectForwardPlusDepthSlice(4.0, 0.1, 100.0, 16U);
  float4 lightRecord = forwardPlusLights[0];
  float sampledShadowDepth = forwardPlusShadowMap.Load(int3(0, 0, 0));
  float resourceShadowVisibility = NexoraEvaluatePcf4(
      lightRecord.w, sampledShadowDepth.xxxx, 0.001);
  float4 uiPosition = NexoraTransformUiPosition(float2(0.0, 0.0), float2(0.001, 0.001),
                                                 float2(-1.0, 1.0));
  float uiClip = NexoraEvaluateUiClip(float2(0.5, 0.5), float4(0.0, 0.0, 1.0, 1.0), 0.01);
  float2 nineSlice = NexoraEvaluateNineSliceUv(float2(0.25, 0.75), float2(128.0, 64.0),
                                               float2(256.0, 256.0), float4(16.0, 16.0, 16.0, 16.0));
  float4 worldUi = NexoraTransformWorldUiPosition(float2(0.5, 0.5), identity);
  uint variant = NexoraMakeShaderVariantKey(1U, 2U, 1U, 1U,
                                             NEXORA_FEATURE_PBR | NEXORA_FEATURE_IBL);
  float3 unlit = NexoraEvaluateUnlit(float3(0.2, 0.4, 0.8), 1.25);
  float4 unlitColor = NexoraEvaluateUnlitColor(float3(0.8, 0.7, 0.2), 0.75, 1.0);

  float3 result = post * ssao + stylized + stylizedResponse + anime + transmission + water + wind +
                  outlinePosition;
  result += pbrIbl + pbrFresnel + pbrF0 + float3(alphaTest, stylizedRamp, animeRamp);
  result += waterBase + waterReflection + float3(faceShadow, waterFade + waterFresnel, uiClip) +
            float3(nineSlice, worldUi.z);
  result *= vegetationAlpha;
  result += float3(attenuation, float(depthSlice + shadowCascade) * 0.001,
                   float(variant & 255U) * 0.0001);
  result += unlit + unlitColor.rgb;
  result += lightRecord.xyz * resourceShadowVisibility;
  result += skinnedPosition.xyz + skinnedNormal + instancePosition.xyz + uiPosition.xyz;
  result += NexoraCompositeUiColor(float4(1.0, 1.0, 1.0, 1.0),
                                   float4(1.0, 1.0, 1.0, 1.0),
                                   NexoraEvaluateUiClip(float2(0.5, 0.5),
                                                         float4(0.0, 0.0, 1.0, 1.0), 0.01)).rgb;
  if (dispatchThreadId.x == 0)
  {
    forwardPlusOutput[0] = uint(saturate(attenuation) * 65535.0);
    forwardPlusShadowOutput[0] = uint(saturate(resourceShadowVisibility) * 65535.0);
  }
}
