#ifndef NEXORA_GAMEPLAY_AUDIO_STATE_H
#define NEXORA_GAMEPLAY_AUDIO_STATE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Optional Audio V1 residency and independent Voice-bus controls. Owner thread only.
 * Bytes count decoded float PCM, not process RSS or queued device copies.
 * Return 0 success, -1 invalid, -2 unsupported size, -3 wrong owner thread.
 * Failure preserves output. Tables borrow the audio owner's lifetime. */
typedef struct NexoraAudioUsageV1 {
  uint32_t struct_size;
  uint32_t active_assets;
  uint32_t active_voices;
  uint32_t retired_generations;
  uint64_t active_bytes;
  uint64_t retired_bytes;
  uint64_t peak_live_bytes;
  uint64_t active_limit;
  uint64_t live_limit;
  uint32_t asset_limit;
  uint32_t reserved;
} NexoraAudioUsageV1;
typedef struct NexoraGameplayAudioStateApiV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  void *context;
  int32_t (*get_usage)(void *context, NexoraAudioUsageV1 *usage);
  /* One-shot Voice bus playback. Uses normal Audio V1 voice handles for stop/pause/gain.
   * Audio V1 music/SFX bus settings do not scale this bus; master still scales all buses.
   * Returns 0, -1 invalid, -2 unavailable output, -3 missing/capacity. Outputs unchanged on
   * failure. */
  int32_t (*play_voice)(void *context, uint64_t asset, float gain, uint64_t *voice);
  int32_t (*set_voice_bus_gain)(void *context, float gain);
} NexoraGameplayAudioStateApiV1;
#ifdef __cplusplus
}
#endif
#endif
