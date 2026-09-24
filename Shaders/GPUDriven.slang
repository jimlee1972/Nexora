StructuredBuffer<uint> computeCandidates : register(t0);
RWStructuredBuffer<uint> computeVisible : register(u1);
RWStructuredBuffer<uint> computeIndirect : register(u2);
RWStructuredBuffer<uint> computeStatistics : register(u3);

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 id : SV_DispatchThreadID) {
  if (id.x >= computeStatistics[2])
    return;
  uint candidate = computeCandidates[id.x];
  if ((candidate & 1) == 0) {
    uint outputIndex;
    InterlockedAdd(computeStatistics[0], 1, outputIndex);
    computeVisible[outputIndex] = candidate;
    computeIndirect[outputIndex] = id.x;
  } else {
    InterlockedAdd(computeStatistics[1], 1);
  }
}
