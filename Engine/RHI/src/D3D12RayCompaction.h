#pragma once
#include "D3D12RayTracing.h"

// Include after the SDK. Each phase retains all resources until its own queue completion.
#if NEXORA_D3D12_RAY_TRACING
namespace nexora::rhi::detail {
inline void RayCompactionDependency(ID3D12GraphicsCommandList4 &commands,
                                    ID3D12Resource &resource) {
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  barrier.UAV.pResource = &resource;
  commands.ResourceBarrier(1, &barrier);
}
inline void QueryRayCompactedSizes(ID3D12Device5 &device, PendingRayScene &pending,
                                   std::span<const D3D12_GPU_VIRTUAL_ADDRESS> addresses) {
  if (addresses.empty() || addresses.size() > 2)
    throw std::invalid_argument("invalid bounded compaction query count");
  const auto bytes = addresses.size_bytes();
  pending.compact_info = RayBuffer(device, bytes, false, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  pending.compact_readback = RayBuffer(device, bytes, false, D3D12_RESOURCE_STATE_COPY_DEST, true);
  pending.compact_info_count = static_cast<std::uint32_t>(addresses.size());
  const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC description{
      pending.compact_info->GetGPUVirtualAddress(),
      D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_COMPACTED_SIZE};
  pending.commands->EmitRaytracingAccelerationStructurePostbuildInfo(
      &description, pending.compact_info_count, addresses.data());
  RayCompactionDependency(*pending.commands.Get(), *pending.compact_info.Get());
  D3D12_RESOURCE_BARRIER copy{};
  copy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  copy.Transition = {pending.compact_info.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                     D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE};
  pending.commands->ResourceBarrier(1, &copy);
  pending.commands->CopyBufferRegion(pending.compact_readback.Get(), 0, pending.compact_info.Get(),
                                     0, bytes);
}
inline UINT64 ReadRayCompactedSize(PendingRayScene &completed, std::uint32_t index,
                                   UINT64 source_bytes) {
  if (!completed.compact_readback || index >= completed.compact_info_count)
    throw std::logic_error("missing completed ray compaction query");
  void *mapped{};
  const D3D12_RANGE read{index * sizeof(UINT64), (index + 1) * sizeof(UINT64)};
  CheckRay(completed.compact_readback->Map(0, &read, &mapped), "Map compacted-size metadata");
  UINT64 bytes{};
  std::memcpy(&bytes, static_cast<const std::byte *>(mapped) + read.Begin, sizeof(bytes));
  const D3D12_RANGE written{0, 0};
  completed.compact_readback->Unmap(0, &written);
  if (!bytes || bytes > RayAllocationLimit || bytes > source_bytes)
    throw std::runtime_error("invalid native compacted acceleration structure size");
  // DXR aligns AS addresses, not queried storage extents. Padding a valid query can exceed
  // an unaligned original buffer width even when the native compacted result fits exactly.
  return bytes;
}
inline ComPtr<ID3D12Resource> RayCompactionBuffer(ID3D12Device5 &device, UINT64 bytes) {
  auto buffer =
      RayBuffer(device, bytes, false, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
  if (buffer->GetGPUVirtualAddress() % D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT)
    throw std::runtime_error("unaligned native compaction destination address");
  return buffer;
}
inline PendingRayScene QueryRayCompaction(ID3D12Device5 &device, const NativeRayScene &source) {
  if (!source.updatable || source.opacity_micromaps || !source.allow_compaction || !source.bottom ||
      !source.top)
    throw std::invalid_argument("compaction requires an ordinary triangle scene");
  PendingRayScene pending;
  pending.source = source;
  pending.scene = source;
  BeginRaySceneCommands(device, pending);
  const std::array addresses{source.bottom->GetGPUVirtualAddress()};
  QueryRayCompactedSizes(device, pending, addresses);
  return pending;
}
inline PendingRayScene CompactRayBottomAndRelinkTop(ID3D12Device5 &device, PendingRayScene pending,
                                                    UINT64 bottom_bytes) {
  BeginRaySceneCommands(device, pending);
  pending.scene.bottom = RayCompactionBuffer(device, bottom_bytes);
  UploadRayInstances(device, pending.scene);
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
  inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
  inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE |
                 D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
                 D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_COMPACTION;
  inputs.NumDescs = pending.scene.instance_count;
  inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
  inputs.InstanceDescs = pending.scene.instances->GetGPUVirtualAddress();
  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO size{};
  device.GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &size);
  ValidateRayPrebuild(size, "compaction TLAS relink", true);
  pending.scene.top = RayBuffer(device, size.ResultDataMaxSizeInBytes, false,
                                D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
  pending.top_scratch = RayScratchBuffer(device, size.UpdateScratchDataSizeInBytes);
  pending.commands->CopyRaytracingAccelerationStructure(
      pending.scene.bottom->GetGPUVirtualAddress(), pending.source.bottom->GetGPUVirtualAddress(),
      D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT);
  RayCompactionDependency(*pending.commands.Get(), *pending.scene.bottom.Get());
  inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
  const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC update{
      pending.scene.top->GetGPUVirtualAddress(), inputs, pending.source.top->GetGPUVirtualAddress(),
      RayScratchAddress(pending.top_scratch)};
  pending.commands->BuildRaytracingAccelerationStructure(&update, 0, nullptr);
  RayCompactionDependency(*pending.commands.Get(), *pending.scene.top.Get());
  const std::array addresses{pending.scene.top->GetGPUVirtualAddress()};
  QueryRayCompactedSizes(device, pending, addresses);
  return pending;
}
inline PendingRayScene CompactRayTop(ID3D12Device5 &device, PendingRayScene pending,
                                     UINT64 top_bytes) {
  BeginRaySceneCommands(device, pending);
  // The completed relink's TLAS must remain alive as the final compact copy's source.
  pending.source = pending.scene;
  pending.scene.top = RayCompactionBuffer(device, top_bytes);
  pending.commands->CopyRaytracingAccelerationStructure(
      pending.scene.top->GetGPUVirtualAddress(), pending.source.top->GetGPUVirtualAddress(),
      D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE_COMPACT);
  RayCompactionDependency(*pending.commands.Get(), *pending.scene.top.Get());
  return pending;
}
} // namespace nexora::rhi::detail
#endif
