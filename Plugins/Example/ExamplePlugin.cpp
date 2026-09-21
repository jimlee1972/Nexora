#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Foundation/PluginAbi.h"

NEXORA_PLUGIN_ABI_EXPORT std::uint32_t NexoraPluginAbiVersion() noexcept {
  return nexora::foundation::kEngineAbiVersion;
}
