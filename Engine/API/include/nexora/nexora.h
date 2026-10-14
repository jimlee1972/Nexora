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
#include <stddef.h>
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
  NEXORA_GAMEPLAY_CAPABILITY_SCENE_API = 1u << 3,
  NEXORA_GAMEPLAY_CAPABILITY_2D_API = 1u << 4,
  NEXORA_GAMEPLAY_CAPABILITY_POINTER_INPUT = 1u << 5
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

typedef struct NexoraVec3 {
  double x, y, z;
} NexoraVec3;
typedef struct NexoraAssetHandle {
  uint64_t value;
} NexoraAssetHandle;
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

#define NEXORA_POINTER_INPUT_ABI_VERSION 1u
/* Caller sets struct_size. Motion is per native batch in client pixels; wheel is in Window
 * 120-unit steps. Repeated reads never consume it. buttons: left/right/middle/back/forward bits. */
typedef struct NexoraPointerInputSnapshotV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  uint64_t sequence;
  double delta_x;
  double delta_y;
  double wheel_x;
  double wheel_y;
  uint32_t buttons;
  uint32_t reserved;
} NexoraPointerInputSnapshotV1;
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

/*
 * Component wires exchanged through read_component/write_component. The component type is the
 * 64-bit FNV-1a hash of the component name. Transforms are local: relative to the parent, or the
 * world pose for a root. "Nexora.Transform" (three doubles: the local position; a write keeps the
 * rotation and scale) is unchanged.
 */
typedef struct NexoraQuat {
  double x, y, z, w;
} NexoraQuat;
/*
 * "Nexora.TransformV2" (read/write): the full local transform. A write is rejected unless every
 * value is finite, the quaternion has non-zero length (it is normalized), and no scale is zero;
 * negative scale mirrors. "Nexora.WorldTransform" (read only) uses the same layout for the world
 * pose, which is the nearest translation/rotation/scale when a non-uniformly scaled ancestor has a
 * rotated descendant (like Unity's lossyScale).
 */
typedef struct NexoraTransformV2 {
  NexoraVec3 position;
  NexoraQuat rotation;
  NexoraVec3 scale;
} NexoraTransformV2;
/*
 * "Nexora.Parent" (read/write): the parent entity, 0 for a root. A write reparents within the same
 * scene and is rejected for a cycle, a missing parent, or another scene. keep_local == 0 keeps the
 * world pose (Unity's worldPositionStays, the default); non-zero keeps the local values. Reads
 * return keep_local and reserved as 0.
 */
typedef struct NexoraParent {
  uint64_t parent;
  uint32_t keep_local;
  uint32_t reserved;
} NexoraParent;

enum NexoraSpawnComponent {
  NEXORA_SPAWN_CAMERA = 1u << 0,
  NEXORA_SPAWN_LIGHT = 1u << 1,
  NEXORA_SPAWN_MESH = 1u << 2,
  NEXORA_SPAWN_PHYSICS = 1u << 3
};

