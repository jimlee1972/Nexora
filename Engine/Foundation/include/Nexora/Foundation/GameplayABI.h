#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEXORA_GAMEPLAY_ABI_VERSION 2u

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

typedef struct NexoraGameplayHostV2 {
  uint32_t struct_size;
  uint32_t abi_version;
  void *context;
  void (*log)(void *context, uint32_t level, const char *message, uint32_t message_length);
  int32_t (*read_component)(void *context, uint64_t entity, uint64_t component_type, void *data,
                            uint32_t data_size);
  int32_t (*write_component)(void *context, uint64_t entity, uint64_t component_type,
                             const void *data, uint32_t data_size);
  int32_t (*subscribe_event)(void *context, uint64_t event_type);
  void (*set_tick_enabled)(void *context, uint32_t enabled);
} NexoraGameplayHostV2;

typedef struct NexoraGameModuleV2 {
  uint32_t struct_size;
  uint32_t abi_version;
  void *module_state;
  int32_t (*initialize)(void **module_state, const NexoraGameplayHostV2 *host);
  void (*update)(void *module_state, double delta_seconds);
  void (*shutdown)(void *module_state);
  uint32_t (*save_state)(void *module_state, void *data, uint32_t data_size);
  int32_t (*load_state)(void *module_state, const void *data, uint32_t data_size);
} NexoraGameModuleV2;

typedef int32_t (*NexoraGameModuleLoadFn)(uint32_t requested_abi, NexoraGameModuleV2 *module);

#ifdef __cplusplus
}
#endif
