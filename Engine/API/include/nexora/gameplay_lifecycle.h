#ifndef NEXORA_GAMEPLAY_LIFECYCLE_H
#define NEXORA_GAMEPLAY_LIFECYCLE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Owner thread only, borrowed host lifetime. Copies never consume transitions.
 * 0 success, -1 invalid argument/token, -2 unsupported, -3 lifecycle/native failure.
 * Failed queries preserve outputs. Flags: focused=1, minimized=2, suspended=4.
 * Sequences retain interruptions even when lost/restored in one event batch. */
typedef struct NexoraLifecycleStateV1 {
  uint32_t struct_size;
  uint32_t flags;
  uint64_t sequence;
  uint64_t interruption_sequence;
  uint64_t suspend_sequence;
  uint64_t resume_sequence;
  uint64_t close_token;
  uint64_t disconnect_sequence[4];
  uint32_t connected_mask;
  uint32_t rumble_mask;
} NexoraLifecycleStateV1;
typedef struct NexoraGameplayLifecycleApiV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  void *context;
  int32_t (*get_state)(void *context, NexoraLifecycleStateV1 *state);
  /* Explicit opt-in. Legacy modules still close immediately. Cannot disable with pending close. */
  int32_t (*set_close_guard)(void *context, uint32_t enabled);
  /* Exact pending token; accept=0 cancels, accept=1 authorizes exit (does NOT save). */
  int32_t (*resolve_close)(void *context, uint64_t token, uint32_t accept);
  /* Motors [0,1], duration 1..5000ms; both zero stops (duration may be 0).
   * Unsupported devices return -2. No queued replay after blur/reconnect/resume.
   * Nonzero output is rejected while unfocused/minimized/suspended. */
  int32_t (*rumble)(void *context, uint32_t device, float low, float high, uint32_t duration_ms);
} NexoraGameplayLifecycleApiV1;
#ifdef __cplusplus
}
#endif
#endif
