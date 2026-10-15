#include "Nexora/Editor/TwoDPlayContext.h"
#include <algorithm>
#include <limits>

namespace nexora::editor {
TwoDPlayContext::TwoDPlayContext(std::size_t capacity)
    : thread_(std::this_thread::get_id()), pool_(capacity) {}
TwoDPlayContext::~TwoDPlayContext() = default;

std::unique_ptr<TwoDPlayContext> TwoDPlayContext::Create(const TwoDDocument &document,
                                                         std::size_t capacity) noexcept {
  if (!capacity || capacity > kMaximumPoolCapacity)
    return {};
  try {
    // Use the document's validated canonical snapshot boundary; no authoring borrow is retained.
    const auto bytes = document.Save();
    if (bytes.empty() || bytes.size() > 64 * 1024 * 1024)
      return {};
    auto result = std::unique_ptr<TwoDPlayContext>(new TwoDPlayContext(capacity));
    result->scene_ = document.StartPlay(result->world_);
    if (!result->scene_ || !result->world_.Activate(result->scene_->SceneId()))
      return {};
    if (twod::ValidateAtlas(document.Atlas()))
      result->atlas_ = std::make_shared<const twod::Atlas>(document.Atlas());
    if (twod::ValidateClip(document.Clip()))
      result->clip_ = std::make_shared<const twod::Clip>(document.Clip());
    result->tiles_ = document.Tiles();
    result->bridge_ = std::make_unique<twod::GameplayBridge>(
        result->world_, *result->scene_,
        [clip = result->clip_](twod::AssetId id) {
          return clip && clip->id == id ? clip : nullptr;
        },
        &result->tiles_, &result->pool_);
    return result;
  } catch (...) {
    return {};
  }
}

int32_t TwoDPlayContext::GetApi(NexoraTwoDApiV1 *output) noexcept {
  return bridge_->GetApi(output);
}
void TwoDPlayContext::ForgetEntity(runtime::Id id) { bridge_->ForgetEntity(id); }

std::optional<twod::SpriteFrame> TwoDPlayContext::ExtractFrame(std::uint32_t width,
                                                               std::uint32_t height,
                                                               std::uint64_t texture) noexcept {
  if (thread_ != std::this_thread::get_id() || !width || !height || !texture ||
      texture >= std::numeric_limits<std::uint64_t>::max() - 1)
    return std::nullopt;
  try {
    const auto camera = bridge_->RenderCamera(width, height);
    auto poses = world_.SceneWorldPoses(scene_->SceneId());
    // A new document may have no Atlas yet. Its empty Tilemap contributes no inputs.
    auto tiles =
        tiles_.Tiles().empty() ? std::optional{twod::TilemapInstances{}} : tiles_.Collect(camera);
    if (!poses || !tiles)
      return std::nullopt;
    // World poses use scene storage order. Preserve SpriteScene's entity-ID tie order before
    // assigning distinct draw identities to scene, pool and tile namespaces.
    std::sort(poses->begin(), poses->end(),
              [](const auto &a, const auto &b) { return a.id < b.id; });
    std::vector<twod::SpriteInstance> instances;
    bool overflow{};
    const auto append = [&](twod::Sprite sprite, const runtime::TransformMatrix &matrix) {
      if (instances.size() == kMaximumFrameInstances) {
        overflow = true;
        return;
      }
      // Draw-only identities keep scene, pool and tile namespaces distinct for stable ties.
      instances.push_back({instances.size() + 1, std::move(sprite), matrix});
    };
    for (const auto &pose : *poses)
      if (const auto *sprite = scene_->Find(pose.id))
        append(*sprite, pose.matrix);
    pool_.ForEach(
        [&](auto &instance) { append(instance.sprite, runtime::ToMatrix(instance.transform)); });
    for (const auto &instance : tiles->instances)
      append(instance.sprite, instance.matrix);
    if (overflow)
      return std::nullopt;
    return twod::ExtractSprites(
        instances, camera,
        [atlas = atlas_, texture](twod::AssetId id) -> std::optional<twod::AtlasBinding> {
          if (!atlas || atlas->id != id)
            return std::nullopt;
          return twod::AtlasBinding{atlas, texture};
        });
  } catch (...) {
    return std::nullopt;
  }
}
} // namespace nexora::editor
