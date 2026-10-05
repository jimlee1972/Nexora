import Nexora.Common;

// Native adapters own these bounded tier-1 bindings. Lighting math stays shared.
struct SceneConstants
{
  row_major float4x4 mvp;
  float4 lightDirection;
  float4 lightColor;
  float4 legacyColor;
};
#ifdef NEXORA_VULKAN
[[vk::push_constant]] ConstantBuffer<SceneConstants> scene;
#else
[[vk::binding(0, 5)]] ConstantBuffer<SceneConstants> scene : register(b0);
#endif
struct MaterialConstants
{
  float4 cameraPosition; // w: manual sRGB output transfer for UNORM targets
  float4 baseColor;      // linear factors
  float4 emissionRoughness; // xyz linear emission factor; w perceptual roughness
  float4 properties;    // x metallic; y AO; z normal scale; w reserved
};
[[vk::binding(4, 0)]] ConstantBuffer<MaterialConstants> material : register(b1);
[[vk::binding(0, 0)]] Sampler2D<float4> baseMap : register(t0);
[[vk::binding(1, 0)]] Sampler2D<float4> normalMap : register(t1);
[[vk::binding(2, 0)]] Sampler2D<float4> ormMap : register(t2);
[[vk::binding(3, 0)]] Sampler2D<float4> emissionMap : register(t3);

struct VertexInput
{
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float2 uv : TEXCOORD0;
  [[vk::location(3)]] float4 model0 : INSTANCE_MODEL0;
  [[vk::location(4)]] float4 model1 : INSTANCE_MODEL1;
  [[vk::location(5)]] float4 model2 : INSTANCE_MODEL2;
  [[vk::location(6)]] float4 normal0 : INSTANCE_NORMAL0;
  [[vk::location(7)]] float4 normal1 : INSTANCE_NORMAL1;
  [[vk::location(8)]] float4 normal2 : INSTANCE_NORMAL2;
  [[vk::location(9)]] float4 color : INSTANCE_COLOR;
  [[vk::location(10)]] float4 tangent : TANGENT;
};
struct VertexOutput
{
  float4 position : SV_Position;
  float3 worldPosition : TEXCOORD0;
  float3 normal : TEXCOORD1;
  float4 tangent : TEXCOORD2;
  float2 uv : TEXCOORD3;
  float4 color : TEXCOORD4;
};
// Scale directions before shared normalization, preserving extreme affine inputs without overflow.
float3 geometryDirection(float3 value)
{
  float magnitude = max(max(abs(value.x), abs(value.y)), abs(value.z));
  return magnitude > 0 ? NexoraSafeNormalize(value / magnitude) : float3(0, 0, 0);
}
[shader("vertex")]
VertexOutput pbrVertexMain(VertexInput input)
{
  VertexOutput output;
  float4 p = float4(input.position, 1.0);
  output.worldPosition = float3(dot(input.model0, p), dot(input.model1, p), dot(input.model2, p));
  output.position = mul(scene.mvp, float4(output.worldPosition, 1.0));
#ifdef NEXORA_VULKAN
  output.position.y = -output.position.y;
#endif
  float3 n = geometryDirection(input.normal);
  output.normal = geometryDirection(float3(dot(input.normal0.xyz, n), dot(input.normal1.xyz, n),
                                             dot(input.normal2.xyz, n)));
  float modelMagnitude = max(max(max(abs(input.model0.x), abs(input.model0.y)), abs(input.model0.z)),
                             max(max(max(abs(input.model1.x), abs(input.model1.y)), abs(input.model1.z)),
                                 max(max(abs(input.model2.x), abs(input.model2.y)), abs(input.model2.z))));
  float3 t = float3(dot(input.model0.xyz / modelMagnitude, input.tangent.xyz),
                    dot(input.model1.xyz / modelMagnitude, input.tangent.xyz),
                    dot(input.model2.xyz / modelMagnitude, input.tangent.xyz));
  t = geometryDirection(t - output.normal * dot(output.normal, t));
  float handedness = input.normal0.w; // Exact CPU determinant sign, also for cancelling affine matrices.
  output.tangent = float4(t, input.tangent.w * handedness);
  output.uv = input.uv;
  output.color = input.color;
  return output;
}
[shader("fragment")]
float4 pbrFragmentMain(VertexOutput input) : SV_Target
{
  float3 n = NexoraSafeNormalize(input.normal);
  float3 t = NexoraSafeNormalize(input.tangent.xyz - n * dot(n, input.tangent.xyz));
  float3 b = NexoraSafeNormalize(cross(n, t)) * input.tangent.w;
  float3 encoded = normalMap.Sample(input.uv).xyz;
  encoded.xy = (encoded.xy - 0.5) * material.properties.z + 0.5;
  float3 orm = ormMap.Sample(input.uv).xyz;
  NexoraPbrMaterial surface;
  surface.baseColor = baseMap.Sample(input.uv).rgb * material.baseColor.rgb * input.color.rgb;
  surface.normal = NexoraDecodeTangentNormal(encoded, t, b, n);
  surface.metallic = saturate(orm.b * material.properties.x);
  surface.roughness = max(0.05, saturate(orm.g * material.emissionRoughness.w));
  surface.ao = saturate(orm.r * material.properties.y);
  surface.emission = emissionMap.Sample(input.uv).rgb * material.emissionRoughness.xyz;
  surface.alpha = 1;
  // Direct-light integration slice: no environment sample is fabricated as IBL evidence.
  NexoraPbrEnvironment environment;
  environment.diffuseIrradiance = float3(0, 0, 0);
  environment.prefilteredSpecular = float3(0, 0, 0);
  environment.brdfLut = float2(0, 0);
  float3 color = NexoraEvaluatePbr(surface, material.cameraPosition.xyz - input.worldPosition,
                                 -scene.lightDirection.xyz, scene.lightColor.rgb, environment);
  color = NexoraAcesApproximate(color);
  if (material.cameraPosition.w > 0.5)
    color = NexoraLinearToSrgb(color);
  return float4(color, 1);
}
