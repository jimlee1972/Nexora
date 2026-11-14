#pragma once
#include "Nexora/TwoD/AuthoredScene.h"
#include "nexora/gameplay_scene.h"
#include <thread>

namespace nexora::twod {
struct SceneEntry final {
  std::string path;
  AssetId id;
  std::shared_ptr<const AuthoredScene> source;
};
struct SceneInstance final {
  AssetId asset;
  SpriteScene sprites;
  Tilemap tiles;
  std::unordered_map<runtime::Id, runtime::Id> entities;
};
// Owns an immutable bounded catalog and live 2D metadata, borrowing a Play World. Stop module
// callbacks before destruction. All operations require the construction thread. Process only
// outside gameplay callbacks after World::EndFrame; never expose a partially prepared World.
class NEXORA_2D_API SceneSession final {
public:
  SceneSession(runtime::World &, std::vector<SceneEntry>);
  ~SceneSession();
  SceneSession(const SceneSession &) = delete;
  SceneSession &operator=(const SceneSession &) = delete;
  [[nodiscard]] NexoraGameplaySceneApiV1 Api() noexcept;
  // Optional host validation runs on an unpublished World. False/exception preserves live state.
  using Prepare = std::function<bool(const runtime::World &, std::span<const SceneInstance>)>;
  bool Process(const Prepare & = {});
  void Stop() noexcept;
  [[nodiscard]] runtime::Id Primary() const noexcept { return primary_; }
  [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }
  [[nodiscard]] std::span<const SceneInstance> Instances() const noexcept { return instances_; }
  [[nodiscard]] runtime::World &WorldOwner() const noexcept { return world_; }
  void ForgetEntity(runtime::Id);
  [[nodiscard]] SpriteScene *FindEntity(runtime::Id) noexcept;
  [[nodiscard]] Tilemap *PrimaryTiles() noexcept;
  [[nodiscard]] std::optional<std::vector<Shape>> CollisionShapes() const;

private:
  friend class GameplayGeneration;
  [[nodiscard]] std::unique_ptr<SceneSession> StageInto(runtime::World &) const;
  struct Request {
    std::uint64_t id{};
    AssetId asset;
    runtime::Id unload{};
    std::uint32_t mode{}, flags{};
  };
  [[nodiscard]] bool Owner() const noexcept;
  int Load(AssetId, std::uint32_t, std::uint32_t, std::uint64_t &);
  int Unload(runtime::Id, std::uint64_t &);
  runtime::World &world_;
  const std::vector<SceneEntry> catalog_;
  std::vector<SceneInstance> instances_;
  std::optional<Request> pending_;
  NexoraSceneTransitionV1 status_{sizeof(NexoraSceneTransitionV1), 0, 0, 0, 0, 0};
  NexoraSceneTransitionV1 completed_{sizeof(NexoraSceneTransitionV1), 0, 0, 0, 0, 0};
  runtime::Id primary_{};
  std::uint64_t next_{1}, revision_{};
  bool processing_{}, stopped_{};
  const std::thread::id owner_{std::this_thread::get_id()};
};
} // namespace nexora::twod
