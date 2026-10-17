#pragma once

#include <limits>

#if defined(__ID3D12GraphicsCommandList6_INTERFACE_DEFINED__) &&                                   \
    defined(__ID3D12Device2_INTERFACE_DEFINED__)
namespace nexora::rhi::detail {
// Each subobject starts at pointer alignment, including its payload's trailing padding.
template <D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Kind, class Payload>
struct alignas(void *) Subobject {
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE kind{Kind};
  Payload value{};
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
  stream.root.value = root;
  stream.mesh.value = mesh;
  stream.pixel.value = pixel;
  stream.blend.value.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  stream.mask.value = std::numeric_limits<UINT>::max();
  stream.rasterizer.value.FillMode = D3D12_FILL_MODE_SOLID;
  stream.rasterizer.value.CullMode = D3D12_CULL_MODE_NONE;
  stream.rasterizer.value.DepthClipEnable = TRUE;
  stream.topology.value = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  stream.targets.value.NumRenderTargets = 1;
  stream.targets.value.RTFormats[0] = format;
  stream.samples.value = {1, 0};
  D3D12_PIPELINE_STATE_STREAM_DESC descriptor{sizeof(stream), &stream};
  return device.CreatePipelineState(&descriptor, __uuidof(ID3D12PipelineState),
                                    reinterpret_cast<void **>(result));
}
} // namespace nexora::rhi::detail
#endif
