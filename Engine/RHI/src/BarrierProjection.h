#pragma once

#include "Nexora/RHI/Types.h"
#include <optional>

namespace nexora::rhi::detail {
// SDK-independent whole-resource policy for the operations currently admitted by RHI.
// All is deliberate for layout-only points: NONE would prohibit any later barrier in the
// same ExecuteCommandLists scope, while RHI permits multiple rendering/layout scopes.
enum class BarrierSyncDomain : std::uint8_t { All, Copy, Shading, RenderTarget, Indirect };
enum class BarrierMemoryAccess : std::uint8_t {
  NoAccess,
  CopySource,
  CopyDestination,
  ShaderRead,
  ShaderReadWrite,
  RenderTarget,
  IndirectRead
};
enum class BarrierTextureLayout : std::uint8_t {
  Undefined,
  Common,
  CopySource,
  CopyDestination,
  ShaderRead,
  RenderTarget
};
struct BarrierPoint final {
  BarrierSyncDomain sync;
  BarrierMemoryAccess access;
  BarrierTextureLayout layout;
  friend bool operator==(const BarrierPoint &, const BarrierPoint &) = default;
};
inline std::optional<BarrierPoint> TextureBarrierPoint(ResourceState state) noexcept {
  switch (state) {
  case ResourceState::Undefined:
    return BarrierPoint{BarrierSyncDomain::All, BarrierMemoryAccess::NoAccess,
                        BarrierTextureLayout::Undefined};
  case ResourceState::Present:
    return BarrierPoint{BarrierSyncDomain::All, BarrierMemoryAccess::NoAccess,
                        BarrierTextureLayout::Common};
  case ResourceState::CopySource:
    return BarrierPoint{BarrierSyncDomain::Copy, BarrierMemoryAccess::CopySource,
                        BarrierTextureLayout::CopySource};
  case ResourceState::CopyDestination:
    return BarrierPoint{BarrierSyncDomain::Copy, BarrierMemoryAccess::CopyDestination,
                        BarrierTextureLayout::CopyDestination};
  case ResourceState::ShaderRead:
    return BarrierPoint{BarrierSyncDomain::Shading, BarrierMemoryAccess::ShaderRead,
                        BarrierTextureLayout::ShaderRead};
  case ResourceState::RenderTarget:
    return BarrierPoint{BarrierSyncDomain::RenderTarget, BarrierMemoryAccess::RenderTarget,
                        BarrierTextureLayout::RenderTarget};
  }
  return std::nullopt;
}
inline std::optional<BarrierPoint> BufferBarrierPoint(BarrierMemoryAccess access) noexcept {
  const auto layout = BarrierTextureLayout::Common; // Buffers have no physical texture layout.
  switch (access) {
  case BarrierMemoryAccess::NoAccess:
    return BarrierPoint{BarrierSyncDomain::All, access, layout};
  case BarrierMemoryAccess::CopySource:
  case BarrierMemoryAccess::CopyDestination:
    return BarrierPoint{BarrierSyncDomain::Copy, access, layout};
  case BarrierMemoryAccess::ShaderRead:
  case BarrierMemoryAccess::ShaderReadWrite:
    return BarrierPoint{BarrierSyncDomain::Shading, access, layout};
  case BarrierMemoryAccess::IndirectRead:
    return BarrierPoint{BarrierSyncDomain::Indirect, access, layout};
  case BarrierMemoryAccess::RenderTarget:
    break;
  }
  return std::nullopt;
}
} // namespace nexora::rhi::detail
