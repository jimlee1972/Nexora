#pragma once
#include "../Common/NativeSceneView.h"
#include "Nexora/Runtime/GameSceneSession.h"
#include <stdexcept>

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
      : project_(std::move(project)), scenes_(world_, project_) {
    std::uint64_t request{};
    if (scenes_.RequestLoad(project_->StartupScene(), runtime::GameSceneLoadMode::Replace, false,
                            request) != runtime::GameSceneError::None)
      throw std::runtime_error("Configured startup scene could not be queued.");
  }
  PackagedPlayContext(const PackagedPlayContext &) = delete;
  PackagedPlayContext &operator=(const PackagedPlayContext &) = delete;
  [[nodiscard]] game::GameWorld &World() noexcept { return world_; }
  [[nodiscard]] runtime::GameSceneSession &Scenes() noexcept { return scenes_; }
  [[nodiscard]] bool Ready() const noexcept { return bool(published_); }
  [[nodiscard]] std::uint64_t Commits() const noexcept { return commits_; }

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
    const auto processed =
        scenes_.Process([&](const runtime::GameSceneView &view,
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
      result.error = scenes_.ErrorText();
      if (!published_ || (native_failed && !sceneview::CanRedrawRetainedScene(result.status))) {
        if (result.status == SurfaceStatus::Ready)
          result.status = SurfaceStatus::Unsupported;
        return result;
      }
    }
    if (!result.drawn) {
      const auto view = scenes_.View();
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
  runtime::GameSceneSession scenes_;
  std::shared_ptr<sceneview::StaticView> published_;
  std::uint64_t commits_{};
};
} // namespace nexora::editor::preview
