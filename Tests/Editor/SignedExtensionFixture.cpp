#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Foundation/PluginAbi.h"
#include <cstdlib>
#include <fstream>

namespace {
constexpr int mode = NEXORA_SIGNED_FIXTURE_MODE;
bool stopped{};
void Event(const char *event) noexcept {
  try {
    if (const auto *path = std::getenv("NEXORA_SIGNED_FIXTURE_EVENTS")) {
      std::ofstream output(path, std::ios::binary | std::ios::app);
      output << mode << ' ' << event << '\n';
    }
  } catch (...) {
  }
}
struct Lifetime final {
  Lifetime() { Event("initialize"); }
  ~Lifetime() { Event("unload"); }
} lifetime;
int32_t Stop(void *) {
  stopped = true;
  Event("stop");
  return 0;
}
int32_t Poll(void *) { return stopped ? 1 : 0; }
} // namespace
NEXORA_PLUGIN_ABI_EXPORT uint32_t NexoraPluginAbiVersion() noexcept {
  return nexora::foundation::kEngineAbiVersion + (mode == 3 ? 1U : 0U);
}
NEXORA_PLUGIN_ABI_EXPORT void NexoraPluginRegister(void *context,
                                                   NexoraServiceRegisterCallback callback) {
  Event("register");
  stopped = false;
  static char marker[] = {static_cast<char>('A' + mode - 1), '\0'};
  callback(context, mode == 2 ? "signed.B" : "signed.A", marker);
}
NEXORA_PLUGIN_ABI_EXPORT int32_t
NexoraPluginGetLifecycleV1(uint32_t requested, NexoraPluginLifecycleV1 *result) noexcept {
  if (!result || requested != 1 || result->struct_size < sizeof(*result))
    return -1;
  *result = {sizeof(*result), 1, nullptr, Stop, Poll};
  return 0;
}
