#include "Nexora/Foundation/GameplayABI.h"
#include <cstdint>
#include <cstring>

namespace {
const NexoraGameplayHostV3 *host{};
void Crash() {
  volatile std::uintptr_t address{};
  *reinterpret_cast<volatile int *>(address) = 42;
}
int32_t Create(void **state, const NexoraGameplayHostV3 *input) {
  host = input;
  *state = input->context;
  if (std::strcmp(static_cast<const char *>(*state), "create-crash") == 0)
    Crash();
  return 0;
}
int32_t Start(void *state) {
  if (std::strcmp(static_cast<const char *>(state), "start-crash") == 0)
    Crash();
  return 0;
}
int32_t Update(void *state, double) {
  const auto *mode = static_cast<const char *>(state);
  constexpr char message[] = "gameplay breadcrumb before failure";
  host->log(host->context, 2, message, sizeof(message) - 1);
  if (std::strcmp(mode, "update-crash") == 0 || std::strcmp(mode, "fixed-crash") == 0)
    Crash();
  return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
}
void Stop(void *state) {
  if (std::strcmp(static_cast<const char *>(state), "stop-crash") == 0)
    Crash();
}
void Destroy(void *state) {
  if (std::strcmp(static_cast<const char *>(state), "destroy-crash") == 0)
    Crash();
}
} // namespace
#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
extern "C" EXPORT int32_t NexoraGameModuleLoad(uint32_t requested, NexoraGameModuleV3 *module) {
  if (requested != NEXORA_GAMEPLAY_ABI_VERSION || !module)
    return -1;
  *module = {sizeof(*module),
             NEXORA_GAMEPLAY_ABI_VERSION,
             NEXORA_GAMEPLAY_CAPABILITY_FIXED_UPDATE,
             nullptr,
             Create,
             Start,
             Update,
             Update,
             Stop,
             Destroy,
             nullptr,
             nullptr};
  return 0;
}
