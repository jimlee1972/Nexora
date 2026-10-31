// Flat 2D linear scene/emission composition; deliberately independent from the 3D scene path.
import Nexora.Common;
struct TwoDTransform { float4 clip; uint4 style; float4 emission; float4 emissionUv; };
struct TwoDPointLight {
    float4 positionRadiusIntensity;
    float4 color;
    uint4 masks;
    float4 shadow;
};
struct TwoDShadowEdge { float4 points; uint4 masks; };
struct TwoDLightingUniform {
    float4 worldX;
    float4 worldY;
    float4 ambientStrength;
    uint4 counts;
    uint4 shadowLights[2];
    uint4 debug;
    float4 bloom;
    TwoDPointLight lights[32];
};
[[vk::binding(0, 1)]] ConstantBuffer<TwoDLightingUniform> lighting : register(b1);
#ifdef NEXORA_VULKAN
[[vk::push_constant]] ConstantBuffer<TwoDTransform> transform;
#else
[[vk::binding(0, 1)]] ConstantBuffer<TwoDTransform> transform : register(b0);
#endif
[[vk::binding(0, 0)]] Sampler2D<float4> colorTexture : register(t0);
[[vk::binding(0, 2)]] Sampler2D<float4> auxiliaryTexture : register(t1);
[[vk::binding(1, 1)]] StructuredBuffer<TwoDShadowEdge> shadowEdges : register(t5);
[[vk::binding(2, 1)]] StructuredBuffer<float> shadowDistances : register(t6);
[[vk::binding(3, 1)]] RWStructuredBuffer<float> shadowOutput : register(u7);

float twoDCross(float2 a, float2 b) { return a.x * b.y - a.y * b.x; }
[shader("compute")]
[numthreads(64, 1, 1)]
void twoDShadowMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= 1024 || id.y >= lighting.counts.y) return;
    uint lightIndex = lighting.shadowLights[id.y / 4][id.y % 4];
    TwoDPointLight light = lighting.lights[lightIndex];
    float angle = (float(id.x) + 0.5) * (6.283185307179586 / 1024.0);
    float2 direction = float2(cos(angle), sin(angle));
    float closest = light.positionRadiusIntensity.z;
    float offset = light.shadow.y;
    for (uint i = 0; i < lighting.counts.z; ++i) {
        TwoDShadowEdge edge = shadowEdges[i];
        if ((edge.masks.x & light.masks.x) == 0) continue;
        float2 a = edge.points.xy - light.positionRadiusIntensity.xy;
        float2 e = edge.points.zw - edge.points.xy;
        float denominator = twoDCross(direction, e);
        if (abs(denominator) <= 0.0000001 * length(e)) continue;
        float distance = twoDCross(a, e) / denominator;
        float segment = twoDCross(a, direction) / denominator;
        if (distance > offset && distance < closest && segment >= 0 && segment <= 1)
            closest = distance;
    }
    shadowOutput[id.y * 1024 + id.x] = closest;
}

float twoDVisibility(TwoDPointLight light, float2 relative, float distance) {
    if (light.masks.y == 0 || transform.style.z == 0 || distance == 0) return 1;
    float phase = atan2(relative.y, relative.x) / 6.283185307179586;
    phase -= floor(phase);
    float position = phase * 1024.0 - 0.5;
    int center = int(floor(position));
    float fraction = position - floor(position);
    float width = 1 + 4 * light.shadow.x;
    float visible = 0;
    float total = 0;
    for (int tap = -4; tap <= 4; ++tap) {
        float weight = max(0.0, 1 - abs(float(tap)) / width);
        if (weight == 0) continue;
        uint sample = uint(center + tap + 2048) & 1023;
        uint next = (sample + 1) & 1023;
        uint base = (light.masks.y - 1) * 1024;
        float first = distance <= shadowDistances[base + sample] + light.shadow.y ? 1.0 : 0.0;
        float second = distance <= shadowDistances[base + next] + light.shadow.y ? 1.0 : 0.0;
        visible += weight * lerp(first, second, fraction);
        total += weight;
    }
    return visible / total;
}

