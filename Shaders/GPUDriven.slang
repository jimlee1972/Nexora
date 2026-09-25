// Packed ABI used by the native Vulkan acceptance path. Keeping the four fixed storage slots
// preserves the Phase 1b RHI contract while allowing every GPU-driven stage to execute on-device.
StructuredBuffer<uint> computeCandidates : register(t0);
RWStructuredBuffer<uint> computeVisible : register(u1);
RWStructuredBuffer<uint> computeIndirect : register(u2);
RWStructuredBuffer<uint> computeStatistics : register(u3);

static const uint kHeaderWords = 44;
static const uint kCandidateWords = 12;
static const uint kVisibleWords = 5;
// Each command starts with the native VkDrawIndirectCommand-compatible words and retains the
// backend-neutral classification record after them for CompareGPUDrivenResults().
static const uint kCommandWords = 9;

float candidateFloat(uint candidate, uint field) {
  return asfloat(computeCandidates[kHeaderWords + candidate * kCandidateWords + field]);
}

uint candidateUint(uint candidate, uint field) {
  return computeCandidates[kHeaderWords + candidate * kCandidateWords + field];
}

bool inFrustum(uint candidate) {
  float3 center = float3(candidateFloat(candidate, 0), candidateFloat(candidate, 1),
                         candidateFloat(candidate, 2));
  float radius = candidateFloat(candidate, 3);
  // Rows of the view-projection matrix are stored at words 20..35. Extracting planes this way
  // exactly mirrors math::ExtractFrustum's row-major CPU reference convention.
  float4 row0 = float4(asfloat(computeCandidates[20]), asfloat(computeCandidates[21]),
                       asfloat(computeCandidates[22]), asfloat(computeCandidates[23]));
  float4 row1 = float4(asfloat(computeCandidates[24]), asfloat(computeCandidates[25]),
                       asfloat(computeCandidates[26]), asfloat(computeCandidates[27]));
  float4 row2 = float4(asfloat(computeCandidates[28]), asfloat(computeCandidates[29]),
                       asfloat(computeCandidates[30]), asfloat(computeCandidates[31]));
  float4 row3 = float4(asfloat(computeCandidates[32]), asfloat(computeCandidates[33]),
                       asfloat(computeCandidates[34]), asfloat(computeCandidates[35]));
  float4 planes[6] = {row3 + row0, row3 - row0, row3 + row1,
                      row3 - row1, row2,        row3 - row2};
  for (uint i = 0; i < 6; ++i) {
    float length = sqrt(dot(planes[i].xyz, planes[i].xyz));
    if (length > 0.000001 && dot(planes[i].xyz, center) + planes[i].w < -radius * length)
      return false;
  }
  return true;
}

bool occluded(uint candidate) {
  uint policy = computeCandidates[6];
  uint mipCount = computeCandidates[3];
  if (policy == 2 || mipCount == 0)
    return false;
  float3 center = float3(candidateFloat(candidate, 0), candidateFloat(candidate, 1),
                         candidateFloat(candidate, 2));
  float radius = candidateFloat(candidate, 3);
  float4 clip;
  for (uint row = 0; row < 4; ++row) {
    uint base = 20 + row * 4;
    clip[row] = asfloat(computeCandidates[base]) * center.x +
                asfloat(computeCandidates[base + 1]) * center.y +
                asfloat(computeCandidates[base + 2]) * center.z +
                asfloat(computeCandidates[base + 3]);
  }
  if (clip.w <= 0.000001)
    return false;
  float diameter = radius / clip.w * max(computeCandidates[1], computeCandidates[2]);
  uint mip = 0;
  while (mip + 1 < mipCount && diameter > 2.0) {
    diameter *= 0.5;
    ++mip;
  }
  uint width = max(1u, computeCandidates[1] >> mip);
  uint height = max(1u, computeCandidates[2] >> mip);
  uint x = min(uint(clamp((clip.x / clip.w * 0.5 + 0.5) * width, 0.0, float(width - 1))), width - 1);
  uint y = min(uint(clamp((-clip.y / clip.w * 0.5 + 0.5) * height, 0.0, float(height - 1))), height - 1);
  uint offset = computeCandidates[36 + mip];
  float depth = asfloat(computeCandidates[offset + y * width + x]);
  float bias = asfloat(computeCandidates[5]) * (policy == 1 ? 4.0 : 1.0);
  return clip.z / clip.w - radius / clip.w > depth + bias;
}

