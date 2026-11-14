#include "SceneUiComposition.h"
#include <atomic>

namespace Nexora::Presentation::detail {
std::uint64_t NextSceneTextureDomain() noexcept {
  static std::atomic<std::uint64_t> next{1};
  auto value = next.load(std::memory_order_relaxed);
  while (value != UINT64_MAX) {
    if (next.compare_exchange_weak(value, value + 1, std::memory_order_relaxed))
      return value;
  }
  return 0;
}
} // namespace Nexora::Presentation::detail
