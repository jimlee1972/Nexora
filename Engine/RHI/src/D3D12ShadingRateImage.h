#pragma once
#include "Nexora/RHI/ShadingRateImage.h"
#include <cstring>
#include <stdexcept>

// Included after the SDK and WRL headers. All image storage is immutable after construction.
#if defined(__ID3D12GraphicsCommandList5_INTERFACE_DEFINED__)
#define NEXORA_D3D12_VRS_IMAGE 1
namespace nexora::rhi::detail {
using Microsoft::WRL::ComPtr;
inline void CheckRateImage(HRESULT result, const char *operation) {
  if (FAILED(result))
    throw std::runtime_error(operation);
}
struct NativeShadingRateImage final {
  ShadingRateImageDescriptor descriptor;
  ComPtr<ID3D12Resource> texture;
};
struct PendingShadingRateImage final {
  NativeShadingRateImage image;
  ComPtr<ID3D12Resource> upload;
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> commands;
};
inline PendingShadingRateImage BuildShadingRateImage(ID3D12Device &device,
                                                     const ShadingRateImageDescriptor &descriptor,
                                                     UINT tile,
                                                     std::span<const std::uint8_t> rates) {
  if (!IsShadingRateImageShapeValid(descriptor, tile, rates.size()))
    throw std::invalid_argument("invalid rate-image raster/tile extent or byte count");
  for (const auto rate : rates)
    if (!IsShadingRateImageCodeValid(rate))
      throw std::invalid_argument("unsupported rate-image code");
  PendingShadingRateImage result;
  result.image.descriptor = descriptor;
  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = D3D12_HEAP_TYPE_DEFAULT;
  heap.CreationNodeMask = heap.VisibleNodeMask = 1;
  D3D12_RESOURCE_DESC texture{};
  texture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texture.Width = (descriptor.raster_width + tile - 1) / tile;
  texture.Height = (descriptor.raster_height + tile - 1) / tile;
  texture.DepthOrArraySize = texture.MipLevels = 1;
  texture.Format = DXGI_FORMAT_R8_UINT;
  texture.SampleDesc.Count = 1;
  texture.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  CheckRateImage(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &texture,
                                                D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                IID_PPV_ARGS(&result.image.texture)),
                 "Create immutable shading-rate texture");
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
  UINT rows{};
  UINT64 row_bytes{}, total{};
  device.GetCopyableFootprints(&texture, 0, 1, 0, &footprint, &rows, &row_bytes, &total);
  if (!total || total > 1024 * 1024 || rows != texture.Height || row_bytes != texture.Width ||
      footprint.Offset > total || row_bytes > total - footprint.Offset ||
      footprint.Footprint.RowPitch < row_bytes ||
      static_cast<UINT64>(rows - 1) * footprint.Footprint.RowPitch >
          total - footprint.Offset - row_bytes)
    throw std::invalid_argument("rate-image upload exceeds bounded footprint");
  D3D12_RESOURCE_DESC upload{};
  upload.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  upload.Width = total;
  upload.Height = upload.DepthOrArraySize = upload.MipLevels = 1;
  upload.SampleDesc.Count = 1;
  upload.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  heap.Type = D3D12_HEAP_TYPE_UPLOAD;
  CheckRateImage(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &upload,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                IID_PPV_ARGS(&result.upload)),
                 "Create rate-image upload");
  void *mapped{};
  const D3D12_RANGE read{0, 0};
  CheckRateImage(result.upload->Map(0, &read, &mapped), "Map rate-image upload");
  for (UINT y = 0; y < rows; ++y)
    std::memcpy(static_cast<std::byte *>(mapped) + footprint.Offset +
                    static_cast<std::size_t>(y) * footprint.Footprint.RowPitch,
                rates.data() + static_cast<std::size_t>(y) * texture.Width,
                static_cast<std::size_t>(texture.Width));
  const D3D12_RANGE written{0, static_cast<SIZE_T>(total)};
  result.upload->Unmap(0, &written);
  CheckRateImage(device.CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                               IID_PPV_ARGS(&result.allocator)),
                 "Create rate-image allocator");
  CheckRateImage(device.CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, result.allocator.Get(),
                                          nullptr, IID_PPV_ARGS(&result.commands)),
                 "Create rate-image upload list");
  D3D12_TEXTURE_COPY_LOCATION source{};
  source.pResource = result.upload.Get();
  source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  source.PlacedFootprint = footprint;
  D3D12_TEXTURE_COPY_LOCATION destination{};
  destination.pResource = result.image.texture.Get();
  destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  result.commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
  D3D12_RESOURCE_BARRIER ready{};
  ready.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  ready.Transition = {result.image.texture.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                      D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE};
  result.commands->ResourceBarrier(1, &ready);
  return result;
}
} // namespace nexora::rhi::detail
#else
#define NEXORA_D3D12_VRS_IMAGE 0
#endif
