#pragma once

#include "BarrierProjection.h"
#include <limits>
#include <stdexcept>

// Included after d3d12.h, so old SDK builds retain the existing legacy implementation.
#if defined(__ID3D12GraphicsCommandList7_INTERFACE_DEFINED__)
namespace nexora::rhi::detail {
inline D3D12_BARRIER_SYNC NativeSync(BarrierSyncDomain sync) {
  switch (sync) {
  case BarrierSyncDomain::All:
    return D3D12_BARRIER_SYNC_ALL;
  case BarrierSyncDomain::Copy:
    return D3D12_BARRIER_SYNC_COPY;
  case BarrierSyncDomain::Shading:
    // DRAW also finalizes DATA_STATIC_WHILE_SET_AT_EXECUTE graphics descriptors;
    // ALL_SHADING alone does not include its front-end synchronization scope.
    return static_cast<D3D12_BARRIER_SYNC>(D3D12_BARRIER_SYNC_ALL_SHADING |
                                           D3D12_BARRIER_SYNC_DRAW);
  case BarrierSyncDomain::RenderTarget:
    return D3D12_BARRIER_SYNC_RENDER_TARGET;
  case BarrierSyncDomain::Indirect:
    return D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
  }
  throw std::invalid_argument("invalid enhanced synchronization scope");
}
inline D3D12_BARRIER_ACCESS NativeAccess(BarrierMemoryAccess access) {
  switch (access) {
  case BarrierMemoryAccess::NoAccess:
    return D3D12_BARRIER_ACCESS_NO_ACCESS;
  case BarrierMemoryAccess::CopySource:
    return D3D12_BARRIER_ACCESS_COPY_SOURCE;
  case BarrierMemoryAccess::CopyDestination:
    return D3D12_BARRIER_ACCESS_COPY_DEST;
  case BarrierMemoryAccess::ShaderRead:
    return D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
  case BarrierMemoryAccess::ShaderReadWrite:
    return D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
  case BarrierMemoryAccess::RenderTarget:
    return D3D12_BARRIER_ACCESS_RENDER_TARGET;
  case BarrierMemoryAccess::IndirectRead:
    return D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;
  }
  throw std::invalid_argument("invalid enhanced memory access");
}
inline D3D12_BARRIER_LAYOUT NativeLayout(BarrierTextureLayout layout) {
  switch (layout) {
  case BarrierTextureLayout::Undefined:
    return D3D12_BARRIER_LAYOUT_UNDEFINED;
  case BarrierTextureLayout::Common:
    return D3D12_BARRIER_LAYOUT_COMMON;
  case BarrierTextureLayout::CopySource:
    return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
  case BarrierTextureLayout::CopyDestination:
    return D3D12_BARRIER_LAYOUT_COPY_DEST;
  case BarrierTextureLayout::ShaderRead:
    return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
  case BarrierTextureLayout::RenderTarget:
    return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
  }
  throw std::invalid_argument("invalid enhanced texture layout");
}
inline BarrierMemoryAccess BufferMemoryAccess(D3D12_RESOURCE_STATES state) {
  constexpr auto shader_reads = static_cast<UINT>(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) |
                                static_cast<UINT>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  const auto bits = static_cast<UINT>(state);
  // Graph ShaderRead admits both shader domains; individual compute/MS bindings use NON_PIXEL.
  if (bits != 0 && (bits & ~shader_reads) == 0)
    return BarrierMemoryAccess::ShaderRead;
  switch (state) {
  case D3D12_RESOURCE_STATE_COMMON:
    return BarrierMemoryAccess::NoAccess;
  case D3D12_RESOURCE_STATE_COPY_SOURCE:
    return BarrierMemoryAccess::CopySource;
  case D3D12_RESOURCE_STATE_COPY_DEST:
    return BarrierMemoryAccess::CopyDestination;
  case D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
    return BarrierMemoryAccess::ShaderReadWrite;
  case D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT:
    return BarrierMemoryAccess::IndirectRead;
  default:
    throw std::invalid_argument("buffer state has no admitted enhanced projection");
  }
}
inline void EmitEnhancedBufferBarrier(ID3D12GraphicsCommandList7 &commands,
                                      ID3D12Resource *resource, D3D12_RESOURCE_STATES before,
                                      D3D12_RESOURCE_STATES after) {
  const auto source = BufferBarrierPoint(BufferMemoryAccess(before));
  const auto target = BufferBarrierPoint(BufferMemoryAccess(after));
  if (!source || !target)
    throw std::invalid_argument("invalid enhanced buffer barrier");
  D3D12_BUFFER_BARRIER barrier{};
  barrier.SyncBefore = NativeSync(source->sync);
  barrier.SyncAfter = NativeSync(target->sync);
  barrier.AccessBefore = NativeAccess(source->access);
  barrier.AccessAfter = NativeAccess(target->access);
  barrier.pResource = resource;
  barrier.Offset = 0;
  barrier.Size = std::numeric_limits<UINT64>::max();
  D3D12_BARRIER_GROUP group{};
  group.Type = D3D12_BARRIER_TYPE_BUFFER;
  group.NumBarriers = 1;
  group.pBufferBarriers = &barrier;
  commands.Barrier(1, &group);
}
inline void EmitEnhancedTextureBarrier(ID3D12GraphicsCommandList7 &commands,
                                       ID3D12Resource *resource, ResourceState before,
                                       ResourceState after) {
  const auto source = TextureBarrierPoint(before), target = TextureBarrierPoint(after);
  if (!source || !target || after == ResourceState::Undefined)
    throw std::invalid_argument("invalid enhanced texture barrier");
  D3D12_TEXTURE_BARRIER barrier{};
  barrier.SyncBefore = NativeSync(source->sync);
  barrier.SyncAfter = NativeSync(target->sync);
  barrier.AccessBefore = NativeAccess(source->access);
  barrier.AccessAfter = NativeAccess(target->access);
  barrier.LayoutBefore = NativeLayout(source->layout);
  barrier.LayoutAfter = NativeLayout(target->layout);
  barrier.pResource = resource;
  barrier.Subresources.IndexOrFirstMipLevel = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  barrier.Flags = before == ResourceState::Undefined ? D3D12_TEXTURE_BARRIER_FLAG_DISCARD
                                                     : D3D12_TEXTURE_BARRIER_FLAG_NONE;
  D3D12_BARRIER_GROUP group{};
  group.Type = D3D12_BARRIER_TYPE_TEXTURE;
  group.NumBarriers = 1;
  group.pTextureBarriers = &barrier;
  commands.Barrier(1, &group);
}
} // namespace nexora::rhi::detail
#endif
