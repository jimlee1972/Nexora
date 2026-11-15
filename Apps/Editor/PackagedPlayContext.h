#pragma once
#include "../Common/NativeSceneView.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/Runtime/GameSceneSession.h"
#include "PlayWorldReload.h"
#include <stdexcept>
#include <type_traits>
#if defined(NEXORA_HAS_2D)
#include "Nexora/TwoD/GameSpriteSession.h"
#endif

namespace nexora::twod {
struct SpriteFrame;
}
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
    std::size_t visible_sprites{};
    std::string error;
  };
  explicit PackagedPlayContext(std::shared_ptr<const runtime::LoadedGameProject> project)
      : project_(std::move(project)),
        scenes_(std::make_unique<runtime::GameSceneSession>(world_, project_)) {
#if defined(NEXORA_HAS_2D)
    std::string error;
    sprite_assets_ = twod::GameSpriteAssets::Load(project_->SpriteAssets(), &error);
    if (!sprite_assets_)
      throw std::runtime_error(error);
    if (!project_->SpriteAssets().empty()) {
      sprites_ = std::make_unique<twod::GameSpriteSession>(world_.InternalWorld(), *sprite_assets_);
      sprite_assets_.reset();
    }
#else
    if (!project_->SpriteAssets().empty())
      throw std::runtime_error("Configured Sprite Play requires TwoD support.");
#endif
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
    PlayWorldReload hooks{
        this,
        [](void *p, runtime::World *&world, NexoraGameplaySceneApiV1 &api) noexcept {
          return static_cast<PackagedPlayContext *>(p)->BeginReload(world, api);
        },
        [](void *p) noexcept { return static_cast<PackagedPlayContext *>(p)->ValidateReload(); },
        [](void *p, runtime::World *&retired) noexcept {
          static_cast<PackagedPlayContext *>(p)->RetireReload(retired);
        },
        [](void *p) noexcept {
          auto &self = *static_cast<PackagedPlayContext *>(p);
#if defined(NEXORA_HAS_2D)
          self.sprites_ = std::move(self.staged_sprites_);
#endif
          self.scenes_ = std::move(self.staged_scenes_);
          self.published_.swap(self.staged_palette_);
          self.staged_palette_.reset();
          self.staged_world_.reset();
          self.retired_world_.reset();
        },
        [](void *p) noexcept {
          auto &self = *static_cast<PackagedPlayContext *>(p);
#if defined(NEXORA_HAS_2D)
          self.staged_sprites_.reset();
#endif
          self.staged_scenes_.reset();
          self.staged_palette_.reset();
          self.staged_world_.reset();
          self.retired_world_.reset();
        }};
#if defined(NEXORA_HAS_2D)
    hooks.sprites = [](void *p) noexcept {
      return static_cast<PackagedPlayContext *>(p)->staged_sprites_.get();
    };
#endif
    return hooks;
  }

  [[nodiscard]] bool HasSprites() const noexcept { return !project_->SpriteAssets().empty(); }
