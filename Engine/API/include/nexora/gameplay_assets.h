#ifndef NEXORA_GAMEPLAY_ASSETS_H
#define NEXORA_GAMEPLAY_ASSETS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef enum NexoraAssetLoadPhase {
  NEXORA_ASSET_QUEUED = 1,
  NEXORA_ASSET_LOADING = 2,
  NEXORA_ASSET_READY = 3,
  NEXORA_ASSET_FAILED = 4
} NexoraAssetLoadPhase;
/* Catalog kinds 1/2 are cooked Atlas/Clip; 3 is opaque game-owned bytes. */
typedef struct NexoraAssetCatalogEntryV1 {
  uint32_t struct_size, kind;
  uint64_t file_bytes;
} NexoraAssetCatalogEntryV1;
typedef struct NexoraAssetLoadStateV1 {
  uint32_t struct_size, phase;
  int32_t result;
  uint32_t catalog_index;
  uint64_t high, low, byte_length;
} NexoraAssetLoadStateV1;
typedef struct NexoraAssetUsageV1 {
  uint32_t struct_size, active_assets, retired_generations, requests;
  uint64_t active_bytes, retired_bytes, texture_bytes, staging_bytes, peak_content_bytes;
} NexoraAssetUsageV1;
typedef struct NexoraGameplayAssetsApiV1 {
  uint32_t struct_size, abi_version;
  void *context;
  int32_t (*describe_catalog)(void *, uint32_t index, NexoraAssetCatalogEntryV1 *);
  int32_t (*request)(void *, uint32_t index, uint64_t *ticket);
  int32_t (*poll)(void *, uint64_t ticket, NexoraAssetLoadStateV1 *);
  /* Forget/cancel a request. Successfully registered content is NOT implicitly unloaded. */
  int32_t (*release_request)(void *, uint64_t ticket);
  int32_t (*remove_asset)(void *, uint64_t high, uint64_t low);
  int32_t (*get_usage)(void *, NexoraAssetUsageV1 *);
  /* Opaque-byte requests only. Null buffer/zero capacity probes required length. */
  int32_t (*copy_bytes)(void *, uint64_t ticket, void *, uint64_t capacity, uint64_t *length);
} NexoraGameplayAssetsApiV1;
#ifdef __cplusplus
}
#endif
#endif
