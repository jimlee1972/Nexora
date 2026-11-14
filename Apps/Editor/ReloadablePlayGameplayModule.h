#pragma once
#include "Nexora/Editor/GameScriptBuild.h"
#include "PlayGameplayModule.h"
#include "PlayWorldReload.h"
#include <type_traits>

namespace nexora::editor::preview {
// Owner-thread main Editor facade. The Runtime host owns code lifetime; each generation owns a
// stable Host/services/proxy context and private allocation ledger until its DLL finalizers end.
// The PlaySession's World address never changes. Compilation remains outside this transaction.
class ReloadablePlayGameplayModule final {
public:
  using AllocationSnapshot = PlayGameplayModule::AllocationSnapshot;
  using LogSink = PlayGameplayModule::LogSink;
  using LogRejectionSink = PlayGameplayModule::LogRejectionSink;
  using ReloadFailure = runtime::GameplayModuleHost::ReloadFailure;
  explicit ReloadablePlayGameplayModule(LogSink log = {}, LogRejectionSink rejected = {})
      : log_(std::move(log)), rejected_(std::move(rejected)) {}
  ~ReloadablePlayGameplayModule() { Unload(); }
  ReloadablePlayGameplayModule(const ReloadablePlayGameplayModule &) = delete;
  ReloadablePlayGameplayModule &operator=(const ReloadablePlayGameplayModule &) = delete;

