#ifndef NEXORA_GAMEPLAY_TWOD_EFFECTS_H
#define NEXORA_GAMEPLAY_TWOD_EFFECTS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NEXORA_TWOD_EFFECTS_MAX_LIGHTS 32u
#define NEXORA_TWOD_EFFECTS_MAX_SHADOW_LIGHTS 8u
#define NEXORA_TWOD_EFFECTS_MAX_OCCLUDERS 256u
#define NEXORA_TWOD_EFFECTS_MAX_VERTICES 16u
#define NEXORA_TWOD_EFFECTS_MAX_EDGES 2048u
#define NEXORA_TWOD_EFFECTS_MAX_SPRITES 100000u
#define NEXORA_TWOD_EFFECTS_SHADOW_SAMPLES 1024u
#define NEXORA_TWOD_EFFECTS_LIGHTING 1u
#define NEXORA_TWOD_EFFECTS_SHADOWS 2u
#define NEXORA_TWOD_EFFECTS_BLOOM 4u
#define NEXORA_TWOD_EFFECTS_ENTITY 0u
#define NEXORA_TWOD_EFFECTS_POOL 1u

typedef struct NexoraTwoDEffectsPointV1 {
  float x, y;
} NexoraTwoDEffectsPointV1;
typedef struct NexoraTwoDEffectsLightV1 {
  uint64_t id;
  NexoraTwoDEffectsPointV1 position;
  float color[3];
  float intensity, radius, softness, bias;
  uint32_t groups, shadow;
} NexoraTwoDEffectsLightV1;
typedef struct NexoraTwoDEffectsOccluderV1 {
  uint64_t id;
  uint32_t groups, vertex_count;
  NexoraTwoDEffectsPointV1 vertices[NEXORA_TWOD_EFFECTS_MAX_VERTICES];
} NexoraTwoDEffectsOccluderV1;
typedef struct NexoraTwoDEffectsSpriteV1 {
  uint64_t handle;
  uint32_t kind, groups, receive_light, receive_shadow;
  uint64_t emission_atlas_high, emission_atlas_low;
  uint32_t emission_region, reserved;
  float emission_color[3], emission_intensity;
} NexoraTwoDEffectsSpriteV1;
typedef struct NexoraTwoDEffectsBloomV1 {
  float intensity, radius;
  uint32_t levels, reserved;
} NexoraTwoDEffectsBloomV1;
/* All arrays are borrowed for submit only; host copies them before returning success. */
typedef struct NexoraTwoDEffectsFrameV1 {
  uint32_t struct_size, abi_version, enabled, light_count, occluder_count, sprite_count;
  float ambient[3], strength;
  NexoraTwoDEffectsBloomV1 bloom;
  const NexoraTwoDEffectsLightV1 *lights;
  const NexoraTwoDEffectsOccluderV1 *occluders;
  const NexoraTwoDEffectsSpriteV1 *sprites;
} NexoraTwoDEffectsFrameV1;
typedef struct NexoraTwoDEffectsCapabilitiesV1 {
  uint32_t struct_size, abi_version, features, max_lights, max_shadow_lights;
  uint32_t max_occluders, max_vertices, max_edges, max_sprites, shadow_samples;
} NexoraTwoDEffectsCapabilitiesV1;
typedef struct NexoraTwoDEffectsStateV1 {
  uint32_t struct_size, abi_version;
  uint64_t accepted_revision, active_revision;
  uint32_t enabled, light_count, occluder_count, sprite_count;
} NexoraTwoDEffectsStateV1;
typedef struct NexoraGameplayTwoDEffectsApiV1 {
  uint32_t struct_size, abi_version;
  void *context;
  int32_t (*get_capabilities)(void *, NexoraTwoDEffectsCapabilitiesV1 *);
  int32_t (*get_state)(void *, NexoraTwoDEffectsStateV1 *);
  int32_t (*submit_frame)(void *, const NexoraTwoDEffectsFrameV1 *);
  /* Optional tail: resolve a complete existing pool handle to an opaque effects identity.
     Tokens belong to this service; never pack/truncate or persist pool handles into handle. */
  int32_t (*pool_identity)(void *, uint64_t owner, uint32_t index, uint32_t generation,
                           uint64_t *identity);
} NexoraGameplayTwoDEffectsApiV1;
#ifdef __cplusplus
}
#endif
#endif