/* Optional 2D extension. All values are copied; context remains host-owned. */
#define NEXORA_2D_API_VERSION 1u
typedef struct NexoraVec2 {
  float x, y;
} NexoraVec2;
typedef struct NexoraTwoDSprite {
  uint64_t atlas_high, atlas_low;
  uint32_t region, flags; /* flip X=1, flip Y=2, Y-sort=4, visible=8 */
  NexoraVec2 size, pivot;
  float color[4];
  int32_t layer, order;
} NexoraTwoDSprite;
typedef struct NexoraTwoDCollider {
  uint32_t kind; /* circle=0, axis-aligned box=1 */
  NexoraVec2 offset, half_extent;
  float radius;
  uint32_t group, mask;
} NexoraTwoDCollider;
typedef struct NexoraTwoDCamera {
  NexoraVec2 center;
  float vertical_size, rotation_radians; /* viewport stays engine-owned */
} NexoraTwoDCamera;
typedef struct NexoraTwoDShape {
  uint64_t exclude_id;         /* same query namespace; zero excludes nothing */
  NexoraTwoDCollider geometry; /* offset is world center */
} NexoraTwoDShape;
typedef struct NexoraTwoDSweep {
  NexoraVec2 from, to;
  float radius;
  uint32_t group, mask;
} NexoraTwoDSweep;
typedef struct NexoraTwoDHit {
  uint64_t id;
  double fraction;
} NexoraTwoDHit;
typedef struct NexoraTwoDEvent {
  uint32_t length;
  char name[256];
} NexoraTwoDEvent;
typedef struct NexoraTwoDAnimationResult {
  uint32_t region, finished, event_count, reserved;
} NexoraTwoDAnimationResult;
typedef struct NexoraTwoDTile {
  int32_t x, y, layer;
  uint32_t region, collision; /* zero region erases; collision is 0/1 */
} NexoraTwoDTile;
typedef struct NexoraTwoDInstanceHandle {
  uint64_t owner;
  uint32_t index, generation;
} NexoraTwoDInstanceHandle;
typedef struct NexoraTwoDInstance {
  NexoraTransformV2 transform;
  NexoraTwoDSprite sprite;
} NexoraTwoDInstance;
typedef struct NexoraTwoDApiV1 {
  uint32_t struct_size, abi_version;
  void *context;
  int32_t (*set_sprite)(void *context, uint64_t entity,
                        const NexoraTwoDSprite *sprite); /* null removes */
  int32_t (*get_sprite)(void *context, uint64_t entity, NexoraTwoDSprite *sprite);
  int32_t (*set_collider)(void *context, uint64_t entity,
                          const NexoraTwoDCollider *collider); /* null removes */
  int32_t (*get_collider)(void *context, uint64_t entity, NexoraTwoDCollider *collider);
  int32_t (*set_camera)(void *context, const NexoraTwoDCamera *camera);
  int32_t (*get_camera)(void *context, NexoraTwoDCamera *camera);
  int32_t (*play_animation)(void *context, uint64_t entity, uint64_t clip_high, uint64_t clip_low,
                            double speed);
  int32_t (*pause_animation)(void *context, uint64_t entity, uint32_t paused);
  /* Null events/capacity zero probes without advancing. Short buffers publish only result. */
  int32_t (*advance_animation)(void *context, uint64_t entity, double seconds,
                               NexoraTwoDEvent *events, uint32_t capacity,
                               NexoraTwoDAnimationResult *result);
  /* source: scene colliders=0, optional Tilemap=1; IDs never mix namespaces. */
  int32_t (*rebuild_queries)(void *context, uint32_t source);
  int32_t (*overlap)(void *context, uint32_t source, const NexoraTwoDShape *shape, uint64_t *hits,
                     uint32_t capacity, uint32_t *count);
  int32_t (*sweep)(void *context, uint32_t source, const NexoraTwoDSweep *request,
                   NexoraTwoDHit *hits, uint32_t capacity, uint32_t *count);
  int32_t (*paint_tiles)(void *context, const NexoraTwoDTile *tiles, uint32_t count);
  int32_t (*set_chunk_loaded)(void *context, int32_t chunk_x, int32_t chunk_y, uint32_t loaded);
  int32_t (*get_tile)(void *context, int32_t x, int32_t y, int32_t layer, NexoraTwoDTile *tile);
  int32_t (*create_instances)(void *context, const NexoraTwoDInstance *instances, uint32_t count,
                              NexoraTwoDInstanceHandle *handles);
  int32_t (*destroy_instances)(void *context, const NexoraTwoDInstanceHandle *handles,
                               uint32_t count);
  int32_t (*read_instances)(void *context, const NexoraTwoDInstanceHandle *handles, uint32_t count,
                            NexoraTwoDInstance *instances);
  int32_t (*write_instances)(void *context, const NexoraTwoDInstanceHandle *handles, uint32_t count,
                             const NexoraTwoDInstance *instances);
} NexoraTwoDApiV1;

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
  int32_t (*load_scene)(void *context, const char *name, uint32_t name_length, uint32_t persistent,
                        uint64_t *scene);
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
  /* Read only after checking struct_size and CAPABILITY_2D_API. Caller sets api.struct_size. */
  int32_t (*get_twod_api)(void *context, NexoraTwoDApiV1 *api);
  /* Optional suffix: inspect size/capability before reading. Caller sets snapshot.struct_size. */
  int32_t (*capture_pointer_input)(void *context, uint32_t user,
                                   NexoraPointerInputSnapshotV1 *snapshot);
} NexoraGameplayHostV3;

/* The original V3 prefix remains sufficient for modules that do not request extensions. */
#define NEXORA_GAMEPLAY_HOST_V3_BASE_SIZE offsetof(NexoraGameplayHostV3, get_twod_api)
#define NEXORA_GAMEPLAY_HOST_V3_2D_SIZE                                                            \
  (offsetof(NexoraGameplayHostV3, get_twod_api) + sizeof(((NexoraGameplayHostV3 *)0)->get_twod_api))
#define NEXORA_GAMEPLAY_HOST_V3_POINTER_SIZE                                                       \
  (offsetof(NexoraGameplayHostV3, capture_pointer_input) +                                         \
   sizeof(((NexoraGameplayHostV3 *)0)->capture_pointer_input))

/* Safe with an original V3 prefix or an older 2D-only host. No output is touched when absent. */
static inline int32_t NexoraCapturePointerInputV1(const NexoraGameplayHostV3 *host, uint32_t user,
                                                  NexoraPointerInputSnapshotV1 *snapshot) {
  if (!host || host->struct_size < NEXORA_GAMEPLAY_HOST_V3_POINTER_SIZE ||
      host->abi_version != NEXORA_GAMEPLAY_ABI_VERSION ||
      !(host->capabilities & NEXORA_GAMEPLAY_CAPABILITY_POINTER_INPUT) ||
      !host->capture_pointer_input)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  return host->capture_pointer_input(host->context, user, snapshot);
}

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
