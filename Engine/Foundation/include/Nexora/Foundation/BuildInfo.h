#pragma once

#include <cstdint>

#if defined(NEXORA_FOUNDATION_STATIC)
#define NEXORA_FOUNDATION_API
#elif defined(_WIN32)
#if defined(NEXORA_FOUNDATION_EXPORTS)
#define NEXORA_FOUNDATION_API __declspec(dllexport)
#else
#define NEXORA_FOUNDATION_API __declspec(dllimport)
#endif
#else
#define NEXORA_FOUNDATION_API __attribute__((visibility("default")))
#endif

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
