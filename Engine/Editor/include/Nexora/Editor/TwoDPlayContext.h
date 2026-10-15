#pragma once

#include "Nexora/Editor/TwoDDocument.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include <thread>

namespace nexora::editor {

// Owner-thread session snapshot. Unload every gameplay module before destroying this owner;
// copied C tables borrow its bridge and are valid only until destruction.
class NEXORA_EDITOR_API TwoDPlayContext final {
public:
  static constexpr std::size_t kMaximumPoolCapacity = 4096;
  static constexpr std::size_t kMaximumFrameInstances = 100000;
  [[nodiscard]] static std::unique_ptr<TwoDPlayContext>
  Create(const TwoDDocument &, std::size_t pool_capacity = kMaximumPoolCapacity) noexcept;
  ~TwoDPlayContext();
  TwoDPlayContext(const TwoDPlayContext &) = delete;
  TwoDPlayContext &operator=(const TwoDPlayContext &) = delete;

  [[nodiscard]] runtime::World &World() noexcept { return world_; }
  [[nodiscard]] const twod::SpriteScene &Scene() const noexcept { return *scene_; }
  [[nodiscard]] const twod::Tilemap &Tiles() const noexcept { return tiles_; }
  [[nodiscard]] std::size_t InstanceCount() const noexcept { return pool_.Size(); }
  [[nodiscard]] int32_t GetApi(NexoraTwoDApiV1 *) noexcept;
  // Call for every destroyed World ID, including descendants, after command publication.
  void ForgetEntity(runtime::Id);
  // One globally sorted owning frame; no time advancement, asset IO or GPU publication.
  // The host supplies a fresh surface-local texture identity for this frozen Atlas generation.
  [[nodiscard]] std::optional<twod::SpriteFrame>
  ExtractFrame(std::uint32_t width, std::uint32_t height, std::uint64_t texture) noexcept;

private:
  explicit TwoDPlayContext(std::size_t pool_capacity);
  std::thread::id thread_;
  runtime::World world_{runtime::WorldKind::Play};
  std::optional<twod::SpriteScene> scene_;
  std::shared_ptr<const twod::Atlas> atlas_;
  std::shared_ptr<const twod::Clip> clip_;
  twod::Tilemap tiles_;
  twod::InstancePool pool_;
  // Last member: destroy callbacks/context before every owner it borrows.
  std::unique_ptr<twod::GameplayBridge> bridge_;
};

} // namespace nexora::editor