bool greaterVisible(uint left, uint right) {
  // material, mesh, LOD, slot, generation
  uint fields[5] = {3, 2, 4, 0, 1};
  for (uint i = 0; i < 5; ++i) {
    uint a = computeVisible[left * kVisibleWords + fields[i]];
    uint b = computeVisible[right * kVisibleWords + fields[i]];
    if (a != b)
      return a > b;
  }
  return false;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 id : SV_DispatchThreadID) {
  if (id.x != 0)
    return;
  uint candidateCount = computeCandidates[0];
  computeStatistics[0] = candidateCount;
  for (uint i = 1; i < 8; ++i)
    computeStatistics[i] = 0;
  // Graphics submits the maximum candidate count without a CPU count readback. Empty slots are
  // valid zero-work indirect commands, so only the generated bins execute vertices.
  for (uint i = 0; i < candidateCount * kCommandWords; ++i)
    computeIndirect[i] = 0;

  uint visibleCount = 0;
  for (uint candidate = 0; candidate < candidateCount; ++candidate) {
    if ((candidateUint(candidate, 8) & 1) == 0)
      continue;
    if (!inFrustum(candidate)) {
      ++computeStatistics[1];
      continue;
    }
    float3 center = float3(candidateFloat(candidate, 0), candidateFloat(candidate, 1),
                           candidateFloat(candidate, 2));
    float3 camera = float3(asfloat(computeCandidates[7]), asfloat(computeCandidates[8]),
                           asfloat(computeCandidates[9]));
    float distance = length(center - camera);
    if (distance - candidateFloat(candidate, 3) > asfloat(computeCandidates[4])) {
      ++computeStatistics[2];
      continue;
    }
    if (occluded(candidate)) {
      ++computeStatistics[3];
      continue;
    }
    uint lod = 0;
    uint lodCount = candidateUint(candidate, 9);
    uint thresholdCount = computeCandidates[10];
    while (lod + 1 < lodCount && lod < thresholdCount &&
           distance >= asfloat(computeCandidates[11 + lod]))
      ++lod;
    uint output = visibleCount++ * kVisibleWords;
    computeVisible[output] = candidateUint(candidate, 4);
    computeVisible[output + 1] = candidateUint(candidate, 5);
    computeVisible[output + 2] = candidateUint(candidate, 6);
    computeVisible[output + 3] = candidateUint(candidate, 7);
    computeVisible[output + 4] = lod;
  }

  // Stable insertion sort gives deterministic classification without atomics or subgroup-order
  // dependencies. This acceptance kernel favors semantic proof over production-scale throughput.
  for (uint i = 1; i < visibleCount; ++i) {
    uint j = i;
    while (j > 0 && greaterVisible(j - 1, j)) {
      for (uint field = 0; field < kVisibleWords; ++field) {
        uint temporary = computeVisible[(j - 1) * kVisibleWords + field];
        computeVisible[(j - 1) * kVisibleWords + field] =
            computeVisible[j * kVisibleWords + field];
        computeVisible[j * kVisibleWords + field] = temporary;
      }
      --j;
    }
  }

  uint commandCount = 0;
  for (uint instance = 0; instance < visibleCount; ++instance) {
    uint visible = instance * kVisibleWords;
    bool newBin = instance == 0;
    if (!newBin) {
      uint previous = visible - kVisibleWords;
      newBin = computeVisible[visible + 2] != computeVisible[previous + 2] ||
               computeVisible[visible + 3] != computeVisible[previous + 3] ||
               computeVisible[visible + 4] != computeVisible[previous + 4];
    }
    if (newBin) {
      uint command = commandCount++ * kCommandWords;
      computeIndirect[command] = 3; // vertexCount
      computeIndirect[command + 1] = 1; // instanceCount
      computeIndirect[command + 2] = 0; // firstVertex
      computeIndirect[command + 3] = instance; // firstInstance
      computeIndirect[command + 4] = computeVisible[visible + 2];
      computeIndirect[command + 5] = computeVisible[visible + 3];
      computeIndirect[command + 6] = computeVisible[visible + 4];
      computeIndirect[command + 7] = instance;
      computeIndirect[command + 8] = 1;
    } else {
      uint command = (commandCount - 1) * kCommandWords;
      ++computeIndirect[command + 1];
      ++computeIndirect[command + 8];
    }
  }
  computeStatistics[4] = visibleCount;
  computeStatistics[5] = commandCount;
}
