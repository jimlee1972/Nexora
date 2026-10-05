import Nexora.Common;
struct ToneParameters { float4 settings; float4 bloom; }; // x exposure; y manual output transfer
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
  float3 hdr = hdrScene.SampleLevel(input.uv, 0).rgb;
  if (tone.bloom.x > 0) {
    uint width, height;
    hdrScene.GetDimensions(width, height);
    float2 texel = tone.bloom.z / float2(width, height);
    // Fixed bounded twelve-tap two-scale bloom. No temporal history or CPU readback.
    const float2 offsets[12] = {float2(-1,0),float2(1,0),float2(0,-1),float2(0,1),
      float2(-0.5,-0.5),float2(0.5,-0.5),float2(-0.5,0.5),float2(0.5,0.5),
      float2(-0.25,0),float2(0.25,0),float2(0,-0.25),float2(0,0.25)};
    float3 glow = float3(0,0,0);
    [unroll] for (uint i=0; i<12; ++i)
      glow += NexoraExtractBloom(hdrScene.SampleLevel(input.uv + offsets[i] * texel, 0).rgb,
                                  tone.bloom.y) / 12.0;
    hdr = NexoraApplyBloom(hdr, glow, tone.bloom.x);
  }
  float3 color = NexoraAcesApproximate(hdr * tone.settings.x);
  color = saturate(NexoraApplyColorGrade(color, float3(0), float3(1), tone.settings.z, tone.settings.w));
  if (tone.settings.y > 0.5) color = NexoraLinearToSrgb(color);
  return float4(color, 1);
}
