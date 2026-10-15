#ifndef NEXORA_GAMEPLAY_STORAGE_H
#define NEXORA_GAMEPLAY_STORAGE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NEXORA_STORAGE_ABI_VERSION 1u
/* Synchronous owner-thread calls. Context and table borrow the host service lifetime.
 * Keys are 1..64 ASCII letters/digits/_/-. Payload bytes and schema belong to Zig.
 * All errors preserve output buffers/structures. load(NULL,0,info) validates and probes.
 * Save atomically replaces a slot; orphan .tmp files are never promoted to saves.
 * Storage is independent of gameplay module save_state/load_state hot reload. */
typedef enum NexoraStorageResult {
  NEXORA_STORAGE_OK = 0,
  NEXORA_STORAGE_INVALID_ARGUMENT = -10,
  NEXORA_STORAGE_NOT_FOUND = -11,
  NEXORA_STORAGE_BUFFER_TOO_SMALL = -12,
  NEXORA_STORAGE_CORRUPT = -13,
  NEXORA_STORAGE_IO_ERROR = -14,
  NEXORA_STORAGE_LIMIT_EXCEEDED = -15,
  NEXORA_STORAGE_WRONG_THREAD = -16
} NexoraStorageResult;
typedef struct NexoraStorageInfoV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  uint32_t schema_version;
  uint32_t payload_size;
} NexoraStorageInfoV1;
typedef struct NexoraStorageApiV1 {
  uint32_t struct_size;
  uint32_t abi_version;
  void *context;
  int32_t (*save)(void *context, const char *key, uint32_t key_length, uint32_t schema_version,
                  const void *data, uint32_t data_size);
  int32_t (*load)(void *context, const char *key, uint32_t key_length, void *data,
                  uint32_t capacity, NexoraStorageInfoV1 *out_info);
  int32_t (*remove)(void *context, const char *key, uint32_t key_length);
  int32_t (*exists)(void *context, const char *key, uint32_t key_length, uint32_t *out_exists);
} NexoraStorageApiV1;
#ifdef __cplusplus
}
#endif
#endif
