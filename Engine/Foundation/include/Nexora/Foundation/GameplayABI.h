#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEXORA_GAMEPLAY_ABI_VERSION 1u

typedef struct NexoraGameplayHostV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  void *context;
  void (*log)(void *context, uint32_t level, const char *message, uint32_t message_length);
} NexoraGameplayHostV1;

typedef struct NexoraGameModuleV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  void *module_state;
  int32_t (*initialize)(void **module_state, const NexoraGameplayHostV1 *host);
  void (*update)(void *module_state, double delta_seconds);
  void (*shutdown)(void *module_state);
} NexoraGameModuleV1;

typedef int32_t (*NexoraGameModuleLoadFn)(uint32_t requested_abi, NexoraGameModuleV1 *module);

#ifdef __cplusplus
}
#endif
