#pragma once

#include "Nexora/Foundation/Api.h"

#include <cstdint>

namespace nexora::foundation {

inline constexpr std::uint32_t kEngineAbiVersion = 1;

struct BuildInfo final {
  const char *engine_version;
  std::uint32_t abi_version;
  const char *build_configuration;
  const char *link_mode;
};

[[nodiscard]] NEXORA_FOUNDATION_API BuildInfo GetBuildInfo() noexcept;

// Separate accessor so existing binaries linked against GetBuildInfo()'s
// by-value BuildInfo layout are unaffected by adding build-id lookup.
[[nodiscard]] NEXORA_FOUNDATION_API const char *GetBuildId() noexcept;

} // namespace nexora::foundation
