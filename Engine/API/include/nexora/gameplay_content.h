#ifndef NEXORA_GAMEPLAY_CONTENT_H
#define NEXORA_GAMEPLAY_CONTENT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum NexoraContentKind {
  NEXORA_CONTENT_ATLAS = 1,
  NEXORA_CONTENT_CLIP = 2,
  /* Content registry kinds are independent of asset catalog kinds (catalog 3 is opaque). */
  NEXORA_CONTENT_SPRITE_SOURCE = 3
} NexoraContentKind;
typedef struct NexoraContentAsset {
  uint64_t high, low;
  uint32_t kind, count;           /* atlas regions, clip frames, or one Sprite source */
  uint32_t width, height;         /* Atlas dimensions, source canvas, or zero for clips */
  uint64_t atlas_high, atlas_low; /* legacy Clip/source dependency; Atlas identity; source Clip=0 */
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
/* Separately negotiated optional table; the original content table and host ABI are unchanged.
 * register_sprite_source consumes the existing cooked .nx2sprite schema 1 payload. */
typedef struct NexoraGameplaySpriteContentApiV1 {
  uint32_t struct_size, abi_version;
  void *context;
  int32_t (*register_sprite_source)(void *, const void *bytes, uint64_t length, uint64_t *high,
                                    uint64_t *low);
} NexoraGameplaySpriteContentApiV1;
#ifdef __cplusplus
}
#endif
#endif
