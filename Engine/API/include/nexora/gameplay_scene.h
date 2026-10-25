#ifndef NEXORA_GAMEPLAY_SCENE_H
#define NEXORA_GAMEPLAY_SCENE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum NexoraSceneLoadMode { NEXORA_SCENE_REPLACE = 0, NEXORA_SCENE_ADDITIVE = 1 };
enum NexoraSceneState {
  NEXORA_SCENE_QUEUED = 1,
  NEXORA_SCENE_APPLIED = 2,
  NEXORA_SCENE_FAILED = 3,
  NEXORA_SCENE_CANCELLED = 4
};
enum NexoraSceneResult {
  NEXORA_SCENE_OK = 0,
  NEXORA_SCENE_INVALID_ARGUMENT = -1,
  NEXORA_SCENE_UNSUPPORTED = -2,
  NEXORA_SCENE_LIFECYCLE = -3,
  NEXORA_SCENE_BUSY = -4,
  NEXORA_SCENE_NOT_FOUND = -5,
  NEXORA_SCENE_CAPACITY = -6
};
#define NEXORA_SCENE_PERSISTENT 1u
#define NEXORA_SCENE_PRIMARY 2u
/* Optional scene catalog/session service. Owner thread, borrowed host lifetime through module
 * Stop/Destroy. Requests copy their arguments and execute AFTER callbacks at a host frame boundary.
 * Legacy V3 load_scene still creates a logical scene; this table loads packaged authored scenes.
 * Returns 0 success, -1 invalid argument, -2 unsupported, -3 lifecycle/stopped/prepare failure,
 * -4 busy, -5 missing/stale scene or request, -6 capacity/ID exhaustion. Failed calls preserve
 * outputs. mode: Replace=0, Additive=1. flags: persistent=1, no other bits. Additive duplicate
 * assets reject. Scene/entity handles use never-reused World IDs within the borrowed session/World
 * lifetime. */
typedef struct NexoraSceneTransitionV1 {
  uint32_t struct_size;
  uint32_t state; /* Queued=1, Applied=2, Failed=3, Cancelled=4. */
  uint64_t request;
  uint64_t scene;
  int32_t result;
  uint32_t reserved;
} NexoraSceneTransitionV1;
typedef struct NexoraSceneInstanceV1 {
  uint32_t struct_size;
  uint32_t flags; /* persistent=1, primary=2 */
  uint64_t scene;
  uint64_t asset_high;
  uint64_t asset_low;
  uint64_t camera; /* 0 means host default camera; only primary owns the active camera. */
} NexoraSceneInstanceV1;
typedef struct NexoraGameplaySceneApiV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  void *context;
  int32_t (*resolve_scene)(void *context, const char *path, uint32_t length, uint64_t *asset_high,
                           uint64_t *asset_low);
  int32_t (*request_load)(void *context, uint64_t asset_high, uint64_t asset_low, uint32_t mode,
                          uint32_t flags, uint64_t *request);
  int32_t (*request_unload)(void *context, uint64_t scene, uint64_t *request);
  int32_t (*get_status)(void *context, uint64_t request, NexoraSceneTransitionV1 *status);
  int32_t (*get_active_scene)(void *context, uint64_t *scene);
  int32_t (*get_scene_info)(void *context, uint64_t scene, NexoraSceneInstanceV1 *info);
} NexoraGameplaySceneApiV1;
#ifdef __cplusplus
}
#endif
#endif
