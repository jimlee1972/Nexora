#ifndef NEXORA_NEXORA_H
#define NEXORA_NEXORA_H

/*
 * Canonical Nexora language ABI.
 *
 * This header is valid C11 and C++. The companion abi_manifest.json records
 * the since/ownership/nullability/threading/error/determinism contract for
 * every function-pointer export. Append fields to versioned structures; do
 * not reorder, remove, or change the meaning of existing fields.
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEXORA_GAMEPLAY_ABI_VERSION 3u

typedef enum NexoraGameplayResult {
  NEXORA_GAMEPLAY_OK = 0,
  NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT = -1,
  NEXORA_GAMEPLAY_ERROR_UNSUPPORTED = -2,
  NEXORA_GAMEPLAY_ERROR_LIFECYCLE = -3
} NexoraGameplayResult;

typedef enum NexoraGameplayCapability {
  NEXORA_GAMEPLAY_CAPABILITY_NONE = 0,
  NEXORA_GAMEPLAY_CAPABILITY_STATE_MIGRATION = 1u << 0,
  NEXORA_GAMEPLAY_CAPABILITY_FIXED_UPDATE = 1u << 1,
  NEXORA_GAMEPLAY_CAPABILITY_HOST_ALLOCATOR = 1u << 2
} NexoraGameplayCapability;

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

typedef struct NexoraGameplayHostV3 {
  uint32_t struct_size;
  uint32_t abi_version;
  uint64_t capabilities;
  void *context;
  void (*log)(void *context, uint32_t level, const char *message, uint32_t message_length);
  int32_t (*read_component)(void *context, uint64_t entity, uint64_t component_type, void *data,
                            uint32_t data_size);
  int32_t (*write_component)(void *context, uint64_t entity, uint64_t component_type,
                             const void *data, uint32_t data_size);
  void *(*allocate)(void *context, uint64_t size, uint64_t alignment);
  void (*deallocate)(void *context, void *allocation, uint64_t size, uint64_t alignment);
} NexoraGameplayHostV3;

typedef struct NexoraGameModuleV3 {
  uint32_t struct_size;
  uint32_t abi_version;
  uint64_t capabilities;
  void *module_state;
  int32_t (*create)(void **module_state, const NexoraGameplayHostV3 *host);
  int32_t (*on_start)(void *module_state);
  int32_t (*fixed_update)(void *module_state, double fixed_delta_seconds);
  int32_t (*update)(void *module_state, double delta_seconds);
  void (*on_stop)(void *module_state);
  void (*destroy)(void *module_state);
  uint32_t (*save_state)(void *module_state, void *data, uint32_t data_size);
  int32_t (*load_state)(void *module_state, const void *data, uint32_t data_size);
} NexoraGameModuleV3;

typedef int32_t (*NexoraGameModuleLoadV3Fn)(uint32_t requested_abi, NexoraGameModuleV3 *module);

#ifdef __cplusplus
}
#endif

#endif