struct TwoDVertex {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
[shader("vertex")]
TwoDVertex twoDVertexMain(
    [[vk::location(0)]] float2 position : POSITION,
    [[vk::location(1)]] float2 uv : TEXCOORD0,
    [[vk::location(2)]] float4 color : COLOR0) {
    TwoDVertex output;
    output.position = float4(position * transform.clip.xy + transform.clip.zw, 0, 1);
    output.uv = uv;
    output.color = color;
    return output;
}
[shader("vertex")]
TwoDVertex twoDInstanceMain(
    [[vk::location(0)]] float2 origin : POSITION,
    [[vk::location(1)]] float2 axisX : TEXCOORD0,
    [[vk::location(2)]] float2 axisY : TEXCOORD1,
    [[vk::location(3)]] float4 uv : TEXCOORD2,
    [[vk::location(4)]] float4 color : COLOR0,
#ifdef NEXORA_VULKAN
    uint vertexID : SV_VulkanVertexID) {
#else
    uint vertexID : SV_VertexID) {
#endif
    static const float2 corners[6] = {
        float2(0,0), float2(1,0), float2(1,1),
        float2(0,0), float2(1,1), float2(0,1)
    };
    float2 corner = corners[vertexID];
    float2 position = origin + axisX * corner.x + axisY * corner.y;
    TwoDVertex output;
    output.position = float4(position * transform.clip.xy + transform.clip.zw, 0, 1);
    output.uv = lerp(uv.xy, uv.zw, corner);
    output.color = color;
    return output;
}
struct TwoDFragment {
    float4 scene : SV_Target0;
    float4 emission : SV_Target1;
};
[shader("fragment")]
TwoDFragment twoDFragmentMain(TwoDVertex input) {
    float4 color = colorTexture.Sample(input.uv) * input.color;
    TwoDFragment output;
    float3 illumination = float3(1);
    if (transform.style.y != 0) {
        float2 world = float2(dot(input.position.xy, lighting.worldX.xy) + lighting.worldX.z,
                              dot(input.position.xy, lighting.worldY.xy) + lighting.worldY.z);
        illumination = lighting.ambientStrength.rgb;
        for (uint i = 0; i < lighting.counts.x; ++i) {
            TwoDPointLight light = lighting.lights[i];
            if ((light.masks.x & transform.style.x) == 0) continue;
            float distanceToLight = length(world - light.positionRadiusIntensity.xy);
            float radius = light.positionRadiusIntensity.z;
            float attenuation = 0;
            // Exact centers remain valid even on GPUs that flush tiny radii to zero.
            if (distanceToLight == 0) attenuation = 1;
            else if (distanceToLight < radius) {
                float q = distanceToLight / radius;
                float smooth = 1 - q * q;
                attenuation = smooth * smooth;
            }
            if (attenuation > 0)
                attenuation *= twoDVisibility(light, world - light.positionRadiusIntensity.xy, distanceToLight);
            illumination += light.color.rgb * (light.positionRadiusIntensity.w * attenuation * lighting.ambientStrength.w);
        }
    }
    float3 emission = float3(0);
    if (transform.emission.w > 0) {
        float mask = auxiliaryTexture.Sample(input.uv * transform.emissionUv.xy + transform.emissionUv.zw).r;
        emission = min(transform.emission.rgb * (transform.emission.w * saturate(mask)), float3(65504));
    }
    output.scene = float4(clamp(color.rgb * illumination + emission * lighting.ambientStrength.w, float3(0), float3(65504)), saturate(color.a));
    // Every Sprite writes actual coverage in the same sorted MRT pass, even with zero emission.
    output.emission = float4(emission, saturate(color.a));
    return output;
}
[shader("fragment")]
float4 twoDCompositeMain(TwoDVertex input) : SV_Target0 {
    if (lighting.debug.x == 1) {
        uint slot = lighting.debug.y;
        if (slot >= lighting.counts.y) return float4(1, 0, 1, 1);
        uint lightIndex = lighting.shadowLights[slot / 4][slot % 4];
        float radius = lighting.lights[lightIndex].positionRadiusIntensity.z;
        uint angle = min(uint(saturate(input.uv.x * transform.emissionUv.x + transform.emissionUv.z) * 1024), 1023U);
        float distance = shadowDistances[slot * 1024 + angle];
        float normalized = radius > 0 ? saturate(distance / radius) : 0;
        // Diagnostic map displays native data directly, with only the output sRGB encoding.
        return float4(float3(normalized), 1);
    }
    float3 radiance = clamp(colorTexture.Sample(input.uv).rgb, float3(0), float3(65504));
    if (lighting.counts.w > 0 && lighting.bloom.x > 0 && lighting.ambientStrength.w > 0)
        radiance = min(radiance + auxiliaryTexture.Sample(input.uv).rgb *
                      (lighting.bloom.x * lighting.ambientStrength.w), float3(65504));
    // The sRGB attachment performs the sole output encoding; HUD is drawn afterwards.
    return float4(NexoraAcesApproximate(radiance), 1);
}

[shader("fragment")]
float4 twoDBloomMain(TwoDVertex input) : SV_Target0 {
    float2 step = transform.emission.xy;
    float3 result = float3(0);
    if (transform.style.w == 0) {
        result = (colorTexture.Sample(input.uv + step * float2(-0.5, -0.5)).rgb +
                  colorTexture.Sample(input.uv + step * float2( 0.5, -0.5)).rgb +
                  colorTexture.Sample(input.uv + step * float2(-0.5,  0.5)).rgb +
                  colorTexture.Sample(input.uv + step * float2( 0.5,  0.5)).rgb) * 0.25;
    } else {
        for (int y = -1; y <= 1; ++y)
            for (int x = -1; x <= 1; ++x) {
                float weight = float((x == 0 ? 2 : 1) * (y == 0 ? 2 : 1)) / 16.0;
                result += colorTexture.Sample(input.uv + step * float2(x, y)).rgb * weight;
            }
        result = (result + auxiliaryTexture.Sample(input.uv).rgb) * 0.5;
    }
    return float4(clamp(result, float3(0), float3(65504)), 1);
}
