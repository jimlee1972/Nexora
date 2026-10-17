#include "BarrierProjection.h"
#include "Nexora/RHI/Device.h"
#include <array>
#include <iostream>
#include <stdexcept>
#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <d3d12.h>

// This private adapter must see the SDK interface guards before it is parsed.
#include "D3D12EnhancedBarriers.h"
#endif
using namespace nexora::rhi;
using namespace nexora::rhi::detail;
namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace
int main() {
  try {
#if defined(__ID3D12GraphicsCommandList7_INTERFACE_DEFINED__)
    for (const auto bits : {static_cast<UINT>(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
                            static_cast<UINT>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
                            static_cast<UINT>(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) |
                                static_cast<UINT>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)})
      Require(BufferMemoryAccess(static_cast<D3D12_RESOURCE_STATES>(bits)) ==
                  BarrierMemoryAccess::ShaderRead,
              "native graph ShaderRead must admit combined pixel/non-pixel scopes");
    bool mixed_rejected = false;
    try {
      (void)BufferMemoryAccess(static_cast<D3D12_RESOURCE_STATES>(
          static_cast<UINT>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) |
          static_cast<UINT>(D3D12_RESOURCE_STATE_UNORDERED_ACCESS)));
    } catch (const std::invalid_argument &) {
      mixed_rejected = true;
    }
    Require(mixed_rejected, "unsupported native read/write combination admitted");
#endif
    const auto copy = TextureBarrierPoint(ResourceState::CopyDestination);
    const auto target = TextureBarrierPoint(ResourceState::RenderTarget);
    const auto sample = TextureBarrierPoint(ResourceState::ShaderRead);
    Require(copy && copy->sync == BarrierSyncDomain::Copy &&
                copy->access == BarrierMemoryAccess::CopyDestination &&
                copy->layout == BarrierTextureLayout::CopyDestination,
            "copy projection lost independent scopes");
    Require(target && target->sync == BarrierSyncDomain::RenderTarget &&
                target->layout == BarrierTextureLayout::RenderTarget,
            "render target projection");
    Require(sample && sample->sync == BarrierSyncDomain::Shading &&
                sample->access == BarrierMemoryAccess::ShaderRead,
            "sample projection");
    for (auto state : {ResourceState::Undefined, ResourceState::Present}) {
      auto point = TextureBarrierPoint(state);
      Require(point && point->access == BarrierMemoryAccess::NoAccess &&
                  point->sync == BarrierSyncDomain::All,
              "layout-only points must permit later barriers and avoid COMMON write flushes");
    }
    Require(!TextureBarrierPoint(static_cast<ResourceState>(255)),
            "unknown texture state admitted");
    for (auto access : {BarrierMemoryAccess::NoAccess, BarrierMemoryAccess::CopySource,
                        BarrierMemoryAccess::CopyDestination, BarrierMemoryAccess::ShaderRead,
                        BarrierMemoryAccess::ShaderReadWrite, BarrierMemoryAccess::IndirectRead}) {
      auto point = BufferBarrierPoint(access);
      Require(point && point->layout == BarrierTextureLayout::Common && point->access == access,
              "buffer memory usage projection");
    }
    const auto uav = BufferBarrierPoint(BarrierMemoryAccess::ShaderReadWrite);
    Require(uav && uav->sync == BarrierSyncDomain::Shading, "UAV/WAW must synchronize shading");
    const auto indirect = BufferBarrierPoint(BarrierMemoryAccess::IndirectRead);
    Require(indirect && indirect->sync == BarrierSyncDomain::Indirect, "indirect execution scope");
    Require(!BufferBarrierPoint(BarrierMemoryAccess::RenderTarget) &&
                !BufferBarrierPoint(static_cast<BarrierMemoryAccess>(255)),
            "unsupported buffer access admitted");
    auto null = CreateDeviceWithOptions(Backend::Null, {false, false, true});
    const auto cap =
        null->Capabilities().features[static_cast<std::size_t>(GraphicsFeature::EnhancedBarriers)];
    Require(!cap.enabled && !cap.implemented &&
                null->Diagnostics().enhanced_texture_barriers == 0 &&
                null->Diagnostics().enhanced_buffer_barriers == 0,
            "Null fabricated native enhanced barrier support");
    std::cout << "Portable barrier sync/access/layout projection and explicit native "
                 "unavailability passed\n";
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
