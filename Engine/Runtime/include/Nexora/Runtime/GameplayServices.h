#pragma once
#include "nexora/nexora.h"
#include <cstring>
#include <thread>

namespace nexora::runtime {
// The embedding host owns all registered services through module unload. This aggregate only
// copies function tables; it never owns their contexts. Register before attaching modules.
class GameplayServices final {
public:
  NexoraGameplayInputApiV1 input{};
  NexoraGameplayUiApiV1 ui{};
  NexoraGameplayAudioApiV1 audio{};
  NexoraStorageApiV1 storage{};
  NexoraGameplayContentApiV1 content{};

  [[nodiscard]] int32_t GetApi(NexoraGameplayServicesApiV1 *output) noexcept {
    if (std::this_thread::get_id() != owner_)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    if (!output)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    if (output->struct_size < sizeof(*output) || output->abi_version != 1)
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    const NexoraGameplayServicesApiV1 negotiated_table{
        sizeof(NexoraGameplayServicesApiV1),
        1,
        this,
        [](void *p, NexoraGameplayInputApiV1 *v) { return Copy(p, v, &GameplayServices::input); },
        [](void *p, NexoraGameplayUiApiV1 *v) { return Copy(p, v, &GameplayServices::ui); },
        [](void *p, NexoraGameplayAudioApiV1 *v) { return Copy(p, v, &GameplayServices::audio); },
        [](void *p, NexoraStorageApiV1 *v) { return Copy(p, v, &GameplayServices::storage); },
        [](void *p, NexoraGameplayContentApiV1 *v) {
          return Copy(p, v, &GameplayServices::content);
        }};
    std::memcpy(output, &negotiated_table, sizeof(negotiated_table));
    return NEXORA_GAMEPLAY_OK;
  }

private:
  template <class T> static int32_t Copy(void *p, T *output, T GameplayServices::*member) noexcept {
    if (!p || !output)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    const auto &self = *static_cast<GameplayServices *>(p);
    if (std::this_thread::get_id() != self.owner_)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    const auto &registered_table = self.*member;
    if (registered_table.struct_size < sizeof(T) || registered_table.abi_version != 1 ||
        output->struct_size < sizeof(T) || output->abi_version != 1)
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    std::memcpy(output, &registered_table, sizeof(registered_table));
    return NEXORA_GAMEPLAY_OK;
  }
  const std::thread::id owner_{std::this_thread::get_id()};
};
} // namespace nexora::runtime
