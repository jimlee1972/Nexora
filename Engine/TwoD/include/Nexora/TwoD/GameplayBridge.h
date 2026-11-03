#pragma once
#include "Nexora/TwoD/Lifecycle.h"
#include "Nexora/TwoD/Tilemap.h"
#include "nexora/nexora.h"
#include <memory>

namespace nexora::twod {
class SceneSession;
using ClipResolver = std::function<std::shared_ptr<const Clip>(AssetId)>;
// Borrowed scene/World/Tilemap must outlive this owner and every module using its API.
// API callbacks serialize on the construction thread and never expose C++ storage.
class NEXORA_2D_API GameplayBridge final {
public:
  GameplayBridge(runtime::World &, SpriteScene &, ClipResolver = {}, Tilemap * = nullptr,
                 InstancePool * = nullptr);
  // Catalog mode routes entity access/queries across all live instances; Tilemap operations
  // address the primary scene. Session and World must outlive this stable API context.
  GameplayBridge(runtime::World &, SceneSession &, ClipResolver = {}, InstancePool * = nullptr);
  ~GameplayBridge();
  GameplayBridge(const GameplayBridge &) = delete;
  GameplayBridge &operator=(const GameplayBridge &) = delete;
  // Caller sets output->struct_size. Failure leaves output unchanged.
  [[nodiscard]] int32_t GetApi(NexoraTwoDApiV1 *output) noexcept;
  // Rendering remains host-owned. Width/height are supplied by the current surface.
  [[nodiscard]] Camera RenderCamera(std::uint32_t width, std::uint32_t height) const;
  // Call for each WorldCommandBuffer::LastDestroyed(), including descendants, before teardown.
  void ForgetEntity(runtime::Id);
  // Owner-thread unpublished branch over identical cloned World/scene/pool/tile state.
  // Copies camera, animation clocks and query snapshots, plus the host-owned clip resolver.
  // All borrowed owners/resolver captures outlive this branch; allocation may throw.
  // These single-scene operations reject catalog-backed bridges without changing either owner.
  [[nodiscard]] std::unique_ptr<GameplayBridge> ForkForReload(runtime::World &, SpriteScene &,
                                                              Tilemap * = nullptr,
                                                              InstancePool * = nullptr) const;
  // Retarget only after the embedding moves identical state at commit/retirement. Keeps the
  // API context address and animation clocks; fails without writes on invalid bindings/reentry.
  [[nodiscard]] bool RebindForReload(runtime::World &, SpriteScene &, Tilemap * = nullptr,
                                     InstancePool * = nullptr) noexcept;

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace nexora::twod
