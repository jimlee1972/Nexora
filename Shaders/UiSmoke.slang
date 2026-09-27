import Nexora.Common;

[[vk::binding(0, 0)]]
ConstantBuffer<NexoraUiFrame> uiFrame : register(b0);
[[vk::binding(0, 1)]]
Texture2D<float4> uiAtlas : register(t0);
[[vk::binding(0, 2)]]
SamplerState uiSampler : register(s0);

struct UiVertexInput
{
  float2 position : POSITION;
  float2 uv : TEXCOORD0;
  float4 color : COLOR0;
};

struct UiVertexOutput
{
  float4 position : SV_Position;
  float2 uv : TEXCOORD0;
  float4 color : COLOR0;
  float2 screenPosition : TEXCOORD1;
};

[shader("vertex")]
UiVertexOutput uiVertexMain(UiVertexInput input)
{
  UiVertexOutput output;
  output.position = NexoraTransformUiPosition(input.position, uiFrame.scale, uiFrame.translate);
  output.uv = NexoraEvaluateNineSliceUv(input.uv, float2(256.0, 128.0), float2(1024.0, 1024.0),
                                        float4(16.0, 16.0, 16.0, 16.0));
  output.color = input.color;
  output.screenPosition = input.position;
  return output;
}

[shader("fragment")]
float4 uiFragmentMain(UiVertexOutput input) : SV_Target
{
  float4 textureColor = uiAtlas.Sample(uiSampler, input.uv);
  float clip = NexoraEvaluateUiClip(input.screenPosition, uiFrame.clipRect, 0.5);
  return NexoraCompositeUiColor(input.color, textureColor, clip);
}
