#pragma once
#include "Nexora/RHI/SamplerFeedback.h"
#include <algorithm>
#include <array>
#include <limits>
#include <span>
#include <stdexcept>
#if defined(__ID3D12Device8_INTERFACE_DEFINED__) &&                                                \
    defined(__ID3D12GraphicsCommandList1_INTERFACE_DEFINED__)
#define NEXORA_D3D12_SAMPLER_FEEDBACK 1
#include <wrl/client.h>
namespace nexora::rhi::detail {
inline void CheckFeedback(HRESULT result, const char *operation) {
  if (FAILED(result))
    throw std::runtime_error(std::string(operation) + " failed with HRESULT " +
                             std::to_string(static_cast<unsigned long>(result)));
}
inline Microsoft::WRL::ComPtr<ID3D12RootSignature>
FeedbackRoot(ID3D12Device &device, const D3D12_ROOT_SIGNATURE_DESC &description) {
  Microsoft::WRL::ComPtr<ID3DBlob> code, errors;
  CheckFeedback(
      D3D12SerializeRootSignature(&description, D3D_ROOT_SIGNATURE_VERSION_1, &code, &errors),
      "Serialize sampler feedback root");
  Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
  CheckFeedback(device.CreateRootSignature(0, code->GetBufferPointer(), code->GetBufferSize(),
                                           IID_PPV_ARGS(&root)),
                "Create sampler feedback root");
  return root;
}
struct NativeFeedbackProgram final {
  Microsoft::WRL::ComPtr<ID3D12Resource> paired;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> write_root, pack_root;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> write, pack;
};
inline NativeFeedbackProgram CreateNativeFeedbackProgram(ID3D12Device8 &device,
                                                         std::span<const std::byte> write,
                                                         std::span<const std::byte> pack) {
  NativeFeedbackProgram result;
  const std::array<D3D12_DESCRIPTOR_RANGE, 2> ranges{
      {{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0},
       {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 1}}};
  std::array<D3D12_ROOT_PARAMETER, 3> parameters{};
  for (auto &parameter : parameters)
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[0].DescriptorTable = {2, ranges.data()};
  parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
  parameters[1].Descriptor.ShaderRegister = 1;
  parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  parameters[2].Constants = {0, 0, 4};
  D3D12_STATIC_SAMPLER_DESC sampler{};
  sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  sampler.MaxLOD = std::numeric_limits<float>::max();
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  const D3D12_ROOT_SIGNATURE_DESC write_root{3, parameters.data(), 1, &sampler,
                                             D3D12_ROOT_SIGNATURE_FLAG_NONE};
  result.write_root = FeedbackRoot(device, write_root);
  const D3D12_DESCRIPTOR_RANGE decoded_range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
  std::array<D3D12_ROOT_PARAMETER, 2> packed{};
  for (auto &parameter : packed)
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  packed[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  packed[0].DescriptorTable = {1, &decoded_range};
  packed[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
  packed[1].Descriptor.ShaderRegister = 1;
  const D3D12_ROOT_SIGNATURE_DESC pack_root{2, packed.data(), 0, nullptr,
                                            D3D12_ROOT_SIGNATURE_FLAG_NONE};
  result.pack_root = FeedbackRoot(device, pack_root);
  for (unsigned index = 0; index < 2; ++index) {
    const auto code = index == 0 ? write : pack;
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};
    pipeline.pRootSignature = index == 0 ? result.write_root.Get() : result.pack_root.Get();
    pipeline.CS = {code.data(), code.size()};
    auto &target = index == 0 ? result.write : result.pack;
    CheckFeedback(device.CreateComputePipelineState(&pipeline, IID_PPV_ARGS(&target)),
                  "Create sampler feedback pipeline");
  }
  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = D3D12_HEAP_TYPE_DEFAULT;
  heap.CreationNodeMask = heap.VisibleNodeMask = 1;
  D3D12_RESOURCE_DESC paired{};
  paired.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  paired.Width = paired.Height = SamplerFeedbackTextureExtent;
  paired.DepthOrArraySize = 1;
  paired.MipLevels = SamplerFeedbackMipLevels;
  paired.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  paired.SampleDesc.Count = 1;
  paired.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  // Feedback addresses texture metadata only; no shader samples its uninitialized color data.
  CheckFeedback(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &paired,
                                               D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                               nullptr, IID_PPV_ARGS(&result.paired)),
                "Create paired feedback footprint texture");
  return result;
}
struct NativeFeedbackContext final {
  Microsoft::WRL::ComPtr<ID3D12Resource> opaque, decoded;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shader_heap, clear_heap;
  UINT increment{};
};
inline NativeFeedbackContext CreateNativeFeedbackContext(ID3D12Device8 &device,
                                                         const NativeFeedbackProgram &program) {
  NativeFeedbackContext result;
  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = D3D12_HEAP_TYPE_DEFAULT;
  heap.CreationNodeMask = heap.VisibleNodeMask = 1;
  D3D12_RESOURCE_DESC1 feedback{};
  feedback.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  feedback.Width = feedback.Height = SamplerFeedbackTextureExtent;
  feedback.DepthOrArraySize = 1;
  feedback.MipLevels = SamplerFeedbackMipLevels;
  feedback.Format = DXGI_FORMAT_SAMPLER_FEEDBACK_MIN_MIP_OPAQUE;
  feedback.SampleDesc.Count = 1;
  feedback.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  feedback.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  feedback.SamplerFeedbackMipRegion = {SamplerFeedbackRegionExtent, SamplerFeedbackRegionExtent, 1};
  CheckFeedback(device.CreateCommittedResource2(&heap, D3D12_HEAP_FLAG_NONE, &feedback,
                                                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                                nullptr, IID_PPV_ARGS(&result.opaque)),
                "Create opaque min-mip feedback texture");
  D3D12_RESOURCE_DESC decoded{};
  decoded.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  decoded.Width = decoded.Height = SamplerFeedbackMapExtent;
  decoded.DepthOrArraySize = decoded.MipLevels = 1;
  decoded.Format = DXGI_FORMAT_R8_UINT;
  decoded.SampleDesc.Count = 1;
  decoded.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  CheckFeedback(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &decoded,
                                               D3D12_RESOURCE_STATE_RESOLVE_DEST, nullptr,
                                               IID_PPV_ARGS(&result.decoded)),
                "Create decoded min-mip feedback texture");
  D3D12_DESCRIPTOR_HEAP_DESC descriptors{};
  descriptors.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  descriptors.NumDescriptors = 3;
  descriptors.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  CheckFeedback(device.CreateDescriptorHeap(&descriptors, IID_PPV_ARGS(&result.shader_heap)),
                "Create feedback shader descriptors");
  descriptors.NumDescriptors = 1;
  descriptors.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  CheckFeedback(device.CreateDescriptorHeap(&descriptors, IID_PPV_ARGS(&result.clear_heap)),
                "Create feedback CPU clear descriptor");
  result.increment =
      device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  auto cpu = result.shader_heap->GetCPUDescriptorHandleForHeapStart();
  D3D12_SHADER_RESOURCE_VIEW_DESC view{};
  view.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  view.Texture2D.MipLevels = SamplerFeedbackMipLevels;
  // Tier0.9 requires full mip range, MostDetailedMip0, PlaneSlice0 and MinLODClamp0.
  device.CreateShaderResourceView(program.paired.Get(), &view, cpu);
  cpu.ptr += result.increment;
  device.CreateSamplerFeedbackUnorderedAccessView(program.paired.Get(), result.opaque.Get(), cpu);
  device.CreateSamplerFeedbackUnorderedAccessView(
      program.paired.Get(), result.opaque.Get(),
      result.clear_heap->GetCPUDescriptorHandleForHeapStart());
  cpu.ptr += result.increment;
  view.Format = DXGI_FORMAT_R8_UINT;
  view.Texture2D.MipLevels = 1;
  device.CreateShaderResourceView(result.decoded.Get(), &view, cpu);
  return result;
}
inline void RecordNativeFeedback(ID3D12GraphicsCommandList1 &commands,
                                 const NativeFeedbackProgram &program,
                                 const NativeFeedbackContext &context, ID3D12Resource &requests,
                                 ID3D12Resource &output, UINT count) {
  ID3D12DescriptorHeap *heap = context.shader_heap.Get();
  commands.SetDescriptorHeaps(1, &heap);
  auto feedback = context.shader_heap->GetGPUDescriptorHandleForHeapStart();
  feedback.ptr += context.increment;
  const UINT
      ignored_values[4]{}; // The API ignores clear values; a cleared MinMip map decodes to255.
  commands.ClearUnorderedAccessViewUint(feedback,
                                        context.clear_heap->GetCPUDescriptorHandleForHeapStart(),
                                        context.opaque.Get(), ignored_values, 0, nullptr);
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  barrier.UAV.pResource = context.opaque.Get();
  commands.ResourceBarrier(1, &barrier);
  commands.SetComputeRootSignature(program.write_root.Get());
  commands.SetPipelineState(program.write.Get());
  commands.SetComputeRootDescriptorTable(0,
                                         context.shader_heap->GetGPUDescriptorHandleForHeapStart());
  commands.SetComputeRootShaderResourceView(1, requests.GetGPUVirtualAddress());
  const UINT constants[4]{count, 0, 0, 0};
  commands.SetComputeRoot32BitConstants(2, 4, constants, 0);
  commands.Dispatch(std::max(1U, count), 1, 1);
  barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition = {context.opaque.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RESOLVE_SOURCE};
  commands.ResourceBarrier(1, &barrier);
  // MIN_MIP decoding flattens the entire opaque mip chain; the source must be UINT_MAX.
  commands.ResolveSubresourceRegion(context.decoded.Get(), 0, 0, 0, context.opaque.Get(),
                                    std::numeric_limits<UINT>::max(), nullptr, DXGI_FORMAT_R8_UINT,
                                    D3D12_RESOLVE_MODE_DECODE_SAMPLER_FEEDBACK);
  barrier.Transition = {context.decoded.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                        D3D12_RESOURCE_STATE_RESOLVE_DEST,
                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE};
  commands.ResourceBarrier(1, &barrier);
  commands.SetComputeRootSignature(program.pack_root.Get());
  commands.SetPipelineState(program.pack.Get());
  auto decoded = context.shader_heap->GetGPUDescriptorHandleForHeapStart();
  decoded.ptr += 2ULL * context.increment;
  commands.SetComputeRootDescriptorTable(0, decoded);
  commands.SetComputeRootUnorderedAccessView(1, output.GetGPUVirtualAddress());
  commands.Dispatch(16, 1, 1);
}
} // namespace nexora::rhi::detail
#else
#define NEXORA_D3D12_SAMPLER_FEEDBACK 0
#endif
