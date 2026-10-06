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
  float4 properties;    // x metallic; y AO; z normal scale; w linear HDR output
  float4 environment;   // x intensity; y rotation radians; z maximum prefilter LOD; w reserved
  row_major float4x4 lightViewProjection;
  float4 shadowSettings; // normal bias, slope bias, inverse resolution (zero disables), UV Y sign
  float4 shadowTint; // w enables stylized tone
  float4 lightTint;
  float4 ramp; // offset, scale, softness
  float4 vegetation; // time, wind amplitude, alpha cutoff, transmission thickness
  float4 transmission; // linear RGB, unlit flag
  float4 reflection; // horizontal height, role, Fresnel F0, ellipse count
  float4 reflectionRegion0; // center x/z, radius x/z
  float4 reflectionRegion1;
  float4 transparency; // coverage, linear RGB transmission attenuation
  float4 worldMapping; // base/ORM triplanar repeats per world unit; shoreline variation; front-only refraction; two-sided lighting
  float4 atmosphereColor; // linear RGB, maximum haze strength
  float4 atmosphereRange; // start/end camera distance, two reserved
  float4 refraction; // index, slab thickness, inverse target width/height
  float4 pointPositionRadius;
  float4 pointRadiance; // xyz point radiance; w opts into thin dielectric refraction
};
[[vk::binding(4, 0)]] ConstantBuffer<MaterialConstants> material : register(b1);
[[vk::binding(0, 0)]] Sampler2D<float4> baseMap : register(t0);
[[vk::binding(1, 0)]] Sampler2D<float4> normalMap : register(t1);
[[vk::binding(2, 0)]] Sampler2D<float4> ormMap : register(t2);
[[vk::binding(3, 0)]] Sampler2D<float4> emissionMap : register(t3);
[[vk::binding(5, 0)]] Sampler2D<float4> diffuseEnvironment : register(t4);
[[vk::binding(6, 0)]] Sampler2D<float4> specularEnvironment : register(t5);
[[vk::binding(7, 0)]] Sampler2D<float4> brdfIntegration : register(t6);
[[vk::binding(8, 0)]] Sampler2D<float4> directionalShadow : register(t7);

[[vk::binding(9, 0)]] Sampler2D<float4> opaqueBackground : register(t8);

