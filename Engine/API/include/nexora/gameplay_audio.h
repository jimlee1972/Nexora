#ifndef NEXORA_GAMEPLAY_AUDIO_H
#define NEXORA_GAMEPLAY_AUDIO_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NEXORA_GAMEPLAY_AUDIO_API_VERSION 1u
/* Owner-thread API. IDs are host-owned, never pointers. Zero is invalid.
 * All functions return 0 on success; -1 invalid arguments; -2 unsupported or
 * unavailable output; -3 missing asset, stale voice, or exhausted voice capacity.
 * Output structures are written only on success. */
typedef enum NexoraAudioBus {
  NEXORA_AUDIO_BUS_MASTER = 0,
  NEXORA_AUDIO_BUS_MUSIC = 1,
  NEXORA_AUDIO_BUS_SFX = 2
} NexoraAudioBus;
typedef struct NexoraAudioPlayDesc {
  uint64_t asset;
  uint32_t bus;
  uint32_t loop;
  float gain;
} NexoraAudioPlayDesc;
typedef struct NexoraGameplayAudioApiV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  void *context;
  int32_t (*play)(void *context, const NexoraAudioPlayDesc *desc, uint64_t *voice);
  int32_t (*stop)(void *context, uint64_t voice);
  int32_t (*set_paused)(void *context, uint64_t voice, uint32_t paused);
  int32_t (*set_gain)(void *context, uint64_t voice, float gain);
  int32_t (*set_bus_gain)(void *context, uint32_t bus, float gain);
  int32_t (*is_playing)(void *context, uint64_t voice, uint32_t *playing);
  int32_t (*output_available)(void *context, uint32_t *available);
  int32_t (*stop_all)(void *context);
  /* Copies/decodes payload during this call; caller retains bytes. Limit 64 MiB.
   * Replacement keeps active voices on their original sample generation.
   * Invalid WAV returns -1, allocation/capacity failure -3. */
  int32_t (*register_wave)(void *context, uint64_t asset, const void *bytes, uint32_t byte_count);
  int32_t (*remove_asset)(void *context, uint64_t asset);
} NexoraGameplayAudioApiV1;
#ifdef __cplusplus
}
#endif
#endif
