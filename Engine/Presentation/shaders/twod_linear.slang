// Flat 2D linear scene/emission composition; deliberately independent from the 3D scene path.
import Nexora.Common;
struct TwoDTransform { float4 clip; };
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
    output.scene = float4(clamp(color.rgb, float3(0), float3(65504)), saturate(color.a));
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