float2 environmentUv(float3 direction) {
  float angle = atan2(direction.z, direction.x) + material.environment.y;
  return float2(angle / (6.283185307179586) + 0.5,
                acos(clamp(direction.y, -1.0, 1.0)) / 3.141592653589793);
}

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
  bool reflected = material.reflection.y > 1.5;
  // Wind is evaluated at the original position before the deformed result is reflected.
  if (reflected) output.worldPosition.y = 2 * material.reflection.x - output.worldPosition.y;
  if (material.vegetation.y > 0)
    output.worldPosition += NexoraEvaluateVegetationWind(output.worldPosition, float3(1,0,0.35),
        material.vegetation.x, 1-saturate(input.uv.y), 1, 0.37) * material.vegetation.y;
  if (reflected) output.worldPosition.y = 2 * material.reflection.x - output.worldPosition.y;
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
  if (reflected) {
    output.normal.y = -output.normal.y;
    output.tangent.y = -output.tangent.y;
    output.tangent.w = -output.tangent.w;
  }
  output.uv = input.uv;
  output.color = input.color;
  return output;
}
// Share cutout sampling between visible and shadow passes, including world projection.
float3 worldMapWeights(float3 normal) {
  float3 weight = abs(NexoraSafeNormalize(normal));
  return weight / max(0.00001, weight.x + weight.y + weight.z);
}
float4 sampleBaseMap(VertexOutput input) {
  if (material.worldMapping.x <= 0) return baseMap.Sample(input.uv);
  float3 position = input.worldPosition * material.worldMapping.x;
  float3 weight = worldMapWeights(input.normal);
  return baseMap.Sample(frac(position.zy)) * weight.x +
      baseMap.Sample(frac(position.xz)) * weight.y +
      baseMap.Sample(frac(position.xy)) * weight.z;
}
// Interpret triplanar normal maps as bounded world-space surface slopes.
float2 normalMapSlope(float3 encoded) {
  float3 decoded = encoded * 2 - 1;
  return -decoded.xy / max(0.05, abs(decoded.z));
}
float3 sampleWorldNormal(float3 position, float3 normal, float3 weight) {
  if (material.properties.z == 0) return normal;
  float2 x = normalMapSlope(normalMap.Sample(frac(position.zy)).xyz);
  float2 y = normalMapSlope(normalMap.Sample(frac(position.xz)).xyz);
  float2 z = normalMapSlope(normalMap.Sample(frac(position.xy)).xyz);
  float3 gradient = float3(0, x.y, x.x)*weight.x + float3(y.x, 0, y.y)*weight.y +
      float3(z.x, z.y, 0)*weight.z;
  gradient = clamp(gradient, float3(-8), float3(8));
  gradient -= normal * dot(normal, gradient);
  return NexoraSafeNormalize(normal - gradient * material.properties.z);
}
[shader("fragment")]
float4 shadowFragmentMain(VertexOutput input) : SV_Target {
  if (material.vegetation.z > 0 &&
      NexoraEvaluateVegetationAlpha(sampleBaseMap(input).a, material.vegetation.z) == 0)
    discard;
  return float4(input.position.z, 0, 0, 1);
}
float shadowVisibility(float3 position, float3 normal, float3 light) {
  if (material.shadowSettings.z == 0) return 1;
  float4 projected = mul(material.lightViewProjection, float4(position, 1));
  if (projected.w <= 0) return 1;
  float3 ndc = projected.xyz / projected.w;
  float2 uv = ndc.xy * float2(0.5, 0.5 * material.shadowSettings.w) + 0.5;
  if (any(uv < 0) || any(uv > 1) || ndc.z < 0 || ndc.z > 1) return 1;
  float2 step = float2(material.shadowSettings.z * 0.5);
  float4 depths = float4(
    directionalShadow.SampleLevel(uv + float2(-step.x, -step.y), 0).r,
    directionalShadow.SampleLevel(uv + float2(step.x, -step.y), 0).r,
    directionalShadow.SampleLevel(uv + float2(-step.x, step.y), 0).r,
    directionalShadow.SampleLevel(uv + step, 0).r);
  float receiver = NexoraApplyShadowBias(ndc.z, material.shadowSettings.x,
      material.shadowSettings.y, saturate(dot(normal, light)));
  return NexoraEvaluatePcf4(receiver, depths, 0);
}
float4 outputSceneColor(float3 color, float3 worldPosition)
{
  if (material.atmosphereColor.w > 0 && material.transmission.w < 0.5) {
    float distance = length(worldPosition - material.cameraPosition.xyz);
    float amount = smoothstep(material.atmosphereRange.x, material.atmosphereRange.y, distance) *
        material.atmosphereColor.w;
    color = lerp(color, material.atmosphereColor.xyz, amount);
  }
  if (material.properties.w > 0.5)
    return float4(clamp(color * material.transparency.x, float3(0), float3(65504)),
        min(65504.0, length(worldPosition - material.cameraPosition.xyz)));
  color = NexoraAcesApproximate(color);
  if (material.cameraPosition.w > 0.5)
    color = NexoraLinearToSrgb(color);
  return float4(color, 1);
}
bool inReflectionRegion(float2 point, float4 region) {
  float2 offset = (point - region.xy) / max(region.zw, float2(0.00001));
  float angle = atan2(offset.y, offset.x);
  float edge = 1 - material.worldMapping.y * (0.5 + 0.3*sin(angle*7) + 0.2*sin(angle*11+0.9));
  return dot(offset, offset) <= edge*edge;
}
bool reflectionMask(float3 renderedPosition) {
  float3 ray = renderedPosition - material.cameraPosition.xyz;
  if (abs(ray.y) < 0.00001) return false;
  float distance = (material.reflection.x - material.cameraPosition.y) / ray.y;
  if (distance < 0 || distance > 1) return false;
  float2 point = material.cameraPosition.xz + ray.xz * distance;
  return inReflectionRegion(point, material.reflectionRegion0) ||
      (material.reflection.w > 1.5 && inReflectionRegion(point, material.reflectionRegion1));
}
float3 reflectionResponse(float3 color, float3 renderedPosition) {
  if (material.reflection.y < 1.5) return color;
  float3 ray = geometryDirection(material.cameraPosition.xyz - renderedPosition);
  float fresnel = material.reflection.z + (1-material.reflection.z) * pow(1-abs(ray.y), 5);
  return color * fresnel + float3(0.025, 0.03, 0.025) * (1-fresnel);
}
[shader("fragment")]
float4 pbrFragmentMain(VertexOutput input) : SV_Target
{
  float3 renderedPosition = input.worldPosition;
  bool reflected = material.reflection.y > 1.5;
  if (reflected) {
    if (renderedPosition.y >= material.reflection.x || !reflectionMask(renderedPosition)) discard;
    input.worldPosition.y = 2 * material.reflection.x - input.worldPosition.y;
  } else if (material.reflection.y > 0.5 &&
      input.worldPosition.y < material.reflection.x && reflectionMask(renderedPosition)) discard;
  float3 mapWeight = worldMapWeights(input.normal);
  float3 mapPosition = input.worldPosition * material.worldMapping.x;
  float4 baseSample = sampleBaseMap(input);
  if (material.vegetation.z > 0 &&
      NexoraEvaluateVegetationAlpha(baseSample.a, material.vegetation.z) == 0) discard;
  if (material.transmission.w > 0.5)
    return outputSceneColor(reflectionResponse(
        emissionMap.Sample(input.uv).rgb * material.emissionRoughness.xyz, renderedPosition), renderedPosition);
  float3 shadingEye = material.cameraPosition.xyz;
  if (reflected) shadingEye.y = 2 * material.reflection.x - shadingEye.y;
  float3 viewDirection = geometryDirection(shadingEye - input.worldPosition);
  float3 n = NexoraSafeNormalize(input.normal);
  if (material.worldMapping.z > 0.5 && dot(n, viewDirection) <= 0) discard;
  // Orient opt-in sheets before normal mapping, retaining a coherent tangent frame.
  if (material.worldMapping.w > 0.5 && dot(n, viewDirection) < 0) n = -n;
  float3 t = NexoraSafeNormalize(input.tangent.xyz - n * dot(n, input.tangent.xyz));
  float3 b = NexoraSafeNormalize(cross(n, t)) * input.tangent.w;
  float3 encoded = normalMap.Sample(input.uv).xyz;
  encoded.xy = (encoded.xy - 0.5) * material.properties.z + 0.5;
  float3 orm = ormMap.Sample(input.uv).xyz;
  if (material.worldMapping.x > 0)
    orm = ormMap.Sample(frac(mapPosition.zy)).xyz * mapWeight.x +
        ormMap.Sample(frac(mapPosition.xz)).xyz * mapWeight.y +
        ormMap.Sample(frac(mapPosition.xy)).xyz * mapWeight.z;
  NexoraPbrMaterial surface;
  surface.baseColor = material.pointRadiance.w > 0.5 ? float3(0) :
      baseSample.rgb * material.baseColor.rgb * input.color.rgb;
  surface.normal = material.worldMapping.x > 0 ? sampleWorldNormal(mapPosition, n, mapWeight) :
      NexoraDecodeTangentNormal(encoded, t, b, n);
  surface.metallic = saturate(orm.b * material.properties.x);
  surface.roughness = saturate(orm.g * material.emissionRoughness.w);
  surface.ao = saturate(orm.r * material.properties.y);
  surface.emission = emissionMap.Sample(input.uv).rgb * material.emissionRoughness.xyz;
  surface.alpha = 1;
  // A missing environment contributes zero; actual float IBL resources come from native bindings.
  NexoraPbrEnvironment environment;
  environment.diffuseIrradiance = float3(0, 0, 0);
  environment.prefilteredSpecular = float3(0, 0, 0);
  environment.brdfLut = float2(0, 0);
  if (material.environment.x > 0) {
    float3 n = geometryDirection(surface.normal);
    float roughness = saturate(surface.roughness);
    environment.diffuseIrradiance = diffuseEnvironment.SampleLevel(environmentUv(n), 0).rgb *
                                   material.environment.x;
    environment.prefilteredSpecular = specularEnvironment.SampleLevel(
        environmentUv(reflect(-viewDirection, n)), roughness * material.environment.z).rgb *
        material.environment.x;
    environment.brdfLut = brdfIntegration.SampleLevel(
        float2(saturate(dot(n, viewDirection)), roughness), 0).rg;
  }
  float3 light = geometryDirection(-scene.lightDirection.xyz);
  float visibility = shadowVisibility(input.worldPosition, geometryDirection(surface.normal), light);
  float3 direct = NexoraEvaluateDirectBrdf(surface.normal, viewDirection, light,
      surface.baseColor, surface.metallic, surface.roughness, scene.lightColor.rgb);
  float3 ambient = NexoraEvaluatePbrIbl(surface.normal, viewDirection, surface.baseColor,
      surface.metallic, surface.roughness, surface.ao, environment);
  if (material.shadowTint.w > 0.5) {
    float ramp = NexoraEvaluateStylizedRamp(saturate(dot(geometryDirection(surface.normal), light)),
        material.ramp.x, material.ramp.y, material.ramp.z);
    direct = NexoraApplyStylizedResponse(direct, ramp, material.shadowTint.rgb,
        material.lightTint.rgb, 1, 1);
    ambient = NexoraApplyShadowTint(ambient, visibility, material.shadowTint.rgb);
  }
  float3 transmitted = NexoraEvaluateVegetationTransmission(surface.normal, light,
      material.transmission.rgb * scene.lightColor.rgb, material.vegetation.w, 1-visibility);
  float3 local = float3(0);
  if (material.pointPositionRadius.w > 0) {
    float3 delta = material.pointPositionRadius.xyz - input.worldPosition;
    float distanceSquared = dot(delta, delta);
    float window = saturate(1 - distanceSquared /
        (material.pointPositionRadius.w * material.pointPositionRadius.w));
    float attenuation = window * window / max(0.25, distanceSquared);
    local = NexoraEvaluateDirectBrdf(surface.normal, viewDirection, geometryDirection(delta),
        surface.baseColor, surface.metallic, surface.roughness, material.pointRadiance.rgb * attenuation);
  }
  if (material.pointRadiance.w > 0.5) transmitted = float3(0);
  float3 color = direct * visibility + ambient + local + transmitted + max(surface.emission, float3(0));
  float4 result = outputSceneColor(reflectionResponse(color, renderedPosition), renderedPosition);
  if (material.refraction.x > 1 && material.refraction.y > 0) {
    float3 incident = -viewDirection;
    float3 faceNormal = dot(surface.normal, incident) > 0 ? -surface.normal : surface.normal;
    float3 bent = refract(incident, faceNormal, 1 / material.refraction.x);
    if (reflected) { bent.y = -bent.y; incident.y = -incident.y; }
    float4 bentClip = mul(scene.mvp, float4(renderedPosition + bent * material.refraction.y, 1));
    float4 straightClip = mul(scene.mvp, float4(renderedPosition + incident * material.refraction.y, 1));
    float2 shift = float2(0);
    if (bentClip.w > 0.0001 && straightClip.w > 0.0001)
      shift = (bentClip.xy / bentClip.w - straightClip.xy / straightClip.w) * float2(0.5, -0.5);
    float2 uv = input.position.xy * material.refraction.zw;
    // Clamp each axis to 24 pixels and avoid pulling nearer foreground geometry into glass.
    float2 limit = material.refraction.zw * 24;
    float2 target = clamp(uv + clamp(shift, -limit, limit), material.refraction.zw * 0.5,
        1 - material.refraction.zw * 0.5);
    float4 background = opaqueBackground.SampleLevel(target, 0);
    if (background.a + 0.02 < result.a) background = opaqueBackground.SampleLevel(uv, 0);
    if (material.pointRadiance.w > 0.5) {
      // One bounded dielectric interface: tint only the covered transmitted background.
      // Uncovered pixels retain their original background, rather than the shifted sample.
      float fresnel = NexoraEvaluateWaterFresnel(faceNormal, viewDirection, material.refraction.x);
      float3 uncovered = opaqueBackground.SampleLevel(uv, 0).rgb;
      result.rgb = clamp(result.rgb + background.rgb * material.transparency.yzw *
          material.transparency.x * (1-fresnel) + uncovered * (1-material.transparency.x),
          float3(0), float3(65504));
    } else {
      result.rgb = clamp(result.rgb + background.rgb * material.transparency.yzw *
          (1 - material.transparency.x), float3(0), float3(65504));
    }
  }
  return result;
}
