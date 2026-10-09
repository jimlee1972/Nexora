#include "Nexora/Runtime/ProjectPackage.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <unordered_set>

#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace nexora::runtime {
namespace {
constexpr std::array<std::byte, 8> kMagic{std::byte{'N'}, std::byte{'X'}, std::byte{'P'},
                                          std::byte{'R'}, std::byte{'O'}, std::byte{'J'},
                                          std::byte{0},   std::byte{1}};
constexpr std::uint64_t kMaterialReferenceType = 0x45444d41544c0001ULL;
constexpr std::string_view kMaterialReferenceName = "editor.material.asset";
bool Nonzero(AssetUuid id) { return id.high != 0 || id.low != 0; }
bool Less(AssetUuid left, AssetUuid right) {
  return left.high < right.high || (left.high == right.high && left.low < right.low);
}
template <class T> std::optional<T> Fail(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
  return std::nullopt;
}
std::uint64_t Hash(std::span<const std::byte> bytes) {
  std::uint64_t value = 1469598103934665603ULL;
  for (const auto byte : bytes) {
    value ^= std::to_integer<std::uint8_t>(byte);
    value *= 1099511628211ULL;
  }
  return value;
}
void Append(ByteBuffer &bytes, std::uint64_t value, std::size_t width) {
  for (std::size_t index = 0; index < width; ++index)
    bytes.push_back(static_cast<std::byte>(value >> (8 * index)));
}
struct Reader {
  std::span<const std::byte> bytes;
  std::size_t offset{};
  bool Number(std::uint64_t &value, std::size_t width) {
    if (width > bytes.size() - offset)
      return false;
    value = 0;
    for (std::size_t index = 0; index < width; ++index)
      value |= std::uint64_t(std::to_integer<std::uint8_t>(bytes[offset++])) << (8 * index);
    return true;
  }
  bool Skip(std::uint64_t size) {
    if (size > bytes.size() - offset)
      return false;
    offset += static_cast<std::size_t>(size);
    return true;
  }
  bool String(std::size_t maximum) {
    std::uint64_t size{};
    return Number(size, 4) && size <= maximum && Skip(size);
  }
};
// NXAB uses native integer layout today. Reject foreign-endian hosts rather than claiming a
// cross-endian wire; preflight every count/length before the existing deserializer can allocate.
bool PreflightBlob(std::span<const std::byte> bytes) {
  if (bytes.size() > kMaximumProjectBlobBytes || bytes.size() < 4 ||
      std::memcmp(bytes.data(), "NXAB", 4) != 0)
    return false;
  Reader reader{bytes, 4};
  std::uint64_t version{}, ignored{}, dependencies{}, payload{};
  return reader.Number(version, 4) && version == 1 && reader.Number(ignored, 8) &&
         reader.Number(ignored, 8) && reader.String(64) && reader.Number(dependencies, 4) &&
         dependencies <= kMaximumProjectPackageAssets && reader.Skip(dependencies * 16) &&
         reader.String(16) && reader.Number(payload, 8) && payload == bytes.size() - reader.offset;
}
bool ExactDependencies(const RuntimeBlob &blob,
                       const std::unordered_set<AssetUuid, AssetUuidHash> &expected) {
  std::unordered_set<AssetUuid, AssetUuidHash> actual;
  for (const auto id : blob.dependencies)
    if (!Nonzero(id) || !actual.insert(id).second)
      return false;
  return actual == expected;
}
} // namespace

std::shared_ptr<const CookedMesh> LoadedStaticProject::ResolveMesh(std::uint64_t resource) const {
  const auto found = meshes_.find(resource);
  return found == meshes_.end() ? nullptr : found->second;
}

