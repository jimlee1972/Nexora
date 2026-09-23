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
  NEXORA_GAMEPLAY_CAPABILITY_HOST_ALLOCATOR = 1u << 2,
  NEXORA_GAMEPLAY_CAPABILITY_SCENE_API = 1u << 3
} NexoraGameplayCapability;

typedef enum NexoraAllocationOwner {
  NEXORA_ALLOCATION_OWNER_UNKNOWN = 0,
  NEXORA_ALLOCATION_OWNER_GAMEPLAY_STATE = 1,
  NEXORA_ALLOCATION_OWNER_STATE_MIGRATION = 2
} NexoraAllocationOwner;

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

typedef struct NexoraVec3 { double x, y, z; } NexoraVec3;
typedef struct NexoraAssetHandle { uint64_t value; } NexoraAssetHandle;
typedef struct NexoraEntitySpawnDescriptor {
  uint32_t struct_size;
  uint32_t components;
  NexoraVec3 position;
  double camera_fov_degrees;
  float light_intensity;
  uint32_t reserved;
  NexoraAssetHandle mesh;
  NexoraAssetHandle material;
  NexoraVec3 bounds_minimum;
  NexoraVec3 bounds_maximum;
} NexoraEntitySpawnDescriptor;
typedef struct NexoraInputSnapshot {
  uint64_t sequence;
  double move_x;
  double move_y;
  uint32_t buttons;
  uint32_t reserved;
} NexoraInputSnapshot;
typedef struct NexoraRaycastRequest {
  NexoraVec3 origin;
  NexoraVec3 direction;
  double distance;
} NexoraRaycastRequest;
typedef struct NexoraRaycastHit {
  uint64_t entity;
  double distance;
  NexoraVec3 point;
} NexoraRaycastHit;
typedef struct NexoraDebugLine {
  NexoraVec3 start;
  NexoraVec3 end;
  uint32_t rgba;
  float duration_seconds;
} NexoraDebugLine;
typedef struct NexoraFrameDiagnostics {
  uint64_t frame;
  uint64_t scene_entities;
  uint64_t debug_lines;
  uint64_t api_errors;
} NexoraFrameDiagnostics;

enum NexoraSpawnComponent {
  NEXORA_SPAWN_CAMERA = 1u << 0,
  NEXORA_SPAWN_LIGHT = 1u << 1,
  NEXORA_SPAWN_MESH = 1u << 2,
  NEXORA_SPAWN_PHYSICS = 1u << 3
};

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
  void *(*allocate)(void *context, uint64_t owner, uint64_t size, uint64_t alignment);
  void (*deallocate)(void *context, uint64_t owner, void *allocation, uint64_t size,
                     uint64_t alignment);
  int32_t (*load_scene)(void *context, const char *name, uint32_t name_length,
                        uint32_t persistent, uint64_t *scene);
  int32_t (*activate_scene)(void *context, uint64_t scene);
  int32_t (*spawn_entity)(void *context, uint64_t scene,
                          const NexoraEntitySpawnDescriptor *descriptor, uint64_t *entity);
  int32_t (*despawn_entity)(void *context, uint64_t entity);
  int32_t (*capture_input)(void *context, uint32_t user, NexoraInputSnapshot *snapshot);
  int32_t (*resolve_asset)(void *context, uint64_t uuid_high, uint64_t uuid_low,
                           NexoraAssetHandle *asset);
  int32_t (*raycast)(void *context, const NexoraRaycastRequest *request, NexoraRaycastHit *hit);
  int32_t (*debug_draw_line)(void *context, const NexoraDebugLine *line);
  int32_t (*get_diagnostics)(void *context, NexoraFrameDiagnostics *diagnostics);
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
