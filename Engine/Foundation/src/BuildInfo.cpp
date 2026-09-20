#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Foundation/Version.h"

namespace nexora::foundation {
namespace {
#if defined(NEXORA_BUILD_DEBUG)
constexpr auto kConfiguration = "Debug";
#elif defined(NEXORA_BUILD_SHIPPING)
constexpr auto kConfiguration = "Shipping";
#else
constexpr auto kConfiguration = "Development";
#endif

#if defined(NEXORA_MONOLITHIC)
constexpr auto kLinkMode = "Monolithic";
#else
constexpr auto kLinkMode = "Modular";
#endif
} // namespace

BuildInfo GetBuildInfo() noexcept {
  return {NEXORA_ENGINE_VERSION, kEngineAbiVersion, kConfiguration, kLinkMode};
}
} // namespace nexora::foundation
