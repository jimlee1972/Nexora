#include "Nexora/Foundation/GameplayABI.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(std::is_standard_layout_v<NexoraGameplayHostV1>);
static_assert(std::is_standard_layout_v<NexoraGameModuleV1>);
static_assert(offsetof(NexoraGameplayHostV1, struct_size) == 0);
static_assert(offsetof(NexoraGameplayHostV1, abi_version) == sizeof(std::uint32_t));
static_assert(offsetof(NexoraGameModuleV1, struct_size) == 0);
static_assert(NEXORA_GAMEPLAY_ABI_VERSION == 1U);

int main() {
  NexoraGameModuleV1 module{};
  module.struct_size = sizeof(module);
  module.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  return module.struct_size >= sizeof(std::uint32_t) * 2 && module.abi_version == 1 ? 0 : 1;
}
