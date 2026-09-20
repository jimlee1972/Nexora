struct FrameConstants { float4x4 transform; };
ConstantBuffer<FrameConstants> frame : register(b0);
struct VertexOutput { float4 position : SV_Position; float3 color : COLOR0; };
[shader("vertex")]
VertexOutput vertexMain(uint id : SV_VertexID) {
  const float2 positions[3] = { float2(0.0, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5) };
  const float3 colors[3] = { float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1) };
  VertexOutput output;
  output.position = mul(frame.transform, float4(positions[id], 0, 1));
  output.color = colors[id];
  return output;
}
[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target { return float4(input.color, 1); }
