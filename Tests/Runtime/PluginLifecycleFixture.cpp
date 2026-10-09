#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Foundation/PluginAbi.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
constexpr int kMode = NEXORA_PLUGIN_FIXTURE_MODE;
std::atomic<bool> released{}, exited{};
std::thread worker;
bool stopped{};
const char marker[] = "Actual native lifecycle fixture";
void Event(const char *event) noexcept {
  try {
    const auto *path = std::getenv("NEXORA_PLUGIN_EVENT_FILE");
    if (!path)
      return;
    std::ofstream output(
        std::filesystem::path{std::u8string(path, path + std::char_traits<char>::length(path))},
        std::ios::app);
    output << kMode << ' ' << event << '\n';
  } catch (...) {
  }
}
struct NativeLifetime final {
  ~NativeLifetime() {
    released.store(true);
    if (worker.joinable())
      worker.join();
    Event("native-unload");
  }
} native_lifetime;
#if NEXORA_PLUGIN_FIXTURE_MODE != 3
int32_t Request(void *) {
  Event("request");
  stopped = true;
  return kMode == 9 ? -7 : 0;
}
int32_t Poll(void *) {
  Event("poll");
  if (kMode == 10)
    return -8;
  if (kMode == 2 && worker.joinable()) {
    // The first poll releases real background work, but proves no quiescence until it exited.
    if (!released.exchange(true))
      return 0;
    if (!exited.load())
      return 0;
    worker.join();
  }
  return stopped ? 1 : 0;
}
#endif
} // namespace

#if NEXORA_PLUGIN_FIXTURE_MODE != 4
NEXORA_PLUGIN_ABI_EXPORT uint32_t NexoraPluginAbiVersion() noexcept {
  return nexora::foundation::kEngineAbiVersion + (kMode == 5 ? 1U : 0U);
}
#endif
NEXORA_PLUGIN_ABI_EXPORT void
NexoraPluginRegister(void *context, NexoraServiceRegisterCallback callback) noexcept(false) {
  Event("register");
  stopped = false;
  if (kMode == 12 || kMode == 13) {
    for (int i = 0; i < 64; ++i) {
      const auto name =
          i == 0 ? std::string(254, 'x') + "\xC2\xB5" : "fixture.service." + std::to_string(i);
      callback(context, name.c_str(), const_cast<char *>(marker));
    }
    if (kMode == 12)
      callback(context, reinterpret_cast<const char *>(std::uintptr_t{1}),
               const_cast<char *>(marker)); // Reject the quota before reading this pointer.
    return;
  }
  callback(context, "fixture.marker", const_cast<char *>(marker));
  if (kMode == 14) {
    const std::string oversized(257, 'x');
    callback(context, oversized.c_str(), const_cast<char *>(marker));
    callback(context, "\xFF", const_cast<char *>(marker));
    callback(context, nullptr, const_cast<char *>(marker));
    callback(context, "null.service", nullptr);
  }
  if (kMode == 11)
    throw std::runtime_error("fixture registration exception");
  if (kMode == 2) {
    released.store(false);
    exited.store(false);
    worker = std::thread([] {
      while (!released.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      exited.store(true);
    });
  }
}
#if NEXORA_PLUGIN_FIXTURE_MODE != 3
NEXORA_PLUGIN_ABI_EXPORT int32_t
NexoraPluginGetLifecycleV1(uint32_t requested, NexoraPluginLifecycleV1 *lifecycle) noexcept {
  Event("inspect");
  if (!lifecycle || lifecycle->struct_size < sizeof(*lifecycle) || requested != 1)
    return -1;
  *lifecycle = {sizeof(*lifecycle), 1, nullptr, Request, Poll};
  if (kMode == 6)
    lifecycle->schema_version = 2;
  if (kMode == 7)
    lifecycle->struct_size = sizeof(*lifecycle) - 1;
  if (kMode == 8)
    lifecycle->poll_quiescence = nullptr;
  return 0;
}
#endif
