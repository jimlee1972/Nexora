// Flat 2D linear scene/emission composition; deliberately independent from the 3D scene path.
import Nexora.Common;
struct TwoDTransform { float4 clip; uint4 style; };
struct TwoDPointLight {
    float4 positionRadiusIntensity;
    float4 color;
    uint4 masks;
};
struct TwoDLightingUniform {
    float4 worldX;
    float4 worldY;
    float4 ambientStrength;
    uint4 counts;
    TwoDPointLight lights[32];
};
[[vk::binding(0, 1)]] ConstantBuffer<TwoDLightingUniform> lighting : register(b1);
#ifdef NEXORA_VULKAN
[[vk::push_constant]] ConstantBuffer<TwoDTransform> transform;
#else
[[vk::binding(0, 1)]] ConstantBuffer<TwoDTransform> transform : register(b0);
#endif
[[vk::binding(0, 0)]] Sampler2D<float4> colorTexture : register(t0);
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
            illumination += light.color.rgb * (light.positionRadiusIntensity.w * attenuation * lighting.ambientStrength.w);
        }
    }
    output.scene = float4(clamp(color.rgb * illumination, float3(0), float3(65504)), saturate(color.a));
    // An ordinary Sprite occludes prior emission according to its actual coverage.
    output.emission = float4(0, 0, 0, saturate(color.a));
    return output;
}
[shader("fragment")]
float4 twoDCompositeMain(TwoDVertex input) : SV_Target0 {
    float3 radiance = clamp(colorTexture.Sample(input.uv).rgb, float3(0), float3(65504));
    // The sRGB attachment performs the sole output encoding; HUD is drawn afterwards.
    return float4(NexoraAcesApproximate(radiance), 1);
}
