#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Foundation/PluginAbi.h"

NEXORA_PLUGIN_ABI_EXPORT std::uint32_t NexoraPluginAbiVersion() noexcept {
  return nexora::foundation::kEngineAbiVersion;
}

namespace {
const char kExampleServiceMarker[] = "Nexora example plugin";
bool shutdown_requested{};
int32_t RequestShutdown(void *) noexcept {
  shutdown_requested = true;
  return 0;
}
int32_t Quiescent(void *) noexcept { return shutdown_requested ? 1 : 0; }
} // namespace

// Optional: proves the registration contract against a real built plugin,
// not a mock. A plugin that only needs to pass the ABI gate can omit this
// entirely.
NEXORA_PLUGIN_ABI_EXPORT void NexoraPluginRegister(void *context,
                                                   NexoraServiceRegisterCallback register_service) {
  shutdown_requested = false;
  register_service(context, "example.marker", const_cast<char *>(kExampleServiceMarker));
}

NEXORA_PLUGIN_ABI_EXPORT int32_t
NexoraPluginGetLifecycleV1(uint32_t requested, NexoraPluginLifecycleV1 *lifecycle) noexcept {
  if (!lifecycle || lifecycle->struct_size < sizeof(*lifecycle) ||
      requested != NEXORA_PLUGIN_LIFECYCLE_SCHEMA_V1)
    return -1;
  *lifecycle = {sizeof(*lifecycle), NEXORA_PLUGIN_LIFECYCLE_SCHEMA_V1, nullptr, RequestShutdown,
                Quiescent};
  return 0;
}
