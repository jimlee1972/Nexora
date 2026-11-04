#pragma once
#include "../Common/NativeSceneView.h"
#include "Nexora/Runtime/GameSceneSession.h"
#include "PlayWorldReload.h"
#include <stdexcept>
#include <type_traits>

namespace nexora::editor::preview {
// Owner-thread isolated packaged game. This owner outlives the PlaySession borrow and module;
// Stop/Destroy finish before its World/session are released. Authoring documents are never held.
class PackagedPlayContext final {
public:
  using Draw = std::function<Nexora::Presentation::SurfaceStatus(
      const Nexora::Presentation::SceneDrawData &)>;
  struct Frame final {
    Nexora::Presentation::SurfaceStatus status{Nexora::Presentation::SurfaceStatus::Ready};
    bool committed{}, rejected{}, drawn{};
    std::string error;
  };
  explicit PackagedPlayContext(std::shared_ptr<const runtime::LoadedGameProject> project)
      : project_(std::move(project)),
        scenes_(std::make_unique<runtime::GameSceneSession>(world_, project_)) {
    std::uint64_t request{};
    if (scenes_->RequestLoad(project_->StartupScene(), runtime::GameSceneLoadMode::Replace, false,
                             request) != runtime::GameSceneError::None)
      throw std::runtime_error("Configured startup scene could not be queued.");
  }
  PackagedPlayContext(const PackagedPlayContext &) = delete;
  PackagedPlayContext &operator=(const PackagedPlayContext &) = delete;
  [[nodiscard]] game::GameWorld &World() noexcept { return world_; }
  [[nodiscard]] runtime::GameSceneSession &Scenes() noexcept { return *scenes_; }
  [[nodiscard]] bool Ready() const noexcept { return bool(published_); }
  [[nodiscard]] std::uint64_t Commits() const noexcept { return commits_; }
  [[nodiscard]] PlayWorldReload ReloadHooks() noexcept {
    return {this,
            [](void *p, runtime::World *&world, NexoraGameplaySceneApiV1 &api) noexcept {
              return static_cast<PackagedPlayContext *>(p)->BeginReload(world, api);
            },
            [](void *p) noexcept {
              auto &self = *static_cast<PackagedPlayContext *>(p);
              return self.staged_scenes_ &&
                     self.staged_scenes_->RebindForReload(*self.staged_world_);
            },
            [](void *p, runtime::World *&retired) noexcept {
              static_cast<PackagedPlayContext *>(p)->RetireReload(retired);
            },
            [](void *p) noexcept {
              auto &self = *static_cast<PackagedPlayContext *>(p);
              self.scenes_ = std::move(self.staged_scenes_);
              self.staged_world_.reset();
              self.retired_world_.reset();
            },
            [](void *p) noexcept {
              auto &self = *static_cast<PackagedPlayContext *>(p);
              self.staged_scenes_.reset();
              self.staged_world_.reset();
              self.retired_world_.reset();
            }};
  }

