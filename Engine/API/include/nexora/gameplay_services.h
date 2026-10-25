#ifndef NEXORA_GAMEPLAY_SERVICES_H
#define NEXORA_GAMEPLAY_SERVICES_H
#include "gameplay_assets.h"
#include "gameplay_audio.h"
#include "gameplay_audio_state.h"
#include "gameplay_content.h"
#include "gameplay_display.h"
#include "gameplay_input.h"
#include "gameplay_lifecycle.h"
#include "gameplay_scene.h"
#include "gameplay_storage.h"
#include "gameplay_ui.h"
#include "gameplay_ui_draw.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NEXORA_GAMEPLAY_SERVICES_ABI_VERSION 1u
/* Each optional table negotiates independently. Tables and their context borrow the host's
 * serialized owner-thread lifetime, including module Stop/Destroy. No call consumes input.
 * Absent services return UNSUPPORTED without changing the caller's table. */
typedef struct NexoraGameplayServicesApiV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  void *context;
  int32_t (*get_input_api)(void *context, NexoraGameplayInputApiV1 *api);
  int32_t (*get_ui_api)(void *context, NexoraGameplayUiApiV1 *api);
  int32_t (*get_audio_api)(void *context, NexoraGameplayAudioApiV1 *api);
  int32_t (*get_storage_api)(void *context, NexoraStorageApiV1 *api);
  int32_t (*get_content_api)(void *context, NexoraGameplayContentApiV1 *api);
  /* Optional suffix; check struct_size before reading. */
  int32_t (*get_display_api)(void *context, NexoraGameplayDisplayApiV1 *api);
  int32_t (*get_ui_draw_api)(void *context, NexoraGameplayUiDrawApiV1 *api);
  int32_t (*get_assets_api)(void *context, NexoraGameplayAssetsApiV1 *api);
  int32_t (*get_audio_state_api)(void *context, NexoraGameplayAudioStateApiV1 *api);
  int32_t (*get_lifecycle_api)(void *context, NexoraGameplayLifecycleApiV1 *api);
  int32_t (*get_scene_api)(void *context, NexoraGameplaySceneApiV1 *api);
} NexoraGameplayServicesApiV1;
#define NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE offsetof(NexoraGameplayServicesApiV1, get_display_api)
#ifdef __cplusplus
}
#endif
#endif
