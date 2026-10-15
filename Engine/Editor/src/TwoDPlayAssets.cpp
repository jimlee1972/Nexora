#include "Nexora/Editor/TwoDPlayAssets.h"
#include <unordered_set>

namespace nexora::editor {
namespace {
struct GenerationPin final {
  runtime::AssetGenerationStore &store;
  std::uint64_t generation;
  ~GenerationPin() { static_cast<void>(store.Unpin(generation)); }
};
struct AssetReference final {
  runtime::AssetGenerationStore &store;
  runtime::AssetUuid asset;
  std::uint64_t generation;
  ~AssetReference() { store.Release(asset, generation); }
};
} // namespace

std::shared_ptr<const TwoDPlayAssets>
TwoDPlayAssets::Capture(runtime::AssetGenerationStore &store, std::uint64_t generation,
                        std::span<const runtime::AssetUuid> roots, std::string *error,
                        TwoDPlayAssetLimits limits) {
  const auto fail = [&](const char *message) -> std::shared_ptr<const TwoDPlayAssets> {
    if (error)
      *error = message;
    return {};
  };
  if (!generation || !limits.assets || limits.assets > 4096 || !limits.payload_bytes ||
      limits.payload_bytes > 64 * 1024 * 1024 || roots.size() > limits.assets)
    return fail("Invalid 2D Play generation or capture budget.");
  if (!store.Pin(generation))
    return fail("The selected cooked generation is unavailable.");
  const GenerationPin pin{store, generation};
  auto result = std::shared_ptr<TwoDPlayAssets>(new TwoDPlayAssets);
  result->generation_ = generation;
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> seen;
  std::vector<runtime::AssetUuid> pending;
  for (const auto id : roots) {
    if (id == runtime::AssetUuid{} || !seen.insert(id).second)
      return fail("2D Play roots contain a zero or duplicate UUID.");
    pending.push_back(id);
  }
  for (std::size_t index = 0; index < pending.size(); ++index) {
    const auto id = pending[index];
    const auto *blob = store.Load(id, generation);
    if (!blob)
      return fail("A 2D Play asset or dependency is unavailable in the selected generation.");
    const AssetReference reference{store, id, generation};
    if (blob->id != id || blob->schema_version != runtime::RuntimeBlob::kSchemaVersion ||
        blob->payload.size() > limits.payload_bytes - result->payload_bytes_)
      return fail("A 2D Play asset has an invalid envelope or exceeds the payload budget.");
    if (blob->type == "sprite_atlas") {
      if (!blob->dependencies.empty())
        return fail("A cooked Atlas must have no dependencies.");
      auto atlas = twod::DecodeAtlas(blob->payload);
      if (!atlas || atlas->id != id)
        return fail("A cooked Atlas is invalid or has a different UUID.");
      result->atlases_.emplace(id, std::make_shared<const twod::Atlas>(std::move(*atlas)));
    } else if (blob->type == "sprite_clip") {
      auto clip = twod::DecodeClip(blob->payload);
      if (!clip || clip->id != id || blob->dependencies.size() != 1 ||
          blob->dependencies.front() != clip->atlas)
        return fail("A cooked Clip is invalid or has inconsistent UUID dependencies.");
      if (seen.insert(clip->atlas).second) {
        if (pending.size() == limits.assets)
          return fail("The 2D Play dependency closure exceeds the asset budget.");
        pending.push_back(clip->atlas);
      }
      result->clips_.emplace(id, std::make_shared<const twod::Clip>(std::move(*clip)));
    } else {
      return fail("The cooked asset is not a supported 2D Atlas or Clip.");
    }
    result->payload_bytes_ += blob->payload.size();
  }
  for (const auto &[id, clip] : result->clips_) {
    static_cast<void>(id);
    const auto atlas = result->Atlas(clip->atlas);
    if (!atlas)
      return fail("A Clip dependency is not an Atlas.");
    for (const auto &frame : clip->frames)
      if (!atlas->Find(frame.region))
        return fail("A Clip frame references an unavailable Atlas region.");
  }
  if (error)
    error->clear();
  return result;
}

std::shared_ptr<const twod::Atlas> TwoDPlayAssets::Atlas(runtime::AssetUuid id) const {
  const auto found = atlases_.find(id);
  return found == atlases_.end() ? nullptr : found->second;
}

std::shared_ptr<const twod::Clip> TwoDPlayAssets::Clip(runtime::AssetUuid id) const {
  const auto found = clips_.find(id);
  return found == clips_.end() ? nullptr : found->second;
}
} // namespace nexora::editor
