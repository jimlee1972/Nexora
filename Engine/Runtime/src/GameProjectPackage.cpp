#include "Nexora/Runtime/GameProjectPackage.h"
#include "ProjectPackageIO.h"

#include <bit>
#include <map>
#include <set>

namespace nexora::runtime {
namespace {
using namespace package_detail;
constexpr std::array<std::byte, 8> kGameMagic{std::byte{'N'}, std::byte{'X'}, std::byte{'G'},
                                              std::byte{'A'}, std::byte{'M'}, std::byte{'E'},
                                              std::byte{0},   std::byte{1}};
bool CatalogPath(std::string_view path) {
  if (path.empty() || path.size() > kMaximumGameScenePathBytes || !foundation::IsValidUtf8(path) ||
      !path.ends_with(".scene") || path.find_first_of("\\:<>\"|?*") != path.npos ||
      std::ranges::any_of(path, [](unsigned char c) { return c < 32 || c == 127; }))
    return false;
  std::size_t first = 0;
  do {
    const auto last = path.find('/', first);
    const auto part = path.substr(first, last == path.npos ? path.size() - first : last - first);
    if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
      return false;
    if (last == path.npos)
      return true;
    first = last + 1;
  } while (first < path.size());
  return false;
}
std::string Fold(std::string text) {
  for (auto &c : text)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c + ('a' - 'A'));
  return text;
}
} // namespace

class GameProjectResolver final {
public:
  static std::optional<LoadedGameProject> Load(const GameProjectPackage &package,
                                               std::string *error) {
    if (error)
      error->clear();
    if (std::endian::native != std::endian::little || !Nonzero(package.project) ||
        !Nonzero(package.start_scene) || package.scenes.empty() ||
        package.scenes.size() > kMaximumGameScenes || package.assets.empty() ||
        package.assets.size() > kMaximumProjectPackageAssets)
      return Fail<LoadedGameProject>(error,
                                     "Invalid game package identities/counts or byte order.");
    LoadedGameProject result;
    result.project_ = package.project;
    result.start_scene_ = package.start_scene;
    result.asset_count_ = package.assets.size();
    std::unordered_map<AssetUuid, const RuntimeBlob *, AssetUuidHash> blobs;
    std::unordered_set<AssetUuid, AssetUuidHash> scene_ids, used;
    std::unordered_set<std::string> paths;
    std::unordered_map<std::uint64_t, AssetUuid> mesh_resources;
    std::size_t bytes = 68, geometry = 0, snapshots = 0, entity_count = 0;
    for (const auto &entry : package.scenes) {
      if (!Nonzero(entry.id) || !CatalogPath(entry.path) || !scene_ids.insert(entry.id).second ||
          !paths.insert(Fold(entry.path)).second)
        return Fail<LoadedGameProject>(error,
                                       "Invalid/duplicate game scene catalog identity or path.");
      bytes += 20 + entry.path.size();
    }
    if (!scene_ids.contains(package.start_scene))
      return Fail<LoadedGameProject>(error, "Game startup scene is not in the enabled catalog.");
    for (const auto &blob : package.assets) {
      if (!Nonzero(blob.id) || !blobs.emplace(blob.id, &blob).second || blob.schema_version != 1 ||
          blob.type.size() > 64 || blob.content_hash.size() != 16 ||
          blob.dependencies.size() > kMaximumProjectPackageAssets ||
          blob.payload.size() > kMaximumProjectBlobBytes - 128)
        return Fail<LoadedGameProject>(error, "Invalid/duplicate game asset identity or framing.");
      const auto size = 60 + blob.type.size() + blob.dependencies.size() * 16 + blob.payload.size();
      if (size > kMaximumProjectBlobBytes || size + 8 > kMaximumProjectPackageBytes - bytes)
        return Fail<LoadedGameProject>(error, "Game package exceeds its byte budget.");
      bytes += size + 8;
      if (blob.type == kCookedMeshType) {
        const auto decoded = DecodeCookedMesh(blob.payload);
        if (!decoded || !mesh_resources.emplace(MeshResourceId(blob.id), blob.id).second)
          return Fail<LoadedGameProject>(error,
                                         "Invalid game mesh or global resource ID collision.");
        const auto count = decoded.Value().vertices.size() * sizeof(CookedMeshVertex) +
                           decoded.Value().indices.size() * sizeof(std::uint16_t);
        if (count > kMaximumProjectGeometryBytes - geometry)
          return Fail<LoadedGameProject>(error, "Game geometry exceeds 128 MiB.");
        geometry += count;
      } else if (blob.type == kCookedSceneType) {
        if (!scene_ids.contains(blob.id))
          return Fail<LoadedGameProject>(error, "Unlisted scene in game package.");
      } else if (blob.type != kCookedScalarPbrType)
        return Fail<LoadedGameProject>(error, "Unsupported game asset type.");
    }
    std::unordered_map<AssetUuid, std::shared_ptr<const CookedMesh>, AssetUuidHash> meshes;
    std::unordered_map<AssetUuid, std::shared_ptr<const CookedScalarPbr>, AssetUuidHash> materials;
    for (const auto &entry : package.scenes) {
      const auto found = blobs.find(entry.id);
      if (found == blobs.end() || found->second->type != kCookedSceneType)
        return Fail<LoadedGameProject>(error,
                                       "Catalog scene asset is missing or has the wrong type.");
      StaticProjectPackage single{package.project, entry.id, {*found->second}};
      for (const auto id : found->second->dependencies) {
        const auto dependency = blobs.find(id);
        if (dependency == blobs.end() || dependency->second->type == kCookedSceneType)
          return Fail<LoadedGameProject>(
              error, "Missing dependency or unsupported cross-scene reference.");
        single.assets.push_back(*dependency->second);
      }
      auto loaded = LoadStaticProjectPackage(single, error);
      if (!loaded)
        return std::nullopt;
      if (loaded->InactiveComponentCount())
        return Fail<LoadedGameProject>(
            error, "Required opaque component behavior is unsupported in a game package.");
      const auto text_bytes = loaded->SceneData().world_snapshot.size();
      const auto entities = loaded->WorldView().FindScene(loaded->SceneId())->entities.size();
      if (text_bytes > kCookedSceneMaximumSnapshotBytes - snapshots ||
          entities > kCookedSceneMaximumEntities - entity_count)
        return Fail<LoadedGameProject>(error, "Game aggregate snapshot/entity budget exceeded.");
      snapshots += text_bytes;
      entity_count += entities;
      std::unordered_map<Id, const SceneAssetBinding *> bindings;
      for (const auto &binding : loaded->scene_data_.bindings)
        bindings.emplace(binding.entity, &binding);
      for (auto &item : loaded->items_) {
        const auto *binding = bindings.at(item.entity);
        const auto [mesh, inserted] = meshes.emplace(binding->mesh, item.mesh);
        static_cast<void>(inserted);
        item.mesh = mesh->second;
        loaded->meshes_[binding->mesh_resource] = item.mesh;
        if (binding->material) {
          const auto [material, added] = materials.emplace(*binding->material, item.material);
          static_cast<void>(added);
          item.material = material->second;
        }
      }
      used.insert(entry.id);
      used.insert(found->second->dependencies.begin(), found->second->dependencies.end());
      result.scenes_.emplace(entry.id,
                             std::make_shared<const LoadedStaticProject>(std::move(*loaded)));
    }
    if (used.size() != blobs.size())
      return Fail<LoadedGameProject>(error, "Unreferenced extra assets in game package.");
    result.catalog_ = package.scenes;
    std::ranges::sort(result.catalog_,
                      [](const auto &a, const auto &b) { return Less(a.id, b.id); });
    return result;
  }
};

