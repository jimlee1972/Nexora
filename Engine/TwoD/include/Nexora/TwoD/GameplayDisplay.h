#pragma once
#include "Nexora/TwoD/Api.h"
#include "nexora/nexora.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <thread>

namespace nexora::twod {
// Host owns the service and request callback through module unload. No platform dependency.
class GameplayDisplay final {
public:
  using Request = std::function<int32_t(const NexoraDisplayWindowV1 &)>;
  explicit GameplayDisplay(Request request = {}) : request_(std::move(request)) {}
  void BeginFrame(uint32_t width, uint32_t height, float dpi, uint32_t mode) {
    if (std::this_thread::get_id() != owner_)
      throw std::logic_error("display requires owner thread");
    if (width > 16384 || height > 16384 || !std::isfinite(dpi) || dpi <= 0 || mode > 1)
      throw std::invalid_argument("invalid display snapshot");
    NexoraDisplayStateV1 next{};
    next.struct_size = sizeof(next);
    next.abi_version = 1;
    next.pixel_width = width;
    next.pixel_height = height;
    next.logical_width = float(width) / dpi;
    next.logical_height = float(height) / dpi;
    next.dpi_scale = dpi;
    next.mode = mode;
    uint32_t w = width, h = height;
    if (w && h && minimum_ > 0) {
      const double aspect = double(w) / h;
      if (aspect < minimum_)
        h = std::min(h, std::max(1U, uint32_t(std::floor(double(w) / minimum_ + 1e-3))));
      else if (aspect > maximum_)
        w = std::min(w, std::max(1U, uint32_t(std::floor(double(h) * maximum_ + 1e-3))));
    }
    next.viewport_pixels = {float((width - w) / 2), float((height - h) / 2), float(w), float(h)};
    const auto &v = next.viewport_pixels;
    next.viewport_logical = {v.x / dpi, v.y / dpi, v.width / dpi, v.height / dpi};
    // revision changes only when a published observable changes.
    next.revision = state_.revision;
    if (std::memcmp(&next, &state_, sizeof(next)) != 0)
      ++next.revision;
    state_ = next;
  }
  [[nodiscard]] const NexoraDisplayStateV1 &State() const noexcept { return state_; }
  [[nodiscard]] NexoraGameplayDisplayApiV1 Api() noexcept {
    return {sizeof(NexoraGameplayDisplayApiV1),
            1,
            this,
            [](void *p, NexoraDisplayStateV1 *out) {
              return Call(p, [&](auto &s) -> int32_t {
                if (!out)
                  return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
                if (out->struct_size < sizeof(*out) || out->abi_version != 1)
                  return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
                std::memcpy(out, &s.state_, sizeof(*out));
                return NEXORA_GAMEPLAY_OK;
              });
            },
            [](void *p, const NexoraDisplayWindowV1 *v) {
              return Call(p, [&](auto &s) -> int32_t {
                if (!v)
                  return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
                if (v->struct_size < sizeof(*v) || v->abi_version != 1)
                  return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
                if (!v->width || !v->height || v->width > 16384 || v->height > 16384 ||
                    v->mode > 1 || v->resizable > 1)
                  return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
                return s.request_ ? s.request_(*v) : NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
              });
            },
            [](void *p, float minimum, float maximum) {
              return Call(p, [&](auto &s) -> int32_t {
                if (!std::isfinite(minimum) || !std::isfinite(maximum) ||
                    !((minimum == 0 && maximum == 0) || (minimum > 0 && maximum >= minimum)))
                  return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
                s.minimum_ = minimum;
                s.maximum_ = maximum;
                return NEXORA_GAMEPLAY_OK;
              });
            }};
  }

private:
  template <class F> static int32_t Call(void *p, F &&f) noexcept {
    if (!p)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    auto &s = *static_cast<GameplayDisplay *>(p);
    if (std::this_thread::get_id() != s.owner_)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    try {
      return f(s);
    } catch (...) {
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    }
  }
  const std::thread::id owner_{std::this_thread::get_id()};
  Request request_;
  float minimum_{}, maximum_{};
  NexoraDisplayStateV1 state_{};
};
} // namespace nexora::twod
