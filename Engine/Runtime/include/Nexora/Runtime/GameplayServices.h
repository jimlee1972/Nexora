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
  NexoraGameplayLifecycleApiV1 lifecycle{};
  NexoraGameplaySceneApiV1 scene{};
  NexoraGameplayTwoDEffectsApiV1 twod_effects{};

  [[nodiscard]] int32_t GetApi(NexoraGameplayServicesApiV1 *output) noexcept {
    if (std::this_thread::get_id() != owner_)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    if (!output)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    if (output->struct_size < NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE || output->abi_version != 1)
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    const auto bytes =
        output->struct_size >= sizeof(*output) ? sizeof(*output)
        : output->struct_size >= offsetof(NexoraGameplayServicesApiV1, get_twod_effects_api)
            ? offsetof(NexoraGameplayServicesApiV1, get_twod_effects_api)
        : output->struct_size >= offsetof(NexoraGameplayServicesApiV1, get_scene_api)
            ? offsetof(NexoraGameplayServicesApiV1, get_scene_api)
        : output->struct_size >= offsetof(NexoraGameplayServicesApiV1, get_lifecycle_api)
            ? offsetof(NexoraGameplayServicesApiV1, get_lifecycle_api)
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
        },
        [](void *p, NexoraGameplayLifecycleApiV1 *v) {
          return Copy(p, v, &GameplayServices::lifecycle);
        },
        [](void *p, NexoraGameplaySceneApiV1 *v) { return Copy(p, v, &GameplayServices::scene); },
        [](void *p, NexoraGameplayTwoDEffectsApiV1 *v) { return CopyEffects(p, v); }};
    std::memcpy(output, &negotiated_table, bytes);
    return NEXORA_GAMEPLAY_OK;
  }

private:
  static int32_t CopyEffects(void *p, NexoraGameplayTwoDEffectsApiV1 *output) noexcept {
    if (!p || !output)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    const auto &self = *static_cast<GameplayServices *>(p);
    if (std::this_thread::get_id() != self.owner_)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    constexpr auto prefix = offsetof(NexoraGameplayTwoDEffectsApiV1, pool_identity);
    const auto &registered = self.twod_effects;
    if (registered.struct_size < prefix || registered.abi_version != 1 ||
        output->struct_size < prefix || output->abi_version != 1)
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
    const auto bytes =
        registered.struct_size >= sizeof(registered) && output->struct_size >= sizeof(registered)
            ? sizeof(registered)
            : prefix;
    std::memcpy(output, &registered, bytes);
    output->struct_size = static_cast<uint32_t>(bytes);
    return NEXORA_GAMEPLAY_OK;
  }
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
