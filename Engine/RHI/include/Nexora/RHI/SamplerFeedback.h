#pragma once
#include "Nexora/RHI/Types.h"
namespace nexora::rhi {
struct SamplerFeedbackProgramTag;
using SamplerFeedbackProgramHandle = core::Handle<SamplerFeedbackProgramTag>;
inline constexpr std::uint32_t SamplerFeedbackTextureExtent = 64;
inline constexpr std::uint32_t SamplerFeedbackMipLevels = 7;
inline constexpr std::uint32_t SamplerFeedbackRegionExtent = 16;
inline constexpr std::uint32_t SamplerFeedbackMapExtent = 4;
inline constexpr std::uint32_t SamplerFeedbackMaximumRequests = 64;
inline constexpr std::uint32_t SamplerFeedbackMaximumDispatchesPerList = 4;
struct SamplerFeedbackRequest final {
  float u{}, v{};
  std::uint32_t mip{};
  std::uint32_t enabled{1};
};
static_assert(sizeof(SamplerFeedbackRequest) == 16 && offsetof(SamplerFeedbackRequest, mip) == 8 &&
              offsetof(SamplerFeedbackRequest, enabled) == 12);
struct SamplerFeedbackProgramDescriptor final {
  GraphicsRequirements graphics_requirements{};
  std::string debug_name;
};
} // namespace nexora::rhi