  // Invoke after gameplay callbacks, inside the acquired native render frame. Draw borrows all
  // descriptor spans only for its call. The real surface owns uploaded/fence resources. A CPU
  // admission failure keeps the old World/palette and draws it; a native backend failure is
  // returned to the host's surface-recovery policy with the old World retained.
  Frame Render(Nexora::Presentation::SceneViewport viewport, bool retain_geometry,
               const Draw &draw) {
    using Nexora::Presentation::SurfaceStatus;
    Frame result;
    if (!viewport.width || !viewport.height || !draw) {
      result.status = SurfaceStatus::Unsupported;
      result.error = "Packaged Play needs a valid native view and draw owner.";
      return result;
    }
    const auto aspect = static_cast<float>(viewport.width) / static_cast<float>(viewport.height);
    world_.EndFrame();
    const auto submit = [&](const sceneview::StaticView &palette, const runtime::World &world) {
      auto data = palette.DrawData(world, aspect);
      if (!data) {
        result.error = "Packaged Play camera projection is invalid.";
        return SurfaceStatus::Unsupported;
      }
      data->viewport = viewport;
      data->retainMeshGeometry = retain_geometry;
      return palette.instances.empty() ? SurfaceStatus::Ready : draw(*data);
    };
    bool native_failed{};
    const auto processed = scenes_->Process(
        [&](const runtime::GameSceneView &view,
            std::string &error) -> std::unique_ptr<runtime::GameScenePresentation> {
          const auto items = runtime::CaptureGameSceneRenderItems(view, &error);
          auto palette =
              items ? sceneview::PrepareSceneView(*items, view.world, view.primary, &error, true)
                    : std::nullopt;
          if (!palette)
            return {};
          auto staged = std::make_shared<sceneview::StaticView>(std::move(*palette));
          // Allocate every CPU owner before recording native work or publishing the World.
          auto presentation = std::make_unique<Presentation>(*this, staged);
          const auto status = submit(*staged, view.world);
          if (status != SurfaceStatus::Ready) {
            result.status = status;
            native_failed = true;
            error = result.error.empty() ? "Packaged Play native candidate preparation failed."
                                         : result.error;
            return {};
          }
          result.drawn = true;
          result.committed = true;
          return presentation;
        });
    if (!processed) {
      result.rejected = true;
      result.committed = false;
      result.error = scenes_->ErrorText();
      if (!published_ || (native_failed && !sceneview::CanRedrawRetainedScene(result.status))) {
        if (result.status == SurfaceStatus::Ready)
          result.status = SurfaceStatus::Unsupported;
        return result;
      }
    }
    if (!result.drawn) {
      const auto view = scenes_->View();
      const auto items = runtime::CaptureGameSceneRenderItems(view, &result.error);
      const auto live =
          items ? sceneview::PrepareSceneView(*items, view.world, view.primary, &result.error, true)
                : std::nullopt;
      if (!live) {
        result.status = SurfaceStatus::Unsupported;
        return result;
      }
      result.status = submit(*live, view.world);
      result.drawn = result.status == SurfaceStatus::Ready;
    }
    return result;
  }

private:
  bool BeginReload(runtime::World *&world, NexoraGameplaySceneApiV1 &api) noexcept try {
    if (!scenes_->Owner() || !Ready() || staged_world_ || staged_scenes_ || retired_world_)
      return false;
    // GameWorld is CPU state: this copies existing audio/physics/character bindings and their
    // clocks. It has no native audio device or renderer owner. Native palette remains retained.
    retired_world_ = std::make_unique<game::GameWorld>(runtime::WorldKind::Play);
    staged_world_ = std::make_unique<game::GameWorld>(world_);
    staged_scenes_ = scenes_->ForkForReload(*staged_world_);
    if (!staged_scenes_)
      return false;
    world = &staged_world_->InternalWorld();
    api = staged_scenes_->Api();
    return true;
  } catch (...) {
    return false;
  }
  void RetireReload(runtime::World *&retired) noexcept {
    static_assert(std::is_nothrow_move_assignable_v<game::GameWorld>);
    *retired_world_ = std::move(world_);
    world_ = std::move(*staged_world_);
    if (!scenes_->RebindForReload(*retired_world_) || !staged_scenes_->RebindForReload(world_))
      std::terminate();
    retired = &retired_world_->InternalWorld();
  }
  struct Presentation final : runtime::GameScenePresentation {
    PackagedPlayContext &owner;
    std::shared_ptr<sceneview::StaticView> palette;
    Presentation(PackagedPlayContext &context, std::shared_ptr<sceneview::StaticView> prepared)
        : owner(context), palette(std::move(prepared)) {}
    void Commit() noexcept override {
      owner.published_.swap(palette);
      ++owner.commits_;
    }
  };
  game::GameWorld world_{runtime::WorldKind::Play};
  std::shared_ptr<const runtime::LoadedGameProject> project_;
  std::unique_ptr<runtime::GameSceneSession> scenes_;
  // Staged/retired session destruction precedes the World it borrows.
  std::unique_ptr<game::GameWorld> staged_world_, retired_world_;
  std::unique_ptr<runtime::GameSceneSession> staged_scenes_;
  std::shared_ptr<sceneview::StaticView> published_;
  std::uint64_t commits_{};
};
} // namespace nexora::editor::preview