std::optional<LoadedStaticProject> LoadStaticProjectPackage(const StaticProjectPackage &package,
                                                            std::string *error) {
  if (error)
    error->clear();
  if constexpr (std::endian::native != std::endian::little)
    return Fail<LoadedStaticProject>(error, "NXAB static packages require a little-endian host");
  if (!Nonzero(package.project) || !Nonzero(package.scene) || package.assets.empty() ||
      package.assets.size() > kMaximumProjectPackageAssets)
    return Fail<LoadedStaticProject>(error, "invalid project identity or asset count");
  LoadedStaticProject result;
  result.project_ = package.project;
  result.scene_asset_ = package.scene;
  result.asset_count_ = package.assets.size();
  std::unordered_map<AssetUuid, const RuntimeBlob *, AssetUuidHash> blobs;
  std::unordered_map<AssetUuid, std::shared_ptr<const CookedMesh>, AssetUuidHash> meshes;
  std::unordered_map<AssetUuid, std::shared_ptr<const CookedScalarPbr>, AssetUuidHash> materials;
  std::size_t bytes = 64, geometry_bytes = 0;
  for (const auto &blob : package.assets) {
    if (!Nonzero(blob.id) || !blobs.emplace(blob.id, &blob).second || blob.type.size() > 64 ||
        blob.content_hash.size() != 16 || blob.dependencies.size() > kMaximumProjectPackageAssets ||
        blob.payload.size() > kMaximumProjectBlobBytes - 128 || blob.schema_version != 1)
      return Fail<LoadedStaticProject>(error, "invalid or duplicate runtime asset");
    const auto serialized_size =
        60 + blob.type.size() + blob.dependencies.size() * 16 + blob.payload.size();
    if (serialized_size > kMaximumProjectBlobBytes ||
        serialized_size + 8 > kMaximumProjectPackageBytes - bytes)
      return Fail<LoadedStaticProject>(error, "static package byte budget exceeded");
    bytes += serialized_size + 8;
    const auto serialized = AssetCooker::Serialize(blob);
    if (!PreflightBlob(serialized) || !AssetCooker::Deserialize(serialized))
      return Fail<LoadedStaticProject>(error, "runtime blob integrity failed");
    if (blob.type == kCookedMeshType) {
      const auto decoded = DecodeCookedMesh(blob.payload);
      if (!decoded || !blob.dependencies.empty())
        return Fail<LoadedStaticProject>(error, "unsupported cooked mesh");
      const auto size = decoded.Value().vertices.size() * sizeof(CookedMeshVertex) +
                        decoded.Value().indices.size() * sizeof(std::uint16_t);
      if (size > kMaximumProjectGeometryBytes - geometry_bytes)
        return Fail<LoadedStaticProject>(error, "project geometry budget exceeded");
      geometry_bytes += size;
      auto owned = std::make_shared<const CookedMesh>(decoded.Value());
      if (!result.meshes_.emplace(MeshResourceId(blob.id), owned).second)
        return Fail<LoadedStaticProject>(error, "mesh resource ID collision");
      meshes.emplace(blob.id, std::move(owned));
    } else if (blob.type == kCookedScalarPbrType) {
      const auto decoded = DecodeCookedScalarPbr(blob.payload);
      if (!decoded || !blob.dependencies.empty())
        return Fail<LoadedStaticProject>(error, "unsupported cooked scalar material");
      materials.emplace(blob.id, std::make_shared<const CookedScalarPbr>(decoded.Value()));
    } else if (blob.type == kCookedSceneType && blob.id == package.scene) {
      auto decoded = DecodeCookedScene(blob.payload);
      if (!decoded)
        return Fail<LoadedStaticProject>(error, "unsupported cooked scene");
      result.scene_data_ = std::move(decoded.Value());
    } else {
      return Fail<LoadedStaticProject>(error, "unsupported asset schema or extra scene");
    }
  }
  const auto scene_blob = blobs.find(package.scene);
  if (scene_blob == blobs.end() || scene_blob->second->type != kCookedSceneType)
    return Fail<LoadedStaticProject>(error, "entry scene asset is missing");
  const auto scene = result.world_.LoadSceneSnapshot(result.scene_data_.world_snapshot);
  if (!scene || !result.world_.Activate(*scene))
    return Fail<LoadedStaticProject>(error, "scene snapshot cannot activate");
  result.scene_ = *scene;
  const auto *world_scene = result.world_.FindScene(*scene);
  // Resolve every hierarchy matrix once in parent-before-child order. Calling WorldMatrix for
  // each binding would repeatedly walk ancestors and perform linear World entity lookups.
  std::unordered_map<Id, const Entity *> entities;
  std::unordered_map<Id, std::vector<Id>> children;
  std::unordered_map<Id, TransformMatrix> matrices;
  entities.reserve(world_scene->entities.size());
  children.reserve(world_scene->entities.size());
  matrices.reserve(world_scene->entities.size());
  std::vector<Id> pending;
  for (const auto &entity : world_scene->entities) {
    entities.emplace(entity.id, &entity);
    if (entity.parent)
      children[entity.parent].push_back(entity.id);
    else
      pending.push_back(entity.id);
  }
  for (std::size_t index = 0; index < pending.size(); ++index) {
    const auto *entity = entities.at(pending[index]);
    auto matrix = ToMatrix(entity->transform);
    if (entity->parent) {
      const auto parent = matrices.find(entity->parent);
      if (parent == matrices.end())
        return Fail<LoadedStaticProject>(error, "invalid static parent order");
      matrix = MultiplyMatrices(parent->second, matrix);
    }
    if (std::ranges::any_of(matrix, [](double value) { return !std::isfinite(value); }))
      return Fail<LoadedStaticProject>(error, "invalid static world matrix");
    matrices.emplace(entity->id, matrix);
    if (const auto descendants = children.find(entity->id); descendants != children.end())
      pending.insert(pending.end(), descendants->second.begin(), descendants->second.end());
  }
  if (matrices.size() != entities.size())
    return Fail<LoadedStaticProject>(error, "incomplete static hierarchy");
  std::unordered_set<AssetUuid, AssetUuidHash> used;
  std::unordered_map<Id, const SceneAssetBinding *> bindings;
  for (const auto &binding : result.scene_data_.bindings) {
    const auto found_entity = entities.find(binding.entity);
    const auto *entity = found_entity == entities.end() ? nullptr : found_entity->second;
    const auto mesh = meshes.find(binding.mesh);
    if (!entity || !entity->mesh_renderer || mesh == meshes.end() ||
        !bindings.emplace(binding.entity, &binding).second ||
        binding.mesh_resource != MeshResourceId(binding.mesh) ||
        entity->mesh_data.mesh != binding.mesh_resource)
      return Fail<LoadedStaticProject>(error, "missing or incompatible mesh binding");
    StaticRenderItem item;
    item.entity = entity->id;
    item.mesh = mesh->second;
    item.world_transform = matrices.at(entity->id);
    used.insert(binding.mesh);
    if (binding.material) {
      const auto material = materials.find(*binding.material);
      if (material == materials.end())
        return Fail<LoadedStaticProject>(error, "material binding is missing");
      item.material = material->second;
      used.insert(*binding.material);
    } else if (entity->mesh_data.material.shader != 0) {
      return Fail<LoadedStaticProject>(error, "legacy shader has no StaticView implementation");
    }
    result.items_.push_back(std::move(item));
  }
  for (const auto &entity : world_scene->entities) {
    if (entity.mesh_renderer && !bindings.contains(entity.id))
      return Fail<LoadedStaticProject>(error, "unresolved scene mesh renderer");
    if ((entity.camera &&
         (!std::isfinite(entity.camera_data.vertical_field_of_view) ||
          entity.camera_data.vertical_field_of_view <= 0 ||
          entity.camera_data.vertical_field_of_view >= 180 ||
          !std::isfinite(entity.camera_data.near_plane) ||
          !std::isfinite(entity.camera_data.far_plane) || entity.camera_data.near_plane <= 0 ||
          entity.camera_data.far_plane <= entity.camera_data.near_plane)) ||
        (entity.light &&
         (!std::isfinite(entity.light_data.intensity) || entity.light_data.intensity < 0)))
      return Fail<LoadedStaticProject>(error, "invalid static camera or light");
  }
  if (!ExactDependencies(*scene_blob->second, used) || used.size() + 1 != blobs.size())
    return Fail<LoadedStaticProject>(error,
                                     "scene dependency closure is incomplete or contains extras");
  for (const auto &opaque : result.scene_data_.opaque) {
    if (!entities.contains(opaque.entity))
      return Fail<LoadedStaticProject>(error, "opaque record entity is missing");
    if (opaque.type == kMaterialReferenceType || opaque.type_name == kMaterialReferenceName) {
      const auto binding = bindings.find(opaque.entity);
      if (opaque.type != kMaterialReferenceType || opaque.type_name != kMaterialReferenceName ||
          opaque.data.size() != 17 || opaque.data[0] != std::byte{1} || binding == bindings.end() ||
          !binding->second->material)
        return Fail<LoadedStaticProject>(error, "unsupported material reference metadata");
      Reader reference{opaque.data, 1};
      AssetUuid material;
      if (!reference.Number(material.high, 8) || !reference.Number(material.low, 8) ||
          material != *binding->second->material)
        return Fail<LoadedStaticProject>(error,
                                         "material reference metadata disagrees with binding");
    } else {
      ++result.inactive_components_;
    }
  }
  std::ranges::sort(result.items_, {}, &StaticRenderItem::entity);
  // Exercise the existing Runtime's real bundle integrity path, with owning cooked assets.
  const auto bundle = BundleBuilder::Build("static-project", 1, {}, package.assets);
  if (!bundle || !BundleBuilder::Verify(*bundle))
    return Fail<LoadedStaticProject>(error, "runtime bundle verification failed");
  return result;
}

