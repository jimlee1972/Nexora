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
  NexoraGameplayDisplayApiV1 display{};
  NexoraGameplayUiDrawApiV1 ui_draw{};
  NexoraGameplayAssetsApiV1 assets{};
  NexoraGameplayAudioStateApiV1 audio_state{};

  [[nodiscard]] int32_t GetApi(NexoraGameplayServicesApiV1 *output) noexcept {
    if (std::this_thread::get_id() != owner_)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    if (!output)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    if (output->struct_size < NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE || output->abi_version != 1)
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    const auto bytes =
        output->struct_size >= sizeof(*output) ? sizeof(*output)
        : output->struct_size >= offsetof(NexoraGameplayServicesApiV1, get_audio_state_api)
            ? offsetof(NexoraGameplayServicesApiV1, get_audio_state_api)
        : output->struct_size >= offsetof(NexoraGameplayServicesApiV1, get_assets_api)
            ? offsetof(NexoraGameplayServicesApiV1, get_assets_api)
        : output->struct_size >= offsetof(NexoraGameplayServicesApiV1, get_ui_draw_api)
            ? offsetof(NexoraGameplayServicesApiV1, get_ui_draw_api)
            : NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE;
    const NexoraGameplayServicesApiV1 negotiated_table{
        static_cast<uint32_t>(bytes),
        1,
        this,
        [](void *p, NexoraGameplayInputApiV1 *v) { return Copy(p, v, &GameplayServices::input); },
        [](void *p, NexoraGameplayUiApiV1 *v) { return Copy(p, v, &GameplayServices::ui); },
        [](void *p, NexoraGameplayAudioApiV1 *v) { return Copy(p, v, &GameplayServices::audio); },
        [](void *p, NexoraStorageApiV1 *v) { return Copy(p, v, &GameplayServices::storage); },
        [](void *p, NexoraGameplayContentApiV1 *v) {
          return Copy(p, v, &GameplayServices::content);
        },
        [](void *p, NexoraGameplayDisplayApiV1 *v) {
          return Copy(p, v, &GameplayServices::display);
        },
        [](void *p, NexoraGameplayUiDrawApiV1 *v) {
          return Copy(p, v, &GameplayServices::ui_draw);
        },
        [](void *p, NexoraGameplayAssetsApiV1 *v) { return Copy(p, v, &GameplayServices::assets); },
        [](void *p, NexoraGameplayAudioStateApiV1 *v) {
          return Copy(p, v, &GameplayServices::audio_state);
        }};
    std::memcpy(output, &negotiated_table, bytes);
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
