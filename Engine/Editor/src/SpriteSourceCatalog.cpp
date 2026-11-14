#include "Nexora/Editor/SpriteSourceCatalog.h"
#include "Nexora/Editor/ProjectContent.h"

namespace nexora::editor {
namespace {
SpriteSourceContentVersion ContentVersion(const ContentBrowserModel &content) {
  return {content.ProjectGeneration(), content.Revision()};
}
} // namespace

bool SpriteSourceCatalog::PublishContent(const ContentBrowserModel &content, std::string *error) {
  const auto fail = [&](const char *message) {
    if (error)
      *error = message;
    return false;
  };
  if (!content.ProjectGeneration())
    return fail("Sprite source catalog requires a nonzero project generation.");
  // Imported immutable payloads cannot be fabricated by public constructors. Avoid decoding or
  // fully validating an Atlas again for every source; build one stable-region index per Atlas.
  struct AtlasEntry final {
    std::shared_ptr<const twod::Atlas> atlas;
    std::unordered_map<std::uint32_t, const twod::Region *> regions;
  };
  std::unordered_map<runtime::AssetUuid, AtlasEntry, runtime::AssetUuidHash> atlases;
  decltype(sources_) candidate;
  std::size_t bytes{}, count{};
  for (const auto &item : content.Items()) {
    if (!item.twod || (!item.twod->Atlas() && !item.twod->SpriteSource()))
      continue;
    const auto &payload = *item.twod;
    if (++count > kMaximumWorkspaceTwoDAssets || item.id == runtime::AssetUuid{} ||
        item.id != payload.Id() || item.type != payload.Type() ||
        payload.RetainedBytes() > kMaximumWorkspaceTwoDBytes - bytes || atlases.contains(item.id) ||
        candidate.contains(item.id))
      return fail(
          "Sprite source catalog rejected a typed identity, duplicate or workspace budget.");
    bytes += payload.RetainedBytes();
    if (auto atlas = payload.Atlas()) {
      AtlasEntry entry{atlas, {}};
      entry.regions.reserve(atlas->regions.size());
      for (const auto &region : atlas->regions)
        entry.regions.emplace(region.id, &region);
      atlases.emplace(item.id, std::move(entry));
    } else {
      candidate.emplace(item.id, SpriteSourceSnapshot{payload.SpriteSource(), {}});
    }
  }
  for (auto &[id, snapshot] : candidate) {
    static_cast<void>(id);
    const auto &source = *snapshot.source;
    const auto atlas = atlases.find(source.atlas);
    if (atlas == atlases.end())
      return fail("A Sprite source dependency is not an imported Atlas.");
    const auto region = atlas->second.regions.find(source.region);
    if (region == atlas->second.regions.end() || region->second->width != source.trimWidth ||
        region->second->height != source.trimHeight)
      return fail("A Sprite source region is missing or its dimensions differ from the trim.");
    snapshot.atlas = atlas->second.atlas;
  }
  sources_.swap(candidate);
  version_ = ContentVersion(content);
  if (error)
    error->clear();
  return true;
}

void SpriteSourceCatalog::Clear() noexcept {
  sources_.clear();
  version_ = {};
}

std::optional<SpriteSourceSnapshot>
SpriteSourceCatalog::ResolveAsset(runtime::AssetUuid asset,
                                  SpriteSourceContentVersion version) const {
  if (!version.project_generation || version != version_)
    return std::nullopt;
  const auto found = sources_.find(asset);
  return found == sources_.end() ? std::nullopt : std::optional{found->second};
}

OpaqueComponent SpriteSourceReference(runtime::AssetUuid asset) {
  OpaqueComponent component{
      kSpriteSourceReferenceType, std::string(kSpriteSourceReferenceName), {1}};
  for (const auto word : {asset.high, asset.low})
    for (unsigned byte = 0; byte < 8; ++byte)
      component.data.push_back(static_cast<std::uint8_t>(word >> (byte * 8)));
  return component;
}

std::optional<runtime::AssetUuid> ReadSpriteSourceReference(const OpaqueComponentInfo &info) {
  if (info.type != kSpriteSourceReferenceType || info.type_name != kSpriteSourceReferenceName ||
      info.byte_count != 17 || info.preview.size() != 17 || info.preview[0] != 1)
    return std::nullopt;
  runtime::AssetUuid asset;
  for (unsigned byte = 0; byte < 8; ++byte) {
    asset.high |= static_cast<std::uint64_t>(info.preview[1 + byte]) << (byte * 8);
    asset.low |= static_cast<std::uint64_t>(info.preview[9 + byte]) << (byte * 8);
  }
  return asset == runtime::AssetUuid{} ? std::nullopt : std::optional{asset};
}

std::optional<runtime::AssetUuid> ReadSpriteSourceReference(const SceneDocument &scene,
                                                            SceneDocument::NodeKey key) {
  const auto components = scene.InspectOpaqueComponents(key);
  if (!components)
    return std::nullopt;
  for (const auto &component : *components)
    if (component.type == kSpriteSourceReferenceType)
      return ReadSpriteSourceReference(component);
  return std::nullopt;
}

namespace {
bool ValidateTargets(const SceneDocument &scene, std::span<const SceneDocument::NodeKey> keys,
                     SpriteSourceContentVersion version, const ProjectContentSession &content,
                     bool editable) {
  const auto selected = scene.Selection();
  if (!editable || !content.Writable() || keys.empty() || selected.size() != keys.size() ||
      keys.size() > UnknownComponentStore::kMaximumComponents || !version.project_generation ||
      version != ContentVersion(content.Browser()))
    return false;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const auto key = keys[index];
    if (selected[index] != key.id)
      return false;
    const auto components = scene.InspectOpaqueComponents(key);
    if (!components)
      return false;
    for (const auto &component : *components)
      if ((component.type == kSpriteSourceReferenceType ||
           component.type_name == kSpriteSourceReferenceName) &&
          !ReadSpriteSourceReference(component))
        return false;
  }
  return true;
}
} // namespace

bool AssignSpriteSources(SceneDocument &scene, std::span<const SceneDocument::NodeKey> keys,
                         runtime::AssetUuid asset, SpriteSourceContentVersion version,
                         const ProjectContentSession &content, const SpriteSourceCatalog &catalog,
                         bool editable) {
  const auto resolved = catalog.ResolveAsset(asset, version);
  const auto *item = content.Browser().Find(asset);
  const auto *atlas = resolved ? content.Browser().Find(resolved->source->atlas) : nullptr;
  if (!resolved || !item || !item->twod || item->twod->SpriteSource() != resolved->source ||
      !atlas || !atlas->twod || atlas->twod->Atlas() != resolved->atlas ||
      !ValidateTargets(scene, keys, version, content, editable))
    return false;
  return scene.SetOpaqueComponents(keys, SpriteSourceReference(asset));
}

bool ClearSpriteSources(SceneDocument &scene, std::span<const SceneDocument::NodeKey> keys,
                        SpriteSourceContentVersion version, const ProjectContentSession &content,
                        bool editable) {
  return ValidateTargets(scene, keys, version, content, editable) &&
         scene.RemoveOpaqueComponents(keys, kSpriteSourceReferenceType);
}
} // namespace nexora::editor