  bool LoadRelative(runtime::World &world, const std::filesystem::path &root,
                    std::string_view relative, std::string &error,
                    std::shared_ptr<const MeshAssetCatalog> assets = {},
                    const PlayTwoDBinding &two_d = {}, NexoraGameplaySceneApiV1 scene = {},
                    PlayWorldReload reload = {}) try {
    Unload();
    const std::filesystem::path name{std::u8string(relative.begin(), relative.end())};
    if (name.empty() || name.is_absolute() || name.has_root_name()) {
      error = "Choose a gameplay library relative to the project root.";
      return false;
    }
    std::error_code ec;
    const auto directory = std::filesystem::canonical(root, ec);
    if (ec) {
      error = "Project root is unavailable.";
      return false;
    }
    const auto library = std::filesystem::canonical(directory / name, ec);
    const auto local = library.lexically_relative(directory);
    if (ec || local.empty() || local.is_absolute() || *local.begin() == ".." ||
        !std::filesystem::is_regular_file(library, ec) || ec) {
      error = "Gameplay library must be a file inside this project.";
      return false;
    }
    if (!Prepare(world, std::move(assets), two_d, scene, reload, true) || !module_->Load(library)) {
      Unload();
      error = "Gameplay library could not start (path, ABI, or lifecycle failure).";
      return false;
    }
    error.clear();
    return true;
  } catch (...) {
    Unload();
    error = "Gameplay library path or load failed.";
    return false;
  }
  bool Load(runtime::World &world, NexoraGameModuleLoadV3Fn loader,
            std::shared_ptr<const MeshAssetCatalog> assets = {}, const PlayTwoDBinding &two_d = {},
            NexoraGameplaySceneApiV1 scene = {}, PlayWorldReload reload = {}) try {
    Unload();
    if (Prepare(world, std::move(assets), two_d, scene, reload, false) && module_->Load(loader))
      return true;
    Unload();
    return false;
  } catch (...) {
    Unload();
    return false;
  }
  // Trusted compiler result outside the authored project; owner retained through DLL unload.
  bool LoadPrepared(runtime::World &world, PreparedGameScript prepared, std::string &error,
                    std::shared_ptr<const MeshAssetCatalog> assets = {},
                    const PlayTwoDBinding &two_d = {}, NexoraGameplaySceneApiV1 scene = {},
                    PlayWorldReload reload = {}) try {
    if (!prepared.Current(&error))
      return false;
    Unload();
    if (!Prepare(world, std::move(assets), two_d, scene, reload, true) ||
        !module_->Load(prepared.Library())) {
      Unload();
      error = "Compiled gameplay library could not start.";
      return false;
    }
    prepared_.emplace(std::move(prepared));
    error.clear();
    return true;
  } catch (...) {
    Unload();
    error = "Compiled gameplay library preparation failed.";
    return false;
  }
  bool Reload(NexoraGameModuleLoadV3Fn loader) {
    if (!CanReload())
      return false;
    return module_->Reload(loader);
  }
  bool ReloadPrepared(PreparedGameScript prepared, std::string &error) try {
    if (!prepared.Current(&error) || !CanReload()) {
      if (error.empty())
        error = "Gameplay requires a restart or an available isolated reload owner.";
      return false;
    }
    // Both result owners remain alive until Runtime finishes candidate/old DLL finalizers.
    if (!module_->Reload(prepared.Library())) {
      error = "Gameplay replacement rejected; the previous module and state remain active.";
      return false;
    }
    prepared_.emplace(std::move(prepared));
    error.clear();
    return true;
  } catch (...) {
    error = "Gameplay replacement preparation failed; the previous module remains active.";
    return false;
  }
  [[nodiscard]] ReloadFailure GetReloadFailure() const noexcept {
    return admission_failure_ != ReloadFailure::None ? admission_failure_
           : module_                                 ? module_->GetReloadFailure()
                                                     : ReloadFailure::NotLoaded;
  }
  [[nodiscard]] std::uint64_t Generation() const noexcept {
    return module_ ? module_->Generation() : 0;
  }
  [[nodiscard]] runtime::GameplayModuleHost::WorkLease AcquireWork(std::uint64_t generation) {
    return module_ ? module_->AcquireWork(generation) : runtime::GameplayModuleHost::WorkLease{};
  }
  [[nodiscard]] bool IsLoaded() const noexcept { return module_ && module_->IsLoaded(); }
  [[nodiscard]] bool IsBoundTo(const runtime::World *world) const noexcept {
    return active_ && active_->IsBoundTo(world);
  }
  bool FixedUpdate(double seconds) {
    return !IsLoaded() || !module_->SupportsFixedUpdate() || module_->FixedUpdate(seconds);
  }
  bool Update(double seconds) {
    if (!IsLoaded())
      return true;
    if (!std::isfinite(seconds) || seconds < 0)
      return false;
    PlayGameplayModule::Increment(active_->diagnostics_frame_);
    if (!module_->Update(seconds))
      return false;
#if defined(NEXORA_HAS_2D)
    if (active_->two_d_)
      active_->two_d_->UpdateAudio(seconds);
#endif
    return true;
  }
  bool SetInputUser(std::uint32_t user) noexcept {
    if (active_)
      return active_->SetInputUser(user);
    if (user >= PlayInputBindings::kMaximumUsers)
      return false;
    if (idle_user_ != user)
      idle_input_.SetFocused(false);
    idle_user_ = user;
    return true;
  }
  [[nodiscard]] std::uint32_t InputUser() const noexcept {
    return active_ ? active_->InputUser() : idle_user_;
  }
  void SetInputFocus(bool focused) noexcept {
    if (active_)
      active_->SetInputFocus(focused);
    else
      idle_input_.SetFocused(focused);
  }
  bool SetInputBindings(const PlayInputBindings &bindings) noexcept {
    return active_ ? active_->SetInputBindings(bindings) : idle_input_.SetBindings(bindings);
  }
  void ProcessInput(std::span<const Nexora::Window::WindowEvent> events) noexcept {
    if (active_)
      active_->ProcessInput(events);
    else
      static_cast<void>(idle_input_.Process(events));
  }
  bool AdvanceDebugLines(double seconds) noexcept {
    return active_ ? active_->AdvanceDebugLines(seconds) : std::isfinite(seconds) && seconds >= 0;
  }
  [[nodiscard]] std::span<const NexoraDebugLine> DebugLines() const noexcept {
    return active_ ? active_->DebugLines() : std::span<const NexoraDebugLine>{};
  }
  [[nodiscard]] AllocationSnapshot Allocations() const noexcept {
    if (!active_)
      return {};
    auto result = active_->Allocations();
    Accumulate(result, history_);
    return result;
  }
  [[nodiscard]] AllocationSnapshot LastTeardownAllocations() const noexcept { return last_; }
  [[nodiscard]] std::uint64_t AllocationTeardownSequence() const noexcept { return teardowns_; }
#if defined(NEXORA_HAS_2D)
  [[nodiscard]] std::shared_ptr<const TwoDPlayAssets> TwoDAssets() const noexcept {
    return active_ ? active_->TwoDAssets() : nullptr;
  }
  void SetGameViewport(Nexora::Presentation::SceneViewport viewport) noexcept {
    if (active_)
      active_->SetGameViewport(viewport);
  }
  [[nodiscard]] std::optional<PlayTwoDFrame> TwoDFrame(Nexora::Presentation::SceneViewport viewport,
                                                       bool sprites = true) {
    return active_ ? active_->TwoDFrame(viewport, sprites) : std::nullopt;
  }
  [[nodiscard]] std::optional<twod::SpriteFrame>
  ExtractTwoDFrame(std::uint32_t width, std::uint32_t height,
                   const PlayTwoDServices::TextureResolver &textures) {
    return active_ ? active_->ExtractTwoDFrame(width, height, textures) : std::nullopt;
  }
  [[nodiscard]] std::vector<std::uint64_t> TakeRetiredTwoDTextures() noexcept {
    std::vector<std::uint64_t> result;
    result.swap(retired_textures_);
    return result;
  }
#endif
  void Unload() noexcept {
    if (active_) {
      idle_input_ = active_->input_;
      active_->SetInputFocus(false);
    }
    idle_input_.SetFocused(false);
    idle_user_ = 0;
    module_.reset(); // Drains actual work, then Stop/Destroy/library finalizers.
    prepared_.reset();
    Release(candidate_);
    Release(active_, Retirement::Session);
    candidate_world_.reset();
    retired_world_.reset();
    world_ = nullptr;
    world_reload_ = {};
    admission_failure_ = ReloadFailure::None;
    history_ = {};
  }

private:
  using Phase = PlayServiceGate::Phase;
  enum class Retirement { Discard, Generation, Session };
  static void Accumulate(AllocationSnapshot &target, const AllocationSnapshot &source) noexcept {
    target.peak_allocations = std::max(target.peak_allocations, source.peak_allocations);
    target.peak_bytes = std::max(target.peak_bytes, source.peak_bytes);
    const auto add = [](std::uint64_t &to, std::uint64_t from) {
      to = from > std::numeric_limits<std::uint64_t>::max() - to
               ? std::numeric_limits<std::uint64_t>::max()
               : to + from;
    };
    add(target.rejected_allocations, source.rejected_allocations);
    add(target.rejected_deallocations, source.rejected_deallocations);
    add(target.reclaimed_allocations, source.reclaimed_allocations);
    add(target.reclaimed_bytes, source.reclaimed_bytes);
  }
  bool Prepare(runtime::World &world, std::shared_ptr<const MeshAssetCatalog> assets,
               const PlayTwoDBinding &two_d, NexoraGameplaySceneApiV1 scene, PlayWorldReload reload,
               bool native) {
    if ((assets && !assets->Generation()) || (reload.Present() && !reload.Complete()))
      return false;
    active_ = std::make_unique<PlayGameplayModule>(log_, rejected_);
    if (!active_->scenes_.Bind(world))
      return false;
    active_->world_ = world_ = &world;
    active_->input_ = idle_input_;
    active_->mesh_assets_ = std::move(assets);
    world_reload_ = reload;
    if (!active_->BindPackagedScene(scene, two_d.scene) || !active_->BindTwoD(world, two_d, native))
      return false;
    module_ = std::make_unique<runtime::GameplayModuleHost>(
        active_->Host(), runtime::GameplayModuleHost::QuiescenceBarrier{}, Transaction());
    return true;
  }
  bool CanReload() noexcept {
    admission_failure_ = ReloadFailure::None;
    if (!IsLoaded()) {
      admission_failure_ = ReloadFailure::NotLoaded;
      return false;
    }
    if (active_->packaged_services_.scene.struct_size && !world_reload_.Complete()) {
      admission_failure_ = ReloadFailure::RestartRequired;
      return false;
    }
    return true;
  }
  void Release(std::unique_ptr<PlayGameplayModule> &context,
               Retirement kind = Retirement::Discard) noexcept {
    if (!context)
      return;
    const bool bound = context->world_ != nullptr;
    context->Unload();
    if (bound && kind == Retirement::Generation)
      Accumulate(history_, context->LastTeardownAllocations());
    if (bound && kind == Retirement::Session) {
      last_ = context->LastTeardownAllocations();
      Accumulate(last_, history_);
      PlayGameplayModule::Increment(teardowns_);
    }
#if defined(NEXORA_HAS_2D)
    auto textures = context->TakeRetiredTwoDTextures();
    try {
      retired_textures_.insert(retired_textures_.end(), textures.begin(), textures.end());
    } catch (...) {
    }
#endif
    context.reset();
  }
  bool Begin(NexoraGameplayHostV3 &host) noexcept try {
    if (!active_->gate_.SetPhase(Phase::Capture))
      return false;
    runtime::World *staged{};
    NexoraGameplaySceneApiV1 scene{};
    if (world_reload_.Complete()) {
      if (!world_reload_.begin(world_reload_.context, staged, scene))
        return false;
    } else {
      candidate_world_ = std::make_unique<runtime::World>(*world_);
      retired_world_ = std::make_unique<runtime::World>(runtime::WorldKind::Play);
      staged = candidate_world_.get();
    }
    if (!staged || staged == world_ || staged->Kind() != runtime::WorldKind::Play)
      return false;
    candidate_ = std::make_unique<PlayGameplayModule>(log_, rejected_);
    if (!candidate_->gate_.SetPhase(Phase::Candidate))
      return false;
    candidate_->world_ = staged;
    auto scenes = active_->scenes_.ForkForReload(*staged);
    if (!scenes)
      return false;
    candidate_->scenes_ = std::move(*scenes);
    candidate_->mesh_assets_ = active_->mesh_assets_;
    candidate_->input_ = active_->input_;
    candidate_->input_user_ = active_->input_user_;
    candidate_->debug_lines_ = active_->debug_lines_;
    candidate_->diagnostics_frame_ = active_->diagnostics_frame_;
    candidate_->diagnostics_errors_ = active_->diagnostics_errors_;
    if (!candidate_->BindPackagedScene(scene, 0))
      return false;
#if defined(NEXORA_HAS_2D)
    if (active_->two_d_) {
      candidate_->two_d_ = active_->two_d_->ForkForReload(*staged, candidate_->gate_);
      if (!candidate_->two_d_)
        return false;
    }
#endif
    host = candidate_->Host();
    host.capabilities |= NEXORA_GAMEPLAY_CAPABILITY_RELOAD_CANDIDATE;
    return true;
  } catch (...) {
    return false;
  }
  bool Validate() noexcept {
    if (!candidate_ || !candidate_->gate_.Valid() || !active_->gate_.Valid() ||
        !candidate_->scenes_.RebindForReload(*candidate_->world_))
      return false;
#if defined(NEXORA_HAS_2D)
    if (candidate_->two_d_ && (!candidate_->two_d_->PrepareReloadCommit() ||
                               !candidate_->two_d_->RebindForReload(*candidate_->world_)))
      return false;
#endif
    return !world_reload_.Complete() || world_reload_.validate(world_reload_.context);
  }
  void Rollback() noexcept {
    Release(candidate_); // Runtime already destroyed candidate code and library finalizers.
    candidate_world_.reset();
    retired_world_.reset();
    if (world_reload_.Complete())
      world_reload_.rollback(world_reload_.context);
    static_cast<void>(active_->gate_.SetPhase(Phase::Live));
  }
  void Retire() noexcept {
    static_assert(std::is_nothrow_move_assignable_v<runtime::World>);
    runtime::World *retired{};
    if (world_reload_.Complete()) {
      world_reload_.retire(world_reload_.context, retired);
    } else {
      *retired_world_ = std::move(*world_);
      *world_ = std::move(*candidate_world_);
      retired = retired_world_.get();
    }
    active_->world_ = retired;
    candidate_->world_ = world_;
    // Final admission established these identical-move invariants before publication. A host
    // invariant violation cannot safely resume either generation after native-owner transfer.
    bool valid = retired && active_->scenes_.RebindForReload(*retired) &&
                 candidate_->scenes_.RebindForReload(*world_) &&
                 active_->gate_.SetPhase(Phase::Retired);
#if defined(NEXORA_HAS_2D)
    if (valid && active_->two_d_)
      valid = active_->two_d_->RebindForReload(*retired) &&
              candidate_->two_d_->RebindForReload(*world_) &&
              candidate_->two_d_->CommitReloadResources(*active_->two_d_);
#endif
    if (!valid)
      std::terminate();
  }
  void Commit() noexcept {
    // Old callbacks/finalizers finished; preserve session counters without publishing a Stop.
    Release(active_, Retirement::Generation);
    active_ = std::move(candidate_);
    if (world_reload_.Complete())
      world_reload_.commit(world_reload_.context);
    candidate_world_.reset();
    retired_world_.reset();
    static_cast<void>(active_->gate_.SetPhase(Phase::Live));
  }
  runtime::GameplayModuleHost::ReloadTransaction Transaction() noexcept {
    return {
        this,
        [](void *p, NexoraGameplayHostV3 &host) noexcept {
          return static_cast<ReloadablePlayGameplayModule *>(p)->Begin(host);
        },
        [](void *p) noexcept { static_cast<ReloadablePlayGameplayModule *>(p)->Rollback(); },
        [](void *p) noexcept { static_cast<ReloadablePlayGameplayModule *>(p)->Retire(); },
        [](void *p) noexcept { static_cast<ReloadablePlayGameplayModule *>(p)->Commit(); },
        [](void *p) noexcept {
          return static_cast<ReloadablePlayGameplayModule *>(p)->active_->gate_.SetPhase(
              Phase::Capture);
        },
        [](void *p) noexcept {
          return static_cast<ReloadablePlayGameplayModule *>(p)->active_->gate_.Valid();
        },
        [](void *p) noexcept {
          static_cast<void>(
              static_cast<ReloadablePlayGameplayModule *>(p)->active_->gate_.SetPhase(Phase::Live));
        },
        [](void *p) noexcept {
          return static_cast<ReloadablePlayGameplayModule *>(p)->Validate();
        }};
  }
  LogSink log_;
  LogRejectionSink rejected_;
  runtime::World *world_{};
  PlayWorldReload world_reload_;
  std::unique_ptr<PlayGameplayModule> active_, candidate_;
  std::unique_ptr<runtime::World> candidate_world_, retired_world_;
  std::unique_ptr<runtime::GameplayModuleHost> module_;
  std::optional<PreparedGameScript> prepared_;
  AllocationSnapshot last_{};
  AllocationSnapshot history_{};
  std::uint64_t teardowns_{};
  ReloadFailure admission_failure_{};
  PlayInputState idle_input_;
  std::uint32_t idle_user_{};
#if defined(NEXORA_HAS_2D)
  std::vector<std::uint64_t> retired_textures_;
#endif
};
} // namespace nexora::editor::preview
