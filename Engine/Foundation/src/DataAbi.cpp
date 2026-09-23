#include "Nexora/Foundation/DataAbi.h"

#include "Nexora/Foundation/Types.h"

#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <vector>

struct NexoraFoundationOwnedBuffer final {
  std::vector<std::uint8_t> bytes;
};

namespace {
NexoraFoundationResult CreateBuffer(NexoraFoundationByteView source,
                                    NexoraFoundationOwnedBuffer **out_buffer, bool validate_utf8) {
  if (out_buffer == nullptr)
    return NEXORA_FOUNDATION_ERROR_INVALID_ARGUMENT;
  *out_buffer = nullptr;
  if ((source.data == nullptr && source.size != 0) ||
      source.size > std::numeric_limits<std::size_t>::max())
    return NEXORA_FOUNDATION_ERROR_INVALID_ARGUMENT;
  if (source.size > std::vector<std::uint8_t>{}.max_size())
    return NEXORA_FOUNDATION_ERROR_INVALID_ARGUMENT;
  if (validate_utf8 && source.size != 0 &&
      !nexora::foundation::IsValidUtf8(
          {reinterpret_cast<const char *>(source.data), static_cast<std::size_t>(source.size)}))
    return NEXORA_FOUNDATION_ERROR_INVALID_UTF8;

  try {
    auto buffer = std::make_unique<NexoraFoundationOwnedBuffer>();
    if (source.size != 0)
      buffer->bytes.assign(source.data, source.data + static_cast<std::size_t>(source.size));
    *out_buffer = buffer.release();
    return NEXORA_FOUNDATION_OK;
  } catch (const std::bad_alloc &) {
    return NEXORA_FOUNDATION_ERROR_OUT_OF_MEMORY;
  }
}
} // namespace

NexoraFoundationResult
nexora_foundation_buffer_create(NexoraFoundationByteView bytes,
                                NexoraFoundationOwnedBuffer **out_buffer) noexcept {
  return CreateBuffer(bytes, out_buffer, false);
}

NexoraFoundationResult
nexora_foundation_string_create_utf8(NexoraFoundationByteView text,
                                     NexoraFoundationOwnedBuffer **out_string) noexcept {
  return CreateBuffer(text, out_string, true);
}

NexoraFoundationResult nexora_foundation_buffer_view(const NexoraFoundationOwnedBuffer *buffer,
                                                     NexoraFoundationByteView *out_view) noexcept {
  if (buffer == nullptr || out_view == nullptr)
    return NEXORA_FOUNDATION_ERROR_INVALID_ARGUMENT;
  out_view->data = buffer->bytes.empty() ? nullptr : buffer->bytes.data();
  out_view->size = buffer->bytes.size();
  return NEXORA_FOUNDATION_OK;
}

NexoraFoundationResult nexora_foundation_buffer_copy(const NexoraFoundationOwnedBuffer *buffer,
                                                     std::uint8_t *destination,
                                                     std::uint64_t capacity,
                                                     std::uint64_t *out_required_size) noexcept {
  if (buffer == nullptr || out_required_size == nullptr ||
      (destination == nullptr && capacity != 0))
    return NEXORA_FOUNDATION_ERROR_INVALID_ARGUMENT;
  *out_required_size = buffer->bytes.size();
  if (capacity < buffer->bytes.size())
    return NEXORA_FOUNDATION_ERROR_BUFFER_TOO_SMALL;
  if (!buffer->bytes.empty()) {
    if (destination == nullptr)
      return NEXORA_FOUNDATION_ERROR_INVALID_ARGUMENT;
    std::memcpy(destination, buffer->bytes.data(), buffer->bytes.size());
  }
  return NEXORA_FOUNDATION_OK;
}

void nexora_foundation_buffer_destroy(NexoraFoundationOwnedBuffer *buffer) noexcept {
  delete buffer;
}
