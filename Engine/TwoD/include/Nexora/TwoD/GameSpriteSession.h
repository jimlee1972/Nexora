#pragma once
#include "Nexora/TwoD/GameSpriteAssets.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include <thread>

namespace nexora::twod {
// Optional main-game metadata on the embedding's existing Play World. No World, file watcher,
// module or native resource is owned here. Drain callbacks/preparations before destruction.
class NEXORA_2D_API GameSpriteSession final {
  struct State;

public:
  class NEXORA_2D_API Prepared final : public runtime::GameScenePresentation {
  public:
    ~Prepared() override;
    // Borrow only for this call, during the Runtime scene prepare callback. The World remains
    // read-only; no candidate World pointer is retained by this owning metadata preparation.
    [[nodiscard]] std::optional<SpriteFrame> Extract(const runtime::World &, std::uint32_t width,
                                                     std::uint32_t height);
    // Call only after Runtime has committed that exact candidate World. No allocation/callback.
    void Commit() noexcept override;

  private:
    friend class GameSpriteSession;
    Prepared(GameSpriteSession &, std::unique_ptr<State>, std::uintptr_t world_token);
    GameSpriteSession &owner_;
    std::unique_ptr<State> state_;
    std::uintptr_t world_token_{}; // Comparison only; never dereferenced or exposed to gameplay.
    bool committed_{};
  };

  class NEXORA_2D_API PreparedSources final {
  public:
    ~PreparedSources();
    PreparedSources(const PreparedSources &) = delete;
    PreparedSources &operator=(const PreparedSources &) = delete;
    [[nodiscard]] const GameSpriteSession &Session() const noexcept { return owner_; }
    [[nodiscard]] const GameSpriteAssets &Assets() const noexcept { return *assets_; }
    [[nodiscard]] std::optional<SpriteFrame> Extract(std::uint32_t width, std::uint32_t height);
    // Only after the embedding admits this exact catalog's native frame. No allocation/callback.
    void Commit() noexcept;

  private:
    friend class GameSpriteSession;
    PreparedSources(GameSpriteSession &, std::shared_ptr<const GameSpriteAssets>);
    GameSpriteSession &owner_;
    std::shared_ptr<const GameSpriteAssets> assets_;
    bool committed_{};
  };

  GameSpriteSession(runtime::World &, GameSpriteAssets, std::size_t pool_capacity = 4096);
  ~GameSpriteSession();
  GameSpriteSession(const GameSpriteSession &) = delete;
  GameSpriteSession &operator=(const GameSpriteSession &) = delete;
  // One outstanding preparation. Destruction without Commit discards it and retains live state.
  [[nodiscard]] std::unique_ptr<Prepared> Prepare(const runtime::GameSceneView &,
                                                  std::string *error = nullptr);
  // After World EndFrame, before scene preparation/rendering. Releases destroyed metadata and
  // animator/query state without replaying immutable authored references over gameplay edits.
  [[nodiscard]] bool Reconcile();
  [[nodiscard]] std::optional<SpriteFrame> Extract(std::uint32_t width, std::uint32_t height);
  [[nodiscard]] GameplayBridge &Bridge() noexcept { return *bridge_; }
  [[nodiscard]] InstancePool &Pool() noexcept { return pool_; }
  [[nodiscard]] runtime::World &WorldOwner() const noexcept { return *world_; }
  [[nodiscard]] bool Available() const noexcept;
  // Idle owner only. Resolve every entity/pool binding, including hidden and legacy Sprites,
  // without allocation, callbacks, publication or World/native-resource mutation.
  [[nodiscard]] bool ValidateBindings() const noexcept;
  [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }
  [[nodiscard]] const GameSpriteAssets &Assets() const noexcept { return *assets_; }
  // Freeze API writes while preparing a source/Atlas-only generation over unchanged live state.
  // Missing live bindings, inventory/Clip changes or any outstanding preparation reject.
  [[nodiscard]] std::unique_ptr<PreparedSources>
  PrepareSources(std::span<const runtime::RuntimeBlob>, std::string *error = nullptr);
  [[nodiscard]] runtime::Id Primary() const noexcept;
  // Serialized host/bridge borrows; expire at preparation publication or entity destruction.
  [[nodiscard]] SpriteScene *FindEntity(runtime::Id) noexcept;
  void ForgetEntity(runtime::Id);
  [[nodiscard]] std::optional<std::vector<Shape>> CollisionShapes() const;
  // Unpublished branch over an identical Play World clone. Immutable assets are shared;
  // Sprite/collider/animator/query values and pool slots are copied, preserving handles.
  [[nodiscard]] std::unique_ptr<GameSpriteSession> ForkForReload(runtime::World &) const;
  // Retarget identical moved storage at commit/retirement without changing API addresses.
  // Invalid membership, preparation, reentry or foreign-thread calls make no writes.
  [[nodiscard]] bool RebindForReload(runtime::World &) noexcept;

private:
  struct ReloadCopy final {};
  GameSpriteSession(const GameSpriteSession &, runtime::World &, ReloadCopy);
  [[nodiscard]] bool Owner() const noexcept;
  [[nodiscard]] const char *BindingError(const GameSpriteAssets &) const noexcept;
  [[nodiscard]] std::optional<SpriteFrame> ExtractState(const State &, const runtime::World &,
                                                        Camera, const GameSpriteAssets &);
  runtime::World *world_;
  std::shared_ptr<const GameSpriteAssets> assets_;
  InstancePool pool_;
  std::unique_ptr<State> state_;
  std::unique_ptr<GameplayBridge> bridge_;
  std::uint64_t revision_{};
  bool preparing_{};
  const std::thread::id thread_{std::this_thread::get_id()};
};
} // namespace nexora::twod
