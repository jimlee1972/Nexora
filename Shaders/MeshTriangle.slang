#include "Triangle.slang"
// GPUDriven.slang writes visibleCount at word4; no CPU count decision is required.
StructuredBuffer<uint> meshStatistics : register(t0);
[shader("mesh")]
[numthreads(1, 1, 1)]
[outputtopology("triangle")]
void meshMain(out vertices VertexOutput vertices[3], out indices uint3 triangles[1]) {
  const bool visible = meshStatistics[4] != 0;
  SetMeshOutputCounts(visible ? 3 : 0, visible ? 1 : 0);
  if (!visible) return;
  const float2 positions[3] = {float2(0.0, -0.5), float2(0.5, 0.5), float2(-0.5, 0.5)};
  const float3 colors[3] = {float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1)};
  for (uint i = 0; i < 3; ++i) {
    vertices[i].position = mul(frame.transform, float4(positions[i], 0, 1));
    vertices[i].color = colors[i];
  }
  triangles[0] = uint3(0, 1, 2);
}
