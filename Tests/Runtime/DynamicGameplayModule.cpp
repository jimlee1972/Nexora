#include "Nexora/Foundation/GameplayABI.h"

#include <cstdint>
#include <cstring>

#ifndef NEXORA_TEST_GENERATION
#define NEXORA_TEST_GENERATION 1
#endif

namespace {

struct State final {
  std::uint32_t value{NEXORA_TEST_GENERATION};
};

State state;

#if defined(NEXORA_TEST_HOST_FINALIZER)
const NexoraGameplayHostV3 *retained_host{};
struct Finalizer final {
  ~Finalizer() {
    if (retained_host && retained_host->log) {
      constexpr char message[] = "candidate finalizer";
      retained_host->log(retained_host->context, 1, message, sizeof(message) - 1);
    }
  }
} finalizer;
#endif

int32_t Create(void **output, [[maybe_unused]] const NexoraGameplayHostV3 *host) {
#if defined(NEXORA_TEST_HOST_FINALIZER)
  retained_host = host;
#endif
  state = {};
  *output = &state;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *) { return NEXORA_GAMEPLAY_OK; }
int32_t Update(void *opaque, double) {
  ++static_cast<State *>(opaque)->value;
  return NEXORA_GAMEPLAY_OK;
}
void Stop(void *) {}
void Destroy(void *) {}
uint32_t Save(void *opaque, void *data, uint32_t size) {
  if (data != nullptr && size >= sizeof(State))
    std::memcpy(data, opaque, sizeof(State));
  return sizeof(State);
}
int32_t Restore(void *opaque, const void *data, uint32_t size) {
#if defined(NEXORA_TEST_FAIL_RESTORE)
  (void)opaque;
  (void)data;
  (void)size;
  return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
#else
  if (size != sizeof(State))
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  std::memcpy(opaque, data, sizeof(State));
  return NEXORA_GAMEPLAY_OK;
#endif
}

} // namespace

#if defined(_WIN32)
#define NEXORA_TEST_MODULE_EXPORT __declspec(dllexport)
#else
#define NEXORA_TEST_MODULE_EXPORT __attribute__((visibility("default")))
#endif

extern "C" NEXORA_TEST_MODULE_EXPORT int32_t NexoraGameModuleLoad(uint32_t requested,
                                                                  NexoraGameModuleV3 *module) {
  if (requested != NEXORA_GAMEPLAY_ABI_VERSION || module == nullptr)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  *module = {sizeof(*module),
             NEXORA_GAMEPLAY_ABI_VERSION,
             NEXORA_GAMEPLAY_CAPABILITY_STATE_MIGRATION,
             nullptr,
             &Create,
             &Start,
             nullptr,
             &Update,
             &Stop,
             &Destroy,
             &Save,
             &Restore};
#if defined(NEXORA_TEST_BAD_ABI)
  ++module->abi_version;
#endif
  return NEXORA_GAMEPLAY_OK;
}