#if defined(NEXORA_HAS_2D)
  [[nodiscard]] const twod::GameSpriteAssets &SpriteAssets() const noexcept {
    return sprites_ ? sprites_->Assets() : *sprite_assets_;
  }
  [[nodiscard]] twod::GameSpriteSession *Sprites() const noexcept { return sprites_.get(); }
  using CompositeDraw = std::function<Nexora::Presentation::SurfaceStatus(
      const Nexora::Presentation::SceneDrawData &, const twod::SpriteFrame &)>;
  // Synchronous owner-frame scope: Runtime invokes the presenter at final validation, before
  // old Stop/Destroy/library finalizers. No presenter or frame borrow survives this call.
  Frame RenderReload(Nexora::Presentation::SceneViewport viewport, bool retain_geometry,
                     const CompositeDraw &draw, const std::function<bool()> &reload) {
    Frame result;
    result.rejected = true;
    result.status = Nexora::Presentation::SurfaceStatus::Unsupported;
    if (!Ready() || reload_frame_ || !viewport.width || !viewport.height || !draw || !reload)
      return result;
    reload_frame_ = &result;
    reload_draw_ = &draw;
    reload_viewport_ = viewport;
    reload_retain_geometry_ = retain_geometry;
    struct Reset final {
      PackagedPlayContext &owner;
      ~Reset() {
        owner.reload_frame_ = nullptr;
        owner.reload_draw_ = nullptr;
      }
    } reset{*this};
    const bool applied = reload();
    result.committed = applied;
    result.rejected = !applied;
    return result;
  }
  [[nodiscard]] std::unique_ptr<twod::GameSpriteSession::PreparedSources>
  PrepareSourceUpdate(std::span<const runtime::RuntimeBlob> blobs, std::string *error) {
    if (!Ready() || !scenes_->Owner() || staged_world_ || !sprites_) {
      if (error)
        *error = "Sprite reimport requires an idle configured Play scene.";
      return {};
    }
    world_.EndFrame();
    if (!sprites_->Reconcile()) {
      if (error)
        *error = "Sprite reimport could not reconcile the current Play scene.";
      return {};
    }
    return sprites_->PrepareSources(blobs, error);
  }
  // Current Runtime World/scene ownership stays intact. The embedding admits new textures in
  // this exact full GUI submission, then exchanges its texture/catalog owners without callbacks.
  Frame RenderSourceUpdate(twod::GameSpriteSession::PreparedSources &update,
                           Nexora::Presentation::SceneViewport viewport, bool retain_geometry,
                           const CompositeDraw &draw) {
    using Nexora::Presentation::SurfaceStatus;
    Frame result;
    result.rejected = true;
    result.status = SurfaceStatus::Unsupported;
    result.error = "Configured Sprite reimport frame preparation failed.";
    if (!Ready() || &update.Session() != sprites_.get() || !viewport.width || !viewport.height ||
        !draw)
      return result;
    const auto view = scenes_->View();
    const auto items = runtime::CaptureGameSceneRenderItems(view, &result.error);
    const auto palette =
        items ? sceneview::PrepareSceneView(*items, view.world, view.primary, &result.error, true)
              : std::nullopt;
    const auto sprites = update.Extract(viewport.width, viewport.height);
    if (!palette || !sprites || sprites->statistics.unavailable)
      return result;
    auto data = palette->DrawData(view.world, float(viewport.width) / float(viewport.height));
    if (!data)
      return result;
    data->viewport = viewport;
    data->retainMeshGeometry = retain_geometry;
    result.status = draw(*data, *sprites);
    if (result.status == SurfaceStatus::Ready) {
      update.Commit();
      result.error.clear();
      result.rejected = false;
      result.drawn = true;
      result.visible_sprites = sprites->statistics.visible;
    }
    return result;
  }
  // The callback admits the complete Scene/Sprite/GUI submission before candidate publication.
  Frame RenderComposed(Nexora::Presentation::SceneViewport viewport, bool retain_geometry,
                       const CompositeDraw &draw) {
    std::size_t visible{};
    auto result =
        RenderPrepared(viewport, retain_geometry,
                       draw ? DrawView([&](const auto &data, const auto &,
                                           const twod::SpriteFrame *sprites, std::string &error) {
                         if (sprites && sprites->statistics.unavailable) {
                           error = "Configured Play Sprite extraction failed.";
                           return Nexora::Presentation::SurfaceStatus::Unsupported;
                         }
                         const twod::SpriteFrame empty;
                         visible = sprites ? sprites->statistics.visible : 0;
                         return draw(data, sprites ? *sprites : empty);
                       })
                            : DrawView{});
    result.visible_sprites = visible;
    return result;
  }
#endif

  // Invoke after gameplay callbacks, inside the acquired native render frame. Draw borrows all
  // descriptor spans only for its call. The real surface owns uploaded/fence resources. A CPU
  // admission failure keeps the old World/palette and draws it; a native backend failure is
  // returned to the host's surface-recovery policy with the old World retained.
  Frame Render(Nexora::Presentation::SceneViewport viewport, bool retain_geometry,
               const Draw &draw) {
    if (HasSprites()) {
      Frame result;
      result.status = Nexora::Presentation::SurfaceStatus::Unsupported;
      result.error = "Source packages require the owning Scene/Sprite compositor.";
      return result;
    }
    return RenderPrepared(viewport, retain_geometry,
                          draw ? DrawView([&](const auto &data, const auto &,
                                              const twod::SpriteFrame *, std::string &) {
                            return data.instances.empty()
                                       ? Nexora::Presentation::SurfaceStatus::Ready
                                       : draw(data);
                          })
                               : DrawView{});
  }

private:
  using DrawView = std::function<Nexora::Presentation::SurfaceStatus(
      const Nexora::Presentation::SceneDrawData &, const runtime::GameSceneView &,
      const twod::SpriteFrame *, std::string &)>;
  Frame RenderPrepared(Nexora::Presentation::SceneViewport viewport, bool retain_geometry,
                       const DrawView &draw) {
    using Nexora::Presentation::SurfaceStatus;
    Frame result;
    if (!viewport.width || !viewport.height || !draw) {
      result.status = SurfaceStatus::Unsupported;
      result.error = "Packaged Play needs a valid native view and draw owner.";
      return result;
    }
    const auto aspect = static_cast<float>(viewport.width) / static_cast<float>(viewport.height);
    world_.EndFrame();
#if defined(NEXORA_HAS_2D)
    if (sprites_ && !sprites_->Reconcile()) {
      result.status = SurfaceStatus::Unsupported;
      result.error = "Configured Play Sprite reconciliation failed.";
      return result;
    }
#endif
    const auto submit = [&](const sceneview::StaticView &palette,
                            const runtime::GameSceneView &view, const twod::SpriteFrame *sprites) {
      auto data = palette.DrawData(view.world, aspect);
      if (!data) {
        result.error = "Packaged Play camera projection is invalid.";
        return SurfaceStatus::Unsupported;
      }
      data->viewport = viewport;
      data->retainMeshGeometry = retain_geometry;
      const auto status = draw(*data, view, sprites, result.error);
      if (status != SurfaceStatus::Ready && result.error.empty()) {
        result.error = "Packaged Play presentation failed: ";
        result.error.append(Nexora::Presentation::ToString(status));
      }
      return status;
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
#if defined(NEXORA_HAS_2D)
          std::optional<twod::SpriteFrame> sprite_frame;
          if (sprites_) {
            presentation->sprites = sprites_->Prepare(view, &error);
            if (!presentation->sprites)
              return {};
            sprite_frame =
                presentation->sprites->Extract(view.world, viewport.width, viewport.height);
            if (!sprite_frame || sprite_frame->statistics.unavailable) {
              error = "Configured Play candidate Sprite extraction failed.";
              return {};
            }
          }
          const auto status = submit(*staged, view, sprite_frame ? &*sprite_frame : nullptr);
#else
          const auto status = submit(*staged, view, nullptr);
#endif
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
#if defined(NEXORA_HAS_2D)
      std::optional<twod::SpriteFrame> sprite_frame;
      if (sprites_) {
        sprite_frame = sprites_->Extract(viewport.width, viewport.height);
        if (!sprite_frame || sprite_frame->statistics.unavailable) {
          result.status = SurfaceStatus::Unsupported;
          result.error = "Configured Play live Sprite extraction failed.";
          return result;
        }
      }
      result.status = submit(*live, view, sprite_frame ? &*sprite_frame : nullptr);
#else
      result.status = submit(*live, view, nullptr);
#endif
      result.drawn = result.status == SurfaceStatus::Ready;
    }
    return result;
  }

