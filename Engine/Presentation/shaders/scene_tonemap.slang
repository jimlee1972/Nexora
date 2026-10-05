import Nexora.Common;
struct ToneParameters { float4 settings; }; // x exposure; y manual output transfer
#ifdef NEXORA_VULKAN
[[vk::push_constant]] ConstantBuffer<ToneParameters> tone;
#else
[[vk::binding(0, 5)]] ConstantBuffer<ToneParameters> tone : register(b0);
#endif
[[vk::binding(0, 0)]] Sampler2D<float4> hdrScene : register(t0);
struct ToneVertex { float4 position : SV_Position; float2 uv : TEXCOORD0; };
[shader("vertex")]
ToneVertex toneVertexMain(
  [[vk::location(0)]] float2 position : POSITION,
  [[vk::location(1)]] float2 uv : TEXCOORD0) {
  ToneVertex output;
  output.position = float4(position, 0, 1);
#ifdef NEXORA_VULKAN
  output.uv = float2(uv.x, 1 - uv.y);
#else
  output.uv = uv;
#endif
  return output;
}
[shader("fragment")]
float4 toneFragmentMain(ToneVertex input) : SV_Target {
  float3 color = NexoraAcesApproximate(hdrScene.SampleLevel(input.uv, 0).rgb * tone.settings.x);
  if (tone.settings.y > 0.5) color = NexoraLinearToSrgb(color);
  return float4(color, 1);
}