std::optional<ByteBuffer> EncodeStaticProjectPackage(const StaticProjectPackage &package,
                                                     std::string *error) {
  if (!LoadStaticProjectPackage(package, error))
    return std::nullopt;
  std::vector<const RuntimeBlob *> sorted;
  for (const auto &blob : package.assets)
    sorted.push_back(&blob);
  std::ranges::sort(sorted,
                    [](const auto *left, const auto *right) { return Less(left->id, right->id); });
  ByteBuffer output(kMagic.begin(), kMagic.end());
  Append(output, 1, 4); // Schema.
  Append(output, 1, 4); // StaticView capability.
  Append(output, package.project.high, 8);
  Append(output, package.project.low, 8);
  Append(output, package.scene.high, 8);
  Append(output, package.scene.low, 8);
  Append(output, sorted.size(), 4);
  for (const auto *blob : sorted) {
    auto bytes = AssetCooker::Serialize(*blob);
    auto dependencies = blob->dependencies;
    std::ranges::sort(dependencies, Less);
    // Canonicalize only the bounded UUID dependency range in the existing schema-1 NXAB wire,
    // avoiding another copy of the potentially large payload. Its content hash covers payload.
    std::size_t dependency_offset = 32 + blob->type.size();
    for (const auto dependency : dependencies)
      for (const auto value : {dependency.high, dependency.low})
        for (std::size_t index = 0; index < 8; ++index)
          bytes[dependency_offset++] = static_cast<std::byte>(value >> (8 * index));
    Append(output, bytes.size(), 8);
    output.insert(output.end(), bytes.begin(), bytes.end());
  }
  Append(output, Hash(output), 8);
  return output;
}

