struct VertexOutput {
  float4 position : SV_Position;
};
VertexOutput vertexMain(uint id : SV_VertexID) {
  float2 positions[3] = {float2(-.5, -.5), float2(.5, -.5), float2(0, .5)};
  VertexOutput result;
  result.position = float4(positions[id % 3], 0, 1);
  return result;
}
float4 fragmentMain(VertexOutput input, uint rate : SV_ShadingRate) : SV_Target0 {
  return float4(float((rate >> 2) & 3) / 3, float(rate & 3) / 3, .125, 1);
}
