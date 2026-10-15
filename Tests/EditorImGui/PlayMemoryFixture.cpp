#include "Nexora/Foundation/GameplayABI.h"
#include <cstdio>
#include <cstring>

namespace {
NexoraGameplayHostV3 host{};
void *retained{};
bool updated{};
int32_t Create(void **state, const NexoraGameplayHostV3 *api) {
  host = *api;
  updated = false;
  *state = host.allocate(host.context, 7, 64, 16);
  return *state ? NEXORA_GAMEPLAY_OK : NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
}
int32_t Start(void *) {
  retained = host.allocate(host.context, 8, 1024 * 1024, 16);
  if (!retained)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  std::memset(retained, 0x5a, 1024 * 1024);
  std::fprintf(stderr, "play memory fixture started live=1048640\n");
  return NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *state, double) {
  if (!updated) {
    void *extra = host.allocate(host.context, 9, 128, 16);
    if (!extra || host.allocate(host.context, 9, 16, 3))
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    host.deallocate(host.context, 8, state, 64, 16); // Rejected, correct release remains possible.
    host.deallocate(host.context, 9, extra, 128, 16);
    updated = true;
    std::fprintf(stderr, "play memory fixture updated peak=1048768 rejected=1,1\n");
  }
  return NEXORA_GAMEPLAY_OK;
}
void Stop(void *) {}
void Destroy(void *state) {
  bool pages = true;
  if (retained)
    for (unsigned offset = 0; offset < 1024 * 1024; offset += 4096)
      pages &= static_cast<unsigned char *>(retained)[offset] == 0x5a;
  host.deallocate(host.context, 7, state, 64, 16);
  // Deliberately retain one block to observe the embedding's fallback recovery in the GUI.
  std::fprintf(stderr, "play memory fixture destroyed retained=1048576 pages=%d\n", pages);
  retained = nullptr;
}
} // namespace
#if defined(_WIN32)
#define NEXORA_MEMORY_EXPORT __declspec(dllexport)
#else
#define NEXORA_MEMORY_EXPORT __attribute__((visibility("default")))
#endif
extern "C" NEXORA_MEMORY_EXPORT int32_t NexoraGameModuleLoad(uint32_t version,
                                                             NexoraGameModuleV3 *module) {
  if (!module || version != NEXORA_GAMEPLAY_ABI_VERSION)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->create = Create;
  module->on_start = Start;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
