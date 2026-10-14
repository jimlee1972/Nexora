#pragma once
#include "Nexora/TwoD/Lifecycle.h"
#include "Nexora/TwoD/Tilemap.h"
#include "nexora/nexora.h"
#include <memory>

namespace nexora::twod {
using ClipResolver = std::function<std::shared_ptr<const Clip>(AssetId)>;
// Borrowed scene/World/Tilemap must outlive this owner and every module using its API.
// API callbacks serialize on the construction thread and never expose C++ storage.
class NEXORA_2D_API GameplayBridge final {
public:
  GameplayBridge(runtime::World &, SpriteScene &, ClipResolver = {}, Tilemap * = nullptr,
                 InstancePool * = nullptr);
  ~GameplayBridge();
  GameplayBridge(const GameplayBridge &) = delete;
  GameplayBridge &operator=(const GameplayBridge &) = delete;
  // Caller sets output->struct_size. Failure leaves output unchanged.
  [[nodiscard]] int32_t GetApi(NexoraTwoDApiV1 *output) noexcept;
  // Rendering remains host-owned. Width/height are supplied by the current surface.
  [[nodiscard]] Camera RenderCamera(std::uint32_t width, std::uint32_t height) const;
  // Call for each WorldCommandBuffer::LastDestroyed(), including descendants, before teardown.
  void ForgetEntity(runtime::Id);

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace nexora::twod
