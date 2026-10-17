// Compile-time allocation with an explicit ceiling; not runtime dynamic allocation.
#ifndef GROUP_WORDS
#define GROUP_WORDS 16384
#endif
StructuredBuffer<uint> input : register(t0);
RWStructuredBuffer<uint> output : register(u1);
groupshared uint scratch[GROUP_WORDS];

[numthreads(128, 1, 1)]
[GroupSharedLimit(65536)]
void computeMain(uint lane : SV_GroupIndex) {
  for (uint i = lane; i < GROUP_WORDS; i += 128)
    scratch[i] = input[i % 8] + i;
  GroupMemoryBarrierWithGroupSync();
  if (lane == 0) {
    uint sum = 0;
    for (uint i = 0; i < GROUP_WORDS; ++i)
      sum += scratch[i];
    output[0] = sum;
  }
}
