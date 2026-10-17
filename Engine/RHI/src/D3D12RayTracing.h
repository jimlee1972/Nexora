#pragma once
#include "Nexora/RHI/RayTracing.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
// Include after d3d12.h. Baseline SDK builds need not declare ray interfaces.
#if defined(__ID3D12Device5_INTERFACE_DEFINED__) &&                                                \
    defined(__ID3D12GraphicsCommandList4_INTERFACE_DEFINED__)
#define NEXORA_D3D12_RAY_TRACING 1
#include <wrl/client.h>
namespace nexora::rhi::detail {
using Microsoft::WRL::ComPtr;
inline constexpr std::uint64_t RayAllocationLimit = 64ULL * 1024 * 1024;
inline void CheckRay(HRESULT result, const char *operation) {
  if (FAILED(result))
    throw std::runtime_error(std::string(operation) + " failed with HRESULT " +
                             std::to_string(static_cast<unsigned long>(result)));
}
inline ComPtr<ID3D12Resource> RayBuffer(ID3D12Device &device, UINT64 size, bool upload,
                                        D3D12_RESOURCE_STATES state) {
  if (!size || size > RayAllocationLimit)
    throw std::invalid_argument("ray allocation exceeds bounded size");
  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = upload ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
  heap.CreationNodeMask = heap.VisibleNodeMask = 1;
  D3D12_RESOURCE_DESC description{};
  description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  description.Width = size;
  description.Height = 1;
  description.DepthOrArraySize = description.MipLevels = 1;
  description.SampleDesc.Count = 1;
  description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  if (!upload)
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  ComPtr<ID3D12Resource> result;
  CheckRay(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &description, state, nullptr,
                                          IID_PPV_ARGS(&result)),
           "Create ray buffer");
  return result;
}
inline void UploadRayBytes(ID3D12Resource &resource, std::span<const std::byte> bytes) {
  void *mapped{};
  const D3D12_RANGE read{0, 0};
  CheckRay(resource.Map(0, &read, &mapped), "Map ray upload");
  std::memcpy(mapped, bytes.data(), bytes.size());
  const D3D12_RANGE written{0, bytes.size()};
  resource.Unmap(0, &written);
}
struct NativeRayScene final {
  ComPtr<ID3D12Resource> vertices, instances, bottom, top;
};
struct PendingRayScene final {
  NativeRayScene scene;
  ComPtr<ID3D12Resource> bottom_scratch, top_scratch;
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList4> commands;
};
inline PendingRayScene BuildNativeRayScene(ID3D12Device5 &device,
                                           std::span<const RayTriangle> triangles) {
  if (!IsRaySceneValid(triangles))
    throw std::invalid_argument("ray scene requires one to eight finite nondegenerate triangles");
  PendingRayScene result;
  result.scene.vertices =
      RayBuffer(device, triangles.size_bytes(), true, D3D12_RESOURCE_STATE_GENERIC_READ);
  UploadRayBytes(*result.scene.vertices.Get(), std::as_bytes(triangles));
  D3D12_RAYTRACING_GEOMETRY_DESC geometry{};
  geometry.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
  geometry.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
  geometry.Triangles.VertexBuffer = {result.scene.vertices->GetGPUVirtualAddress(),
                                     3 * sizeof(float)};
  geometry.Triangles.VertexCount = static_cast<UINT>(triangles.size() * 3);
  geometry.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
  geometry.Triangles.IndexFormat = DXGI_FORMAT_UNKNOWN;
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottom{};
  bottom.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
  bottom.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  bottom.NumDescs = 1;
  bottom.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
  bottom.pGeometryDescs = &geometry;
  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottom_size{};
  device.GetRaytracingAccelerationStructurePrebuildInfo(&bottom, &bottom_size);
  result.scene.bottom = RayBuffer(device, bottom_size.ResultDataMaxSizeInBytes, false,
                                  D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
  result.bottom_scratch = RayBuffer(device, bottom_size.ScratchDataSizeInBytes, false,
                                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  D3D12_RAYTRACING_INSTANCE_DESC instance{};
  instance.Transform[0][0] = instance.Transform[1][1] = instance.Transform[2][2] = 1;
  instance.InstanceMask = 0xff;
  instance.AccelerationStructure = result.scene.bottom->GetGPUVirtualAddress();
  result.scene.instances =
      RayBuffer(device, sizeof(instance), true, D3D12_RESOURCE_STATE_GENERIC_READ);
  UploadRayBytes(*result.scene.instances.Get(), std::as_bytes(std::span{&instance, 1}));
  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS top{};
  top.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
  top.Flags = bottom.Flags;
  top.NumDescs = 1;
  top.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
  top.InstanceDescs = result.scene.instances->GetGPUVirtualAddress();
  D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO top_size{};
  device.GetRaytracingAccelerationStructurePrebuildInfo(&top, &top_size);
  result.scene.top = RayBuffer(device, top_size.ResultDataMaxSizeInBytes, false,
                               D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
  result.top_scratch = RayBuffer(device, top_size.ScratchDataSizeInBytes, false,
                                 D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
  CheckRay(device.CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         IID_PPV_ARGS(&result.allocator)),
           "Create ray build allocator");
  CheckRay(device.CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, result.allocator.Get(),
                                    nullptr, IID_PPV_ARGS(&result.commands)),
           "Create ray build list");
  const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottom_build{
      result.scene.bottom->GetGPUVirtualAddress(), bottom, 0,
      result.bottom_scratch->GetGPUVirtualAddress()};
  result.commands->BuildRaytracingAccelerationStructure(&bottom_build, 0, nullptr);
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  barrier.UAV.pResource = result.scene.bottom.Get();
  result.commands->ResourceBarrier(1, &barrier);
  const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC top_build{
      result.scene.top->GetGPUVirtualAddress(), top, 0, result.top_scratch->GetGPUVirtualAddress()};
  result.commands->BuildRaytracingAccelerationStructure(&top_build, 0, nullptr);
  barrier.UAV.pResource = result.scene.top.Get();
  result.commands->ResourceBarrier(1, &barrier);
  return result;
}
struct NativeRayProgram final {
  RayTracingMode mode{};
  ComPtr<ID3D12RootSignature> root;
  ComPtr<ID3D12Resource> table;
  ComPtr<ID3D12StateObject> state;
  ComPtr<ID3D12PipelineState> compute;
};
inline NativeRayProgram CreateNativeRayProgram(ID3D12Device5 &device, RayTracingMode mode,
                                               std::span<const std::byte> code) {
  NativeRayProgram result;
  result.mode = mode;
  std::array<D3D12_ROOT_PARAMETER, 3> parameters{};
  for (UINT index = 0; index < 2; ++index) {
    parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    parameters[index].Descriptor.ShaderRegister = index;
    parameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  }
  parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
  parameters[2].Descriptor.ShaderRegister = 1;
  parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  const D3D12_ROOT_SIGNATURE_DESC root{3, parameters.data(), 0, nullptr,
                                       D3D12_ROOT_SIGNATURE_FLAG_NONE};
  ComPtr<ID3DBlob> serialized, errors;
  CheckRay(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors),
           "Serialize ray root");
  CheckRay(device.CreateRootSignature(0, serialized->GetBufferPointer(),
                                      serialized->GetBufferSize(), IID_PPV_ARGS(&result.root)),
           "Create ray root");
  if (mode == RayTracingMode::InlineQuery) {
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};
    pipeline.pRootSignature = result.root.Get();
    pipeline.CS = {code.data(), code.size()};
    CheckRay(device.CreateComputePipelineState(&pipeline, IID_PPV_ARGS(&result.compute)),
             "Create inline ray pipeline");
    return result;
  }
  const D3D12_EXPORT_DESC exports[]{{L"rayGeneration", nullptr, D3D12_EXPORT_FLAG_NONE},
                                    {L"rayMiss", nullptr, D3D12_EXPORT_FLAG_NONE},
                                    {L"rayClosest", nullptr, D3D12_EXPORT_FLAG_NONE}};
  const D3D12_DXIL_LIBRARY_DESC library{{code.data(), code.size()}, 3, exports};
  D3D12_HIT_GROUP_DESC group{};
  group.HitGroupExport = L"NexoraTriangleHitGroup";
  group.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
  group.ClosestHitShaderImport = L"rayClosest";
  const D3D12_RAYTRACING_SHADER_CONFIG shader{sizeof(RayHit), 2 * sizeof(float)};
  const D3D12_GLOBAL_ROOT_SIGNATURE signature{result.root.Get()};
  const D3D12_RAYTRACING_PIPELINE_CONFIG pipeline{1};
  const D3D12_STATE_SUBOBJECT objects[]{
      {D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &library},
      {D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &group},
      {D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &shader},
      {D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &signature},
      {D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipeline}};
  const D3D12_STATE_OBJECT_DESC description{D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
                                            static_cast<UINT>(std::size(objects)), objects};
  CheckRay(device.CreateStateObject(&description, IID_PPV_ARGS(&result.state)),
           "Create ray state object");
  ComPtr<ID3D12StateObjectProperties> properties;
  CheckRay(result.state.As(&properties), "Query ray identifiers");
  std::array<std::byte, 192> records{};
  const wchar_t *names[]{L"rayGeneration", L"rayMiss", L"NexoraTriangleHitGroup"};
  for (std::size_t index = 0; index < 3; ++index) {
    const auto *identifier = properties->GetShaderIdentifier(names[index]);
    if (!identifier)
      throw std::invalid_argument("ray library lacks required shader-table export");
    std::memcpy(records.data() + index * 64, identifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
  }
  result.table = RayBuffer(device, records.size(), true, D3D12_RESOURCE_STATE_GENERIC_READ);
  UploadRayBytes(*result.table.Get(), records);
  return result;
}
inline void RecordNativeRays(ID3D12GraphicsCommandList4 &commands, const NativeRayProgram &program,
                             const NativeRayScene &scene, ID3D12Resource &input,
                             ID3D12Resource &output, UINT count) {
  commands.SetComputeRootSignature(program.root.Get());
  commands.SetComputeRootShaderResourceView(0, scene.top->GetGPUVirtualAddress());
  commands.SetComputeRootShaderResourceView(1, input.GetGPUVirtualAddress());
  commands.SetComputeRootUnorderedAccessView(2, output.GetGPUVirtualAddress());
  if (program.mode == RayTracingMode::InlineQuery) {
    commands.SetPipelineState(program.compute.Get());
    commands.Dispatch(count, 1, 1);
    return;
  }
  commands.SetPipelineState1(program.state.Get());
  const auto address = program.table->GetGPUVirtualAddress();
  D3D12_DISPATCH_RAYS_DESC description{};
  description.RayGenerationShaderRecord = {address, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES};
  description.MissShaderTable = {address + 64, 64, 64};
  description.HitGroupTable = {address + 128, 64, 64};
  description.Width = count;
  description.Height = description.Depth = 1;
  commands.DispatchRays(&description);
}
} // namespace nexora::rhi::detail
#else
#define NEXORA_D3D12_RAY_TRACING 0
#endif
