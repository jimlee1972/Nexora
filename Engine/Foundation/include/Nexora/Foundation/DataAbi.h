#pragma once

#include "Nexora/Foundation/Api.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEXORA_FOUNDATION_DATA_ABI_VERSION 1u

typedef int32_t NexoraFoundationResult;
enum {
  NEXORA_FOUNDATION_OK = 0,
  NEXORA_FOUNDATION_ERROR_INVALID_ARGUMENT = 1,
  NEXORA_FOUNDATION_ERROR_INVALID_UTF8 = 2,
  NEXORA_FOUNDATION_ERROR_BUFFER_TOO_SMALL = 3,
  NEXORA_FOUNDATION_ERROR_OUT_OF_MEMORY = 4
};

#ifdef __cplusplus
#define NEXORA_FOUNDATION_NOEXCEPT noexcept
#else
#define NEXORA_FOUNDATION_NOEXCEPT
#endif

typedef struct NexoraFoundationByteView {
  const uint8_t *data;
  uint64_t size;
} NexoraFoundationByteView;

typedef struct NexoraFoundationOwnedBuffer NexoraFoundationOwnedBuffer;

// Copies `bytes` into an engine allocation. A null data pointer is valid only
// when size is zero. The returned handle must be released with destroy.
NEXORA_FOUNDATION_API NexoraFoundationResult nexora_foundation_buffer_create(
    NexoraFoundationByteView bytes,
    NexoraFoundationOwnedBuffer **out_buffer) NEXORA_FOUNDATION_NOEXCEPT;

// As above, but rejects malformed UTF-8. Embedded NUL bytes are preserved.
NEXORA_FOUNDATION_API NexoraFoundationResult nexora_foundation_string_create_utf8(
    NexoraFoundationByteView text,
    NexoraFoundationOwnedBuffer **out_string) NEXORA_FOUNDATION_NOEXCEPT;

// The view remains owned by `buffer` and is invalidated by destroy. It is not
// NUL-terminated and must not be retained beyond the handle's lifetime.
NEXORA_FOUNDATION_API NexoraFoundationResult
nexora_foundation_buffer_view(const NexoraFoundationOwnedBuffer *buffer,
                              NexoraFoundationByteView *out_view) NEXORA_FOUNDATION_NOEXCEPT;

// Caller-owned alternative. Always reports the required byte count. No bytes
// are written and BUFFER_TOO_SMALL is returned when capacity is insufficient.
NEXORA_FOUNDATION_API NexoraFoundationResult nexora_foundation_buffer_copy(
    const NexoraFoundationOwnedBuffer *buffer, uint8_t *destination, uint64_t capacity,
    uint64_t *out_required_size) NEXORA_FOUNDATION_NOEXCEPT;

// Null is accepted. Destruction occurs inside the module that allocated the
// handle, avoiding C++ allocator ownership across a DLL boundary.
NEXORA_FOUNDATION_API void
nexora_foundation_buffer_destroy(NexoraFoundationOwnedBuffer *buffer) NEXORA_FOUNDATION_NOEXCEPT;

#undef NEXORA_FOUNDATION_NOEXCEPT

#ifdef __cplusplus
}
#endif