std::optional<LoadedStaticProject> DecodeStaticProjectPackage(std::span<const std::byte> bytes,
                                                              std::string *error) {
  if (error)
    error->clear();
  if (std::endian::native != std::endian::little || bytes.size() < 60 ||
      bytes.size() > kMaximumProjectPackageBytes || !std::ranges::equal(bytes.first(8), kMagic))
    return Fail<LoadedStaticProject>(error, "invalid static package header or byte budget");
  Reader checksum{bytes.last(8)};
  std::uint64_t expected{};
  if (!checksum.Number(expected, 8) || Hash(bytes.first(bytes.size() - 8)) != expected)
    return Fail<LoadedStaticProject>(error, "static package checksum mismatch");
  Reader reader{bytes.first(bytes.size() - 8), 8};
  std::uint64_t schema{}, capability{}, count{};
  StaticProjectPackage package;
  if (!reader.Number(schema, 4) || schema != 1 || !reader.Number(capability, 4) ||
      capability != 1 || !reader.Number(package.project.high, 8) ||
      !reader.Number(package.project.low, 8) || !reader.Number(package.scene.high, 8) ||
      !reader.Number(package.scene.low, 8) || !reader.Number(count, 4) || count == 0 ||
      count > kMaximumProjectPackageAssets || count > (reader.bytes.size() - reader.offset) / 12)
    return Fail<LoadedStaticProject>(error, "invalid static package schema or asset count");
  AssetUuid previous;
  for (std::uint64_t index = 0; index < count; ++index) {
    std::uint64_t size{};
    if (!reader.Number(size, 8) || size > reader.bytes.size() - reader.offset)
      return Fail<LoadedStaticProject>(error, "truncated static package blob");
    const auto blob_bytes = reader.bytes.subspan(reader.offset, static_cast<std::size_t>(size));
    if (!PreflightBlob(blob_bytes))
      return Fail<LoadedStaticProject>(error, "runtime blob bounds failed");
    auto blob = AssetCooker::Deserialize(blob_bytes);
    if (!blob || !Nonzero(blob->id) || (index && !Less(previous, blob->id)) ||
        !std::ranges::is_sorted(blob->dependencies, Less))
      return Fail<LoadedStaticProject>(error, "invalid blob integrity or UUID ordering");
    previous = blob->id;
    package.assets.push_back(std::move(*blob));
    reader.offset += static_cast<std::size_t>(size);
  }
  if (reader.offset != reader.bytes.size())
    return Fail<LoadedStaticProject>(error, "unexpected static package suffix");
  return LoadStaticProjectPackage(package, error);
}