private:
  bool ValidateReload() noexcept try {
    using Nexora::Presentation::SurfaceStatus;
    if (!staged_world_ || !staged_scenes_)
      return false;
    staged_world_->EndFrame();
    if (!staged_scenes_->RebindForReload(*staged_world_))
      return false;
#if defined(NEXORA_HAS_2D)
    if (staged_sprites_ && (!staged_sprites_->RebindForReload(staged_world_->InternalWorld()) ||
                            !staged_sprites_->Reconcile() || !staged_sprites_->ValidateBindings()))
      return false;
#endif
    std::string error;
    const auto view = staged_scenes_->View();
    const auto items = runtime::CaptureGameSceneRenderItems(view, &error);
    auto palette = items
                       ? sceneview::PrepareSceneView(*items, view.world, view.primary, &error, true)
                       : std::nullopt;
    if (!palette)
      return false;
    staged_palette_ = std::make_shared<sceneview::StaticView>(std::move(*palette));
#if defined(NEXORA_HAS_2D)
    const auto viewport =
        reload_frame_ ? reload_viewport_ : Nexora::Presentation::SceneViewport{0, 0, 1, 1};
    const auto sprites = staged_sprites_ ? staged_sprites_->Extract(viewport.width, viewport.height)
                                         : std::optional<twod::SpriteFrame>{twod::SpriteFrame{}};
    if (!sprites || sprites->statistics.unavailable)
      return false;
    auto data =
        staged_palette_->DrawData(view.world, float(viewport.width) / float(viewport.height));
    if (!data)
      return false;
    if (reload_frame_) {
      data->viewport = viewport;
      data->retainMeshGeometry = reload_retain_geometry_;
      reload_frame_->status = (*reload_draw_)(*data, *sprites);
      reload_frame_->drawn = reload_frame_->status == SurfaceStatus::Ready;
      reload_frame_->visible_sprites = sprites->statistics.visible;
      return reload_frame_->drawn;
    }
#endif
    return true;
  } catch (...) {
    return false;
  }
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
#if defined(NEXORA_HAS_2D)
    if (sprites_) {
      staged_sprites_ = sprites_->ForkForReload(staged_world_->InternalWorld());
      if (!staged_sprites_)
        return false;
    }
#endif
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
#if defined(NEXORA_HAS_2D)
    if (sprites_ && (!sprites_->RebindForReload(retired_world_->InternalWorld()) ||
                     !staged_sprites_ || !staged_sprites_->RebindForReload(world_.InternalWorld())))
      std::terminate();
#endif
    retired = &retired_world_->InternalWorld();
  }
  struct Presentation final : runtime::GameScenePresentation {
    PackagedPlayContext &owner;
    std::shared_ptr<sceneview::StaticView> palette;
#if defined(NEXORA_HAS_2D)
    std::unique_ptr<twod::GameSpriteSession::Prepared> sprites;
#endif
    Presentation(PackagedPlayContext &context, std::shared_ptr<sceneview::StaticView> prepared)
        : owner(context), palette(std::move(prepared)) {}
    void Commit() noexcept override {
#if defined(NEXORA_HAS_2D)
      if (sprites)
        sprites->Commit();
#endif
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
  std::shared_ptr<sceneview::StaticView> staged_palette_;
  std::uint64_t commits_{};
#if defined(NEXORA_HAS_2D)
  std::optional<twod::GameSpriteAssets> sprite_assets_;
  std::unique_ptr<twod::GameSpriteSession> sprites_, staged_sprites_;
  Frame *reload_frame_{};
  const CompositeDraw *reload_draw_{};
  Nexora::Presentation::SceneViewport reload_viewport_{};
  bool reload_retain_geometry_{};
#endif
};
} // namespace nexora::editor::preview
