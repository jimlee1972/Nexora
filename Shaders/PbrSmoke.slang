import Nexora.Common;

struct PbrFrame
{
  float4x4 viewProjection;
  float3 cameraPosition;
  float maxEnvironmentMip;
};

[[vk::binding(0, 0)]]
ConstantBuffer<PbrFrame> pbrFrame : register(b0);
[[vk::binding(0, 1)]]
Texture2D<float4> pbrBaseColorTexture : register(t0);
[[vk::binding(1, 1)]]
Texture2D<float4> pbrNormalTexture : register(t1);
[[vk::binding(2, 1)]]
Texture2D<float4> pbrOrmTexture : register(t2);
[[vk::binding(3, 1)]]
Texture2D<float4> pbrEmissionTexture : register(t3);
[[vk::binding(4, 1)]]
TextureCube<float4> pbrDiffuseIrradiance : register(t4);
[[vk::binding(5, 1)]]
TextureCube<float4> pbrPrefilteredSpecular : register(t5);
[[vk::binding(6, 1)]]
Texture2D<float4> pbrBrdfLut : register(t6);
[[vk::binding(7, 1)]]
Texture2DArray<float4> pbrTextureArray : register(t7);
[[vk::binding(0, 2)]]
SamplerState pbrSampler : register(s0);

struct PbrVertexInput
{
  float3 position : POSITION;
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 uv : TEXCOORD0;
};

struct PbrVertexOutput
{
  float4 position : SV_Position;
  float3 worldPosition : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float3 tangent : TEXCOORD2;
  float3 bitangent : TEXCOORD3;
  float2 uv : TEXCOORD4;
};

[shader("vertex")]
PbrVertexOutput pbrVertexMain(PbrVertexInput input)
{
  PbrVertexOutput output;
  output.position = mul(pbrFrame.viewProjection, float4(input.position, 1.0));
  output.worldPosition = input.position;
  output.normal = NexoraSafeNormalize(input.normal);
  output.tangent = NexoraSafeNormalize(input.tangent.xyz);
  output.bitangent = NexoraSafeNormalize(cross(output.normal, output.tangent) * input.tangent.w);
  output.uv = input.uv;
  return output;
}

[shader("fragment")]
float4 pbrFragmentMain(PbrVertexOutput input) : SV_Target
{
  float4 baseSample = pbrBaseColorTexture.Sample(pbrSampler, input.uv);
  float3 encodedNormal = pbrNormalTexture.Sample(pbrSampler, input.uv).xyz;
  float3 orm = pbrOrmTexture.Sample(pbrSampler, input.uv).xyz;
  float4 arraySample = pbrTextureArray.Sample(pbrSampler, float3(input.uv, 0.0));
  float3 normal = NexoraDecodeTangentNormal(encodedNormal, input.tangent, input.bitangent,
                                             input.normal);
  float3 viewDirection = NexoraSafeNormalize(pbrFrame.cameraPosition - input.worldPosition);
  float3 reflectionDirection = reflect(-viewDirection, normal);
  NexoraPbrEnvironment environment;
  environment.diffuseIrradiance = pbrDiffuseIrradiance.Sample(pbrSampler, normal).rgb;
  environment.prefilteredSpecular = pbrPrefilteredSpecular.SampleLevel(
      pbrSampler, reflectionDirection, saturate(orm.g) * max(pbrFrame.maxEnvironmentMip, 0.0)).rgb;
  environment.brdfLut = pbrBrdfLut.Sample(pbrSampler,
                                          float2(NexoraSaturateDot(normal, viewDirection), orm.g)).rg;
  NexoraPbrMaterial material;
  material.baseColor = baseSample.rgb * arraySample.rgb;
  material.metallic = orm.b;
  material.roughness = max(orm.g, 0.05);
  material.normal = normal;
  material.ao = orm.r;
  material.emission = pbrEmissionTexture.Sample(pbrSampler, input.uv).rgb;
  material.alpha = baseSample.a;
  float3 color = NexoraEvaluatePbr(material, viewDirection, normal,
                                   float3(1.0, 1.0, 1.0), environment);
  return float4(NexoraAcesApproximate(color), material.alpha);
}