std::optional<LoadedStaticProject> ReadStaticProjectPackage(const std::filesystem::path &path,
                                                            std::string *error) {
  if (error)
    error->clear();
  std::error_code ec;
  const auto absolute = std::filesystem::absolute(path, ec);
  if (ec || path.empty())
    return Fail<LoadedStaticProject>(error, "invalid package input path");
#if defined(__unix__) || defined(__APPLE__)
  struct FileDescriptor {
    int value{-1};
    ~FileDescriptor() {
      if (value >= 0)
        close(value);
    }
  };
  FileDescriptor descriptor{open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
  if (descriptor.value < 0)
    return Fail<LoadedStaticProject>(error, "package root could not be opened");
  const auto relative = absolute.relative_path();
  for (auto part = relative.begin(); part != relative.end(); ++part) {
    auto next = part;
    ++next;
    const bool final = next == relative.end();
    const auto opened =
        openat(descriptor.value, part->c_str(),
               O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | (final ? 0 : O_DIRECTORY));
    if (opened < 0)
      return Fail<LoadedStaticProject>(error,
                                       "package path contains an alias or unavailable component");
    close(descriptor.value);
    descriptor.value = opened;
  }
  struct stat status{};
  if (fstat(descriptor.value, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size < 0 ||
      static_cast<std::uint64_t>(status.st_size) > kMaximumProjectPackageBytes)
    return Fail<LoadedStaticProject>(error, "package input is not a bounded regular file");
  ByteBuffer bytes;
  std::array<std::byte, 16384> chunk;
  for (;;) {
    const auto read_count = read(descriptor.value, chunk.data(), chunk.size());
    if (read_count == 0)
      break;
    if (read_count < 0) {
      if (errno == EINTR)
        continue;
      return Fail<LoadedStaticProject>(error, "package input read failed");
    }
    const auto count = static_cast<std::size_t>(read_count);
    if (count > kMaximumProjectPackageBytes - bytes.size())
      return Fail<LoadedStaticProject>(error, "package grew past byte budget");
    bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(count));
  }
  return DecodeStaticProjectPackage(bytes, error);
#else
  std::filesystem::path prefix;
  for (const auto &part : absolute) {
    prefix /= part;
    const auto status = std::filesystem::symlink_status(prefix, ec);
    if (ec || std::filesystem::is_symlink(status))
      return Fail<LoadedStaticProject>(error,
                                       "package path contains an alias or missing component");
  }
  if (!std::filesystem::is_regular_file(absolute, ec) || ec)
    return Fail<LoadedStaticProject>(error, "package input is not a regular file");
  const auto size = std::filesystem::file_size(absolute, ec);
  if (ec || size > kMaximumProjectPackageBytes)
    return Fail<LoadedStaticProject>(error, "package input byte budget exceeded");
  std::ifstream input(absolute, std::ios::binary);
  if (!input)
    return Fail<LoadedStaticProject>(error, "package input could not be opened");
  ByteBuffer bytes;
  std::array<char, 16384> chunk;
  while (input.read(chunk.data(), chunk.size()) || input.gcount() > 0) {
    const auto count = static_cast<std::size_t>(input.gcount());
    if (count > kMaximumProjectPackageBytes - bytes.size())
      return Fail<LoadedStaticProject>(error, "package grew past byte budget");
    const auto *begin = reinterpret_cast<const std::byte *>(chunk.data());
    bytes.insert(bytes.end(), begin, begin + count);
  }
  if (!input.eof())
    return Fail<LoadedStaticProject>(error, "package input read failed");
  return DecodeStaticProjectPackage(bytes, error);
#endif
}
} // namespace nexora::runtime
