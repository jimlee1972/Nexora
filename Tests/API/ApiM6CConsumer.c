#include "nexora/nexora.h"

#include <stddef.h>

_Static_assert(NEXORA_GAMEPLAY_ABI_VERSION == 3u, "unexpected ABI major");
_Static_assert(offsetof(NexoraGameplayHostV3, struct_size) == 0, "descriptor prefix changed");
_Static_assert(offsetof(NexoraGameplayHostV3, abi_version) == sizeof(uint32_t),
               "descriptor prefix changed");
_Static_assert(offsetof(NexoraGameModuleV3, struct_size) == 0, "descriptor prefix changed");
_Static_assert(offsetof(NexoraGameModuleV3, abi_version) == sizeof(uint32_t),
               "descriptor prefix changed");

int main(void) {
  NexoraGameplayHostV3 host = {0};
  NexoraGameModuleV3 module = {0};
  host.struct_size = (uint32_t)sizeof(host);
  host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  module.struct_size = (uint32_t)sizeof(module);
  module.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  return host.struct_size == 0 || module.struct_size == 0;
}
