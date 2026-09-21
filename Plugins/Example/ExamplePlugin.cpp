#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Foundation/PluginAbi.h"

NEXORA_PLUGIN_ABI_EXPORT std::uint32_t NexoraPluginAbiVersion() noexcept {
  return nexora::foundation::kEngineAbiVersion;
}

namespace {
const char kExampleServiceMarker[] = "Nexora example plugin";
}

// Optional: proves the registration contract against a real built plugin,
// not a mock. A plugin that only needs to pass the ABI gate can omit this
// entirely.
NEXORA_PLUGIN_ABI_EXPORT void NexoraPluginRegister(void *context,
                                                   NexoraServiceRegisterCallback register_service) {
  register_service(context, "example.marker", const_cast<char *>(kExampleServiceMarker));
}
