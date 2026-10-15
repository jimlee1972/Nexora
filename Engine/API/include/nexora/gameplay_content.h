#ifndef NEXORA_GAMEPLAY_CONTENT_H
#define NEXORA_GAMEPLAY_CONTENT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum NexoraContentKind {
  NEXORA_CONTENT_ATLAS = 1,
  NEXORA_CONTENT_CLIP = 2
} NexoraContentKind;
typedef struct NexoraContentAsset {
  uint64_t high, low;
  uint32_t kind, count;           /* atlas regions or clip frames */
  uint32_t width, height;         /* zero for clips */
  uint64_t atlas_high, atlas_low; /* dependency for clips; own identity for atlases */
} NexoraContentAsset;
typedef struct NexoraGameplayContentApiV1 {
  uint32_t struct_size, abi_version;
  void *context;
  int32_t (*register_atlas)(void *, const void *bytes, uint64_t length, uint64_t *high,
                            uint64_t *low);
  int32_t (*register_clip)(void *, const void *bytes, uint64_t length, uint64_t *high,
                           uint64_t *low);
  /* Null assets/capacity zero probes. Short buffers only publish required count. Sorted
   * kind/high/low. */
  int32_t (*list_assets)(void *, NexoraContentAsset *assets, uint32_t capacity, uint32_t *count);
  int32_t (*describe_asset)(void *, uint64_t high, uint64_t low, NexoraContentAsset *asset);
} NexoraGameplayContentApiV1;
#ifdef __cplusplus
}
#endif
#endif
