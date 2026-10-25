#pragma once
#include "Nexora/Game/GameWorld.h"
#include "Nexora/Runtime/GameProjectPackage.h"
#include "nexora/gameplay_scene.h"
#include <array>
#include <functional>
#include <thread>

namespace nexora::runtime {
enum class GameSceneError : std::int32_t {
  None = 0,
  InvalidArgument = -1,
  Unsupported = -2,
  Lifecycle = -3,
  Busy = -4,
  NotFound = -5,
  Capacity = -6
};
enum class GameSceneLoadMode : std::uint32_t { Replace = 0, Additive = 1 };
struct GameSceneInstance final {
  Id scene{};
  AssetUuid asset;
  std::shared_ptr<const LoadedStaticProject> source;
  std::unordered_map<Id, Id> entities; // Authored ID -> fresh live ID.
};
struct GameSceneView final {
  const World &world;
  std::span<const GameSceneInstance> instances;
  Id primary{}, camera{};
};
struct GameSceneStats final {
  std::size_t scene_records{}, instances{}, stored_entities{}, audio_bindings{}, playing_audio{};
  std::size_t physics_bindings{}, character_bindings{};
};
// Prepare owns unpublished native resources. Commit is allocation-free/noexcept and publishes
// only after the live World is committed; destruction retires old GPU owners after host draining.
class NEXORA_RUNTIME_API GameScenePresentation {
public:
  virtual ~GameScenePresentation() = default;
  virtual void Commit() noexcept = 0;
};
class NEXORA_RUNTIME_API GameSceneSession final {
public:
  using Prepare =
      std::function<std::unique_ptr<GameScenePresentation>(const GameSceneView &, std::string &)>;
  GameSceneSession(game::GameWorld &, std::shared_ptr<const LoadedGameProject>);
  ~GameSceneSession();
  GameSceneSession(const GameSceneSession &) = delete;
  GameSceneSession &operator=(const GameSceneSession &) = delete;
  [[nodiscard]] bool Owner() const noexcept;
  GameSceneError RequestLoad(AssetUuid, GameSceneLoadMode, bool persistent, std::uint64_t &request);
  GameSceneError RequestUnload(Id, std::uint64_t &request);
  // Host calls outside module callbacks after World::EndFrame. No prepare means CPU-only host.
  // Any candidate/prepare exception records failure and retains every live World/native owner.
  bool Process(const Prepare &prepare = {});
  void Stop() noexcept;
  [[nodiscard]] GameSceneView View() const;
  [[nodiscard]] GameSceneStats Stats() const noexcept;
  [[nodiscard]] std::string_view ErrorText() const noexcept;
  [[nodiscard]] GameSceneError Status(std::uint64_t, NexoraSceneTransitionV1 &) const noexcept;
  [[nodiscard]] NexoraGameplaySceneApiV1 Api() noexcept;

private:
  struct Request final {
    std::uint64_t id{};
    AssetUuid asset;
    Id unload{};
    GameSceneLoadMode mode{};
    bool persistent{};
  };
  [[nodiscard]] Id Camera(const World &, Id) const noexcept;
  void Error(std::string_view) noexcept;
  game::GameWorld &game_;
  std::shared_ptr<const LoadedGameProject> project_;
  std::vector<GameSceneInstance> instances_;
  std::optional<Request> pending_;
  NexoraSceneTransitionV1 status_{sizeof(NexoraSceneTransitionV1), 0, 0, 0, 0, 0};
  NexoraSceneTransitionV1 completed_{sizeof(NexoraSceneTransitionV1), 0, 0, 0, 0, 0};
  std::array<char, 1024> error_{};
  std::uint64_t next_request_{1};
  Id primary_{};
  bool processing_{}, stopped_{};
  const std::thread::id owner_{std::this_thread::get_id()};
};
// Owning live transforms + immutable asset owners; source mappings are never renderer pointers.
[[nodiscard]] NEXORA_RUNTIME_API std::optional<std::vector<StaticRenderItem>>
CaptureGameSceneRenderItems(const GameSceneView &, std::string *error = nullptr);
} // namespace nexora::runtime
