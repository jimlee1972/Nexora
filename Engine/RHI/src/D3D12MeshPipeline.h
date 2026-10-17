#pragma once

#include <limits>

#if defined(__ID3D12GraphicsCommandList6_INTERFACE_DEFINED__) &&                                   \
    defined(__ID3D12Device2_INTERFACE_DEFINED__)
namespace nexora::rhi::detail {
// Each subobject starts at pointer alignment, including its payload's trailing padding.
template <D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Kind, class Payload> union Subobject {
  struct Value final {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE kind;
    Payload value;
  } data;
  // A pointer member supplies required stream alignment, without an alignas padding warning.
  // Only data is active; the alignment member is never read.
  void *pointer_alignment;
  constexpr Subobject() : data{Kind, {}} {}
};
struct MeshStream final {
  Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, ID3D12RootSignature *> root;
  Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS, D3D12_SHADER_BYTECODE> mesh;
  Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS, D3D12_SHADER_BYTECODE> pixel;
  Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND, D3D12_BLEND_DESC> blend;
  Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK, UINT> mask;
  Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, D3D12_RASTERIZER_DESC> rasterizer;
  Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL, D3D12_DEPTH_STENCIL_DESC> depth;
  Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY, D3D12_PRIMITIVE_TOPOLOGY_TYPE>
      topology;
  Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS, D3D12_RT_FORMAT_ARRAY>
      targets;
  Subobject<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC, DXGI_SAMPLE_DESC> samples;
};
inline HRESULT CreateMeshPipeline(ID3D12Device2 &device, ID3D12RootSignature *root,
                                  D3D12_SHADER_BYTECODE mesh, D3D12_SHADER_BYTECODE pixel,
                                  DXGI_FORMAT format, ID3D12PipelineState **result) {
  MeshStream stream;
  stream.root.data.value = root;
  stream.mesh.data.value = mesh;
  stream.pixel.data.value = pixel;
  stream.blend.data.value.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  stream.mask.data.value = std::numeric_limits<UINT>::max();
  stream.rasterizer.data.value.FillMode = D3D12_FILL_MODE_SOLID;
  stream.rasterizer.data.value.CullMode = D3D12_CULL_MODE_NONE;
  stream.rasterizer.data.value.DepthClipEnable = TRUE;
  stream.topology.data.value = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  stream.targets.data.value.NumRenderTargets = 1;
  stream.targets.data.value.RTFormats[0] = format;
  stream.samples.data.value = {1, 0};
  D3D12_PIPELINE_STATE_STREAM_DESC descriptor{sizeof(stream), &stream};
  return device.CreatePipelineState(&descriptor, __uuidof(ID3D12PipelineState),
                                    reinterpret_cast<void **>(result));
}
} // namespace nexora::rhi::detail
#endif