std::shared_ptr<const LoadedStaticProject> LoadedGameProject::ResolveScene(AssetUuid id) const {
  const auto found = scenes_.find(id);
  return found == scenes_.end() ? nullptr : found->second;
}
std::shared_ptr<const LoadedStaticProject>
LoadedGameProject::ResolveScene(std::string_view path) const {
  const auto found = std::ranges::find(catalog_, path, &GameSceneEntry::path);
  return found == catalog_.end() ? nullptr : ResolveScene(found->id);
}
std::optional<LoadedGameProject> LoadGameProjectPackage(const GameProjectPackage &package,
                                                        std::string *error) {
  return GameProjectResolver::Load(package, error);
}
std::optional<ByteBuffer> EncodeGameProjectPackage(const GameProjectPackage &package,
                                                   std::string *error) {
  const auto loaded = LoadGameProjectPackage(package, error);
  if (!loaded)
    return std::nullopt;
  ByteBuffer output(kGameMagic.begin(), kGameMagic.end());
  Append(output, 1, 4); // Schema.
  Append(output, 2, 4); // Packaged scene catalog capability; never StaticView.
  for (const auto value : {package.project.high, package.project.low, package.start_scene.high,
                           package.start_scene.low})
    Append(output, value, 8);
  Append(output, package.scenes.size(), 4);
  for (const auto &entry : loaded->Scenes()) {
    Append(output, entry.id.high, 8);
    Append(output, entry.id.low, 8);
    Append(output, entry.path.size(), 4);
    for (const unsigned char c : entry.path)
      output.push_back(static_cast<std::byte>(c));
  }
  std::vector<const RuntimeBlob *> sorted;
  for (const auto &blob : package.assets)
    sorted.push_back(&blob);
  std::ranges::sort(sorted, [](const auto *a, const auto *b) { return Less(a->id, b->id); });
  Append(output, sorted.size(), 4);
  for (const auto *blob : sorted) {
    auto bytes = AssetCooker::Serialize(*blob);
    auto dependencies = blob->dependencies;
    std::ranges::sort(dependencies, Less);
    std::size_t offset = 32 + blob->type.size();
    for (const auto id : dependencies)
      for (const auto word : {id.high, id.low})
        for (unsigned i = 0; i < 8; ++i)
          bytes[offset++] = static_cast<std::byte>(word >> (8 * i));
    Append(output, bytes.size(), 8);
    output.insert(output.end(), bytes.begin(), bytes.end());
  }
  Append(output, Hash(output), 8);
  return output;
}
std::optional<LoadedGameProject> DecodeGameProjectPackage(std::span<const std::byte> bytes,
                                                          std::string *error) {
  if (error)
    error->clear();
  if (std::endian::native != std::endian::little || bytes.size() < 64 ||
      bytes.size() > kMaximumProjectPackageBytes || !std::ranges::equal(bytes.first(8), kGameMagic))
    return Fail<LoadedGameProject>(error, "Invalid game package header/budget.");
  Reader checksum{bytes.last(8)};
  std::uint64_t expected{};
  if (!checksum.Number(expected, 8) || Hash(bytes.first(bytes.size() - 8)) != expected)
    return Fail<LoadedGameProject>(error, "Game package checksum mismatch.");
  Reader reader{bytes.first(bytes.size() - 8), 8};
  GameProjectPackage package;
  std::uint64_t schema{}, capability{}, count{};
  if (!reader.Number(schema, 4) || schema != 1 || !reader.Number(capability, 4) ||
      capability != 2 || !reader.Number(package.project.high, 8) ||
      !reader.Number(package.project.low, 8) || !reader.Number(package.start_scene.high, 8) ||
      !reader.Number(package.start_scene.low, 8) || !reader.Number(count, 4) || !count ||
      count > kMaximumGameScenes)
    return Fail<LoadedGameProject>(error, "Invalid game catalog count/schema.");
  AssetUuid previous;
  for (std::uint64_t i = 0; i < count; ++i) {
    GameSceneEntry entry;
    std::uint64_t size{};
    if (!reader.Number(entry.id.high, 8) || !reader.Number(entry.id.low, 8) ||
        (i && !Less(previous, entry.id)) || !reader.Number(size, 4) ||
        size > kMaximumGameScenePathBytes || size > reader.bytes.size() - reader.offset)
      return Fail<LoadedGameProject>(error, "Invalid game scene record/order.");
    entry.path.assign(reinterpret_cast<const char *>(reader.bytes.data() + reader.offset),
                      static_cast<std::size_t>(size));
    reader.offset += static_cast<std::size_t>(size);
    previous = entry.id;
    package.scenes.push_back(std::move(entry));
  }
  if (!reader.Number(count, 4) || !count || count > kMaximumProjectPackageAssets ||
      count > (reader.bytes.size() - reader.offset) / 12)
    return Fail<LoadedGameProject>(error, "Invalid game asset count.");
  previous = {};
  for (std::uint64_t i = 0; i < count; ++i) {
    std::uint64_t size{};
    if (!reader.Number(size, 8) || size > reader.bytes.size() - reader.offset)
      return Fail<LoadedGameProject>(error, "Truncated game asset.");
    const auto view = reader.bytes.subspan(reader.offset, static_cast<std::size_t>(size));
    if (!PreflightBlob(view))
      return Fail<LoadedGameProject>(error, "Invalid game asset framing.");
    auto blob = AssetCooker::Deserialize(view);
    if (!blob || (i && !Less(previous, blob->id)) ||
        !std::ranges::is_sorted(blob->dependencies, Less))
      return Fail<LoadedGameProject>(error, "Game asset integrity/order failed.");
    previous = blob->id;
    package.assets.push_back(std::move(*blob));
    reader.offset += static_cast<std::size_t>(size);
  }
  if (reader.offset != reader.bytes.size())
    return Fail<LoadedGameProject>(error, "Unexpected game package suffix.");
  return LoadGameProjectPackage(package, error);
}
std::optional<LoadedGameProject> ReadGameProjectPackage(const std::filesystem::path &path,
                                                        std::string *error) {
  const auto bytes = ReadPackageFile(path, error);
  return bytes ? DecodeGameProjectPackage(*bytes, error) : std::nullopt;
}
} // namespace nexora::runtime
