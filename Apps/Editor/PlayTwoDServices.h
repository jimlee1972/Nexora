#pragma once

#include "Nexora/Runtime/Runtime.h"
#if defined(NEXORA_HAS_2D)
#include "Nexora/Editor/ProjectTwoDPlayAssets.h"
#include "Nexora/TwoD/GameplayBridge.h"
#endif

namespace nexora::editor::preview {
// Owning preparation; the caller rechecks Matches immediately before loading the module.
struct PlayTwoDBinding final {
  runtime::Id scene{};
#if defined(NEXORA_HAS_2D)
  std::shared_ptr<const ProjectTwoDPlayAssets> assets;
#endif
};

#if defined(NEXORA_HAS_2D)
inline std::optional<PlayTwoDBinding> CapturePlayTwoD(const ProjectWorkspace &workspace,
                                                      const ProjectContentSession &content,
                                                      const SceneDocument &document,
                                                      bool authorized, std::string &error) try {
  error.clear();
  std::vector<runtime::AssetUuid> roots;
  for (const auto &item : content.Browser().Items()) {
    if (!item.twod && item.type != ".nx2atlas" && item.type != ".nx2clip")
      continue;
    if (roots.size() == 4096) {
      error = "Play 2D asset selection exceeds 4096 sources.";
      return {};
    }
    roots.push_back(item.id);
  }
  if (roots.empty())
    return PlayTwoDBinding{};
  const auto source = document.CaptureRuntimeScene();
  if (!source) {
    error = "Play 2D scene capture is unavailable.";
    return {};
  }
  const auto generation = content.Browser().ProjectGeneration();
  auto assets = ProjectTwoDPlayAssets::Capture(workspace, content, generation, generation, roots,
                                               authorized, &error);
  if (!assets)
    return {};
  return PlayTwoDBinding{source->scene, std::move(assets)};
} catch (...) {
  error = "Play 2D asset preparation failed.";
  return {};
}

// Borrows the actual Play clone. No second World and no authoring Sprite metadata are created.
// Destroy after module callbacks and before the borrowed World. Calls serialize on its owner
// thread.
class PlayTwoDServices final {
public:
  static std::unique_ptr<PlayTwoDServices> Bind(runtime::World &world,
                                                const PlayTwoDBinding &binding) {
    const auto *scene = world.FindScene(binding.scene);
    if (world.Kind() != runtime::WorldKind::Play || !scene ||
        scene->state != runtime::SceneState::Active || !binding.assets || !binding.assets->Assets())
      return {};
    return std::unique_ptr<PlayTwoDServices>(new PlayTwoDServices(world, binding));
  }
  int32_t GetApi(NexoraTwoDApiV1 *output) noexcept { return bridge_.GetApi(output); }
  void ForgetEntity(runtime::Id entity) { bridge_.ForgetEntity(entity); }
  [[nodiscard]] const twod::SpriteScene &Scene() const noexcept { return scene_; }

private:
  PlayTwoDServices(runtime::World &world, const PlayTwoDBinding &binding)
      : assets_(binding.assets->Assets()), scene_(world, binding.scene), pool_(4096),
        bridge_(
            world, scene_, [assets = assets_](auto id) { return assets->Clip(id); }, nullptr,
            &pool_) {}
  std::shared_ptr<const TwoDPlayAssets> assets_;
  twod::SpriteScene scene_;
  twod::InstancePool pool_;
  twod::GameplayBridge bridge_;
};
#endif
} // namespace nexora::editor::preview
