#include "nexora/nexora.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>

extern "C" uint64_t NexoraPointerLayout(uint32_t);
extern "C" int32_t NexoraPointerProbe(const NexoraGameplayHostV3 *, uint32_t,
                                      NexoraPointerInputSnapshotV1 *);
namespace {
int calls{};
int32_t Capture(void *, uint32_t user, NexoraPointerInputSnapshotV1 *output) {
  ++calls;
  if (!output || user > 3)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  if (output->struct_size < sizeof(*output))
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  *output = {sizeof(*output), 1, 73, 2, -3, -120, 240, 17, 0};
  return NEXORA_GAMEPLAY_OK;
}
} // namespace
int main() {
  const std::array<std::size_t, 16> layout{sizeof(NexoraPointerInputSnapshotV1),
                                           alignof(NexoraPointerInputSnapshotV1),
                                           offsetof(NexoraPointerInputSnapshotV1, struct_size),
                                           offsetof(NexoraPointerInputSnapshotV1, abi_version),
                                           offsetof(NexoraPointerInputSnapshotV1, sequence),
                                           offsetof(NexoraPointerInputSnapshotV1, delta_x),
                                           offsetof(NexoraPointerInputSnapshotV1, delta_y),
                                           offsetof(NexoraPointerInputSnapshotV1, wheel_x),
                                           offsetof(NexoraPointerInputSnapshotV1, wheel_y),
                                           offsetof(NexoraPointerInputSnapshotV1, buttons),
                                           offsetof(NexoraPointerInputSnapshotV1, reserved),
                                           sizeof(NexoraGameplayHostV3),
                                           offsetof(NexoraGameplayHostV3, capture_pointer_input),
                                           NEXORA_GAMEPLAY_HOST_V3_BASE_SIZE,
                                           NEXORA_GAMEPLAY_HOST_V3_2D_SIZE,
                                           NEXORA_GAMEPLAY_HOST_V3_POINTER_SIZE};
  for (uint32_t index = 0; index < layout.size(); ++index)
    assert(NexoraPointerLayout(index) == layout[index]);
  assert(NexoraPointerLayout(static_cast<uint32_t>(layout.size())) == UINT64_MAX);
  NexoraGameplayHostV3 host{};
  host.struct_size = sizeof(host);
  host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  host.capabilities = NEXORA_GAMEPLAY_CAPABILITY_POINTER_INPUT;
  host.capture_pointer_input = Capture;
  const NexoraPointerInputSnapshotV1 sentinel{
      sizeof(NexoraPointerInputSnapshotV1), 9, 19, 1, 2, 3, 4, 7, 123};
  const auto reject_prefix = [&]<std::size_t Size>() {
    alignas(NexoraGameplayHostV3) std::array<std::byte, Size> bytes{};
    host.struct_size = Size;
    std::memcpy(bytes.data(), &host, Size);
    const auto *old = reinterpret_cast<const NexoraGameplayHostV3 *>(bytes.data());
    auto output = sentinel;
    assert(NexoraCapturePointerInputV1(old, 0, &output) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED);
    assert(NexoraPointerProbe(old, 0, &output) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED);
    assert(std::memcmp(&output, &sentinel, sizeof(output)) == 0 && calls == 0);
  };
  reject_prefix.template operator()<NEXORA_GAMEPLAY_HOST_V3_BASE_SIZE>();
  reject_prefix.template operator()<NEXORA_GAMEPLAY_HOST_V3_2D_SIZE>();
  host.struct_size = sizeof(host);
  for (int variant = 0; variant < 3; ++variant) {
    auto missing = host;
    if (variant == 0)
      missing.capabilities = 0;
    if (variant == 1)
      missing.capture_pointer_input = nullptr;
    if (variant == 2)
      missing.abi_version = 99;
    auto output = sentinel;
    assert(NexoraPointerProbe(&missing, 0, &output) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED);
    assert(NexoraCapturePointerInputV1(&missing, 0, &output) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED);
    assert(std::memcmp(&output, &sentinel, sizeof(output)) == 0 && calls == 0);
  }
  auto cpp = sentinel, zig = sentinel;
  assert(NexoraCapturePointerInputV1(&host, 3, &cpp) == NEXORA_GAMEPLAY_OK);
  assert(NexoraPointerProbe(&host, 3, &zig) == NEXORA_GAMEPLAY_OK);
  assert(std::memcmp(&cpp, &zig, sizeof(cpp)) == 0 && cpp.sequence == 73 && cpp.reserved == 0);
  struct Future {
    NexoraPointerInputSnapshotV1 value;
    uint64_t suffix;
  } future{sentinel, 0xfedcba9876543210ULL};
  future.value.struct_size = sizeof(future);
  assert(NexoraPointerProbe(&host, 0, &future.value) == NEXORA_GAMEPLAY_OK);
  assert(future.suffix == 0xfedcba9876543210ULL);
  for (const uint32_t size : {0U, 4U, static_cast<uint32_t>(sizeof(cpp) - 1)}) {
    auto output = sentinel;
    output.struct_size = size;
    const auto before = output;
    assert(NexoraPointerProbe(&host, 0, &output) == NEXORA_GAMEPLAY_ERROR_UNSUPPORTED);
    assert(std::memcmp(&before, &output, sizeof(output)) == 0);
  }
}
