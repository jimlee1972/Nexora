#pragma once

#include "Nexora/RHI/Types.h"
#include <array>
#include <limits>
#include <span>
#include <stdexcept>

// Include after the platform SDK; default builds do not require Work Graph declarations.
#if NEXORA_ENABLE_D3D12_WORK_GRAPHS &&                                                             \
    defined(__ID3D12GraphicsCommandList10_INTERFACE_DEFINED__) &&                                  \
    defined(__ID3D12Device5_INTERFACE_DEFINED__) &&                                                \
    defined(__ID3D12StateObjectProperties1_INTERFACE_DEFINED__) &&                                 \
    defined(__ID3D12WorkGraphProperties_INTERFACE_DEFINED__)
#define NEXORA_D3D12_WORK_GRAPHS 1
#include <wrl/client.h>
namespace nexora::rhi::detail {
inline constexpr std::size_t WorkGraphListCapacity = WorkGraphMaximumDispatchesPerList;
inline constexpr std::uint64_t WorkGraphBackingLimit = 64ULL * 1024 * 1024;
struct NativeWorkGraph final {
  Microsoft::WRL::ComPtr<ID3D12RootSignature> root;
  Microsoft::WRL::ComPtr<ID3D12StateObject> state;
  D3D12_PROGRAM_IDENTIFIER identifier{};
  UINT entry{};
  UINT64 backing_bytes{};
};
inline void CheckWorkGraph(HRESULT result, const char *operation) {
  if (FAILED(result))
    throw std::runtime_error(std::string(operation) + " failed with HRESULT " +
                             std::to_string(static_cast<unsigned long>(result)));
}
inline NativeWorkGraph CreateNativeWorkGraph(ID3D12Device5 &device,
                                             std::span<const std::byte> code) {
  NativeWorkGraph result;
  D3D12_ROOT_PARAMETER output{};
  output.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
  output.Descriptor.ShaderRegister = 1;
  output.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  D3D12_ROOT_SIGNATURE_DESC root_description{};
  root_description.NumParameters = 1;
  root_description.pParameters = &output;
  Microsoft::WRL::ComPtr<ID3DBlob> serialized, errors;
  CheckWorkGraph(D3D12SerializeRootSignature(&root_description, D3D_ROOT_SIGNATURE_VERSION_1,
                                             &serialized, &errors),
                 "Serialize Work Graph root");
  CheckWorkGraph(device.CreateRootSignature(0, serialized->GetBufferPointer(),
                                            serialized->GetBufferSize(),
                                            IID_PPV_ARGS(&result.root)),
                 "Create Work Graph root");
  D3D12_DXIL_LIBRARY_DESC library{{code.data(), code.size()}, 0, nullptr};
  D3D12_GLOBAL_ROOT_SIGNATURE signature{result.root.Get()};
  const D3D12_NODE_ID producer{L"producer", 0};
  D3D12_WORK_GRAPH_DESC graph{};
  graph.ProgramName = L"NexoraVisibleExpansion";
  graph.Flags = D3D12_WORK_GRAPH_FLAG_INCLUDE_ALL_AVAILABLE_NODES;
  graph.NumEntrypoints = 1;
  graph.pEntrypoints = &producer;
  const D3D12_STATE_SUBOBJECT subobjects[]{
      {D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &library},
      {D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &signature},
      {D3D12_STATE_SUBOBJECT_TYPE_WORK_GRAPH, &graph}};
  const D3D12_STATE_OBJECT_DESC description{D3D12_STATE_OBJECT_TYPE_EXECUTABLE,
                                            static_cast<UINT>(std::size(subobjects)), subobjects};
  CheckWorkGraph(device.CreateStateObject(&description, IID_PPV_ARGS(&result.state)),
                 "CreateStateObject(work graph)");
  Microsoft::WRL::ComPtr<ID3D12StateObjectProperties1> programs;
  Microsoft::WRL::ComPtr<ID3D12WorkGraphProperties> properties;
  CheckWorkGraph(result.state.As(&programs), "QueryInterface(work graph program properties)");
  CheckWorkGraph(result.state.As(&properties), "QueryInterface(work graph properties)");
  const auto index = properties->GetWorkGraphIndex(graph.ProgramName);
  const auto invalid = std::numeric_limits<UINT>::max();
  if (index == invalid || properties->GetNumWorkGraphs() != 1 ||
      properties->GetNumNodes(index) != 2 || properties->GetNumEntrypoints(index) != 1)
    throw std::invalid_argument("work graph must contain exactly the bounded two-node program");
  for (const auto node : {producer, D3D12_NODE_ID{L"consumer", 0}}) {
    const auto node_index = properties->GetNodeIndex(index, node);
    if (node_index == invalid ||
        properties->GetNodeLocalRootArgumentsTableIndex(index, node_index) != invalid)
      throw std::invalid_argument("work graph node requires unsupported local root arguments");
  }
  result.entry = properties->GetEntrypointIndex(index, producer);
  if (result.entry == invalid ||
      properties->GetEntrypointRecordSizeInBytes(index, result.entry) != sizeof(WorkGraphInput))
    throw std::invalid_argument("work graph entry record ABI does not match the bounded input");
  result.identifier = programs->GetProgramIdentifier(graph.ProgramName);
  if (!(result.identifier.OpaqueData[0] | result.identifier.OpaqueData[1] |
        result.identifier.OpaqueData[2] | result.identifier.OpaqueData[3]))
    throw std::invalid_argument("work graph program identifier is unavailable");
  D3D12_WORK_GRAPH_MEMORY_REQUIREMENTS memory{};
  properties->GetWorkGraphMemoryRequirements(index, &memory);
  if (memory.MinSizeInBytes > memory.MaxSizeInBytes ||
      memory.MinSizeInBytes > WorkGraphBackingLimit)
    throw std::invalid_argument("work graph minimum backing exceeds the bounded memory limit");
  // Exact MinSizeInBytes is valid independently from the granularity for larger allocations.
  // Zero is a native supported configuration and does not require a fabricated allocation.
  result.backing_bytes = memory.MinSizeInBytes;
  return result;
}
inline void RecordNativeWorkGraph(ID3D12GraphicsCommandList10 &commands,
                                  const NativeWorkGraph &program, ID3D12Resource *backing,
                                  const WorkGraphInput &input) {
  D3D12_SET_PROGRAM_DESC selected{};
  selected.Type = D3D12_PROGRAM_TYPE_WORK_GRAPH;
  selected.WorkGraph.ProgramIdentifier = program.identifier;
  selected.WorkGraph.Flags = D3D12_SET_WORK_GRAPH_FLAG_INITIALIZE;
  selected.WorkGraph.BackingMemory = {backing ? backing->GetGPUVirtualAddress() : 0,
                                      program.backing_bytes};
  commands.SetProgram(&selected);
  // The API copies CPU records into the command list during this call, not at GPU execution.
  D3D12_DISPATCH_GRAPH_DESC dispatch{};
  dispatch.Mode = D3D12_DISPATCH_MODE_NODE_CPU_INPUT;
  dispatch.NodeCPUInput = {program.entry, 1, &input, sizeof(input)};
  commands.DispatchGraph(&dispatch);
}
} // namespace nexora::rhi::detail
#else
#define NEXORA_D3D12_WORK_GRAPHS 0
#endif
