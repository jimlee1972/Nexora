#pragma once

#include "Nexora/Runtime/Runtime.h"
#if defined(NEXORA_HAS_2D)
#include "Nexora/Editor/ProjectTwoDPlayAssets.h"
#include "Nexora/TwoD/GameplayBridge.h"
#include <algorithm>
#include <functional>
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
  [[nodiscard]] std::shared_ptr<const TwoDPlayAssets> Assets() const noexcept { return assets_; }
  using TextureResolver = std::function<std::uint64_t(const twod::Atlas &)>;
  // An owning, globally sorted CPU snapshot. Texture registration is host-owned and serialized.
  [[nodiscard]] std::optional<twod::SpriteFrame>
  ExtractFrame(std::uint32_t width, std::uint32_t height, const TextureResolver &textures) try {
    if (!width || !height || width > 16384 || height > 16384 || !textures)
      return {};
    const auto camera = bridge_.RenderCamera(width, height);
    if (scene_.Sprites().size() + pool_.Size() > 4096)
      return {};
    const auto &world = scene_.WorldOwner();
    const auto *scene = world.FindScene(scene_.SceneId());
    if (!scene || scene->state != runtime::SceneState::Active || scene->entities.size() > 100000)
      return {};
    auto poses = world.SceneWorldPoses(scene_.SceneId());
    if (!poses || poses->size() > 100000)
      return {};
    std::sort(poses->begin(), poses->end(),
              [](const auto &a, const auto &b) { return a.id < b.id; });
    std::vector<twod::SpriteInstance> instances;
    instances.reserve(scene_.Sprites().size() + pool_.Size());
    for (const auto &pose : *poses)
      if (const auto *sprite = scene_.Find(pose.id))
        instances.push_back({instances.size() + 1, *sprite, pose.matrix});
    pool_.ForEach([&](auto &instance) {
      instances.push_back(
          {instances.size() + 1, instance.sprite, runtime::ToMatrix(instance.transform)});
    });
    return twod::ExtractSprites(
        instances, camera,
        [assets = assets_, &textures](twod::AssetId id) -> std::optional<twod::AtlasBinding> {
          const auto atlas = assets->Atlas(id);
          if (!atlas)
            return {};
          const auto texture = textures(*atlas);
          return texture ? std::optional{twod::AtlasBinding{atlas, texture}} : std::nullopt;
        });
  } catch (...) {
    return {};
  }

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
