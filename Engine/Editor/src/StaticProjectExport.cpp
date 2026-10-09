#include "Nexora/Editor/StaticProjectExport.h"

#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Runtime/ProjectPackage.h"

#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>
#include <unordered_set>

namespace nexora::editor {
namespace {
#if NEXORA_ASSET_PIPELINE_ENABLED
bool Nonzero(runtime::AssetUuid id) { return id.high != 0 || id.low != 0; }
bool LessUuid(runtime::AssetUuid a, runtime::AssetUuid b) {
  return a.high < b.high || (a.high == b.high && a.low < b.low);
}
// Bound the declared count before World can allocate a candidate. Shared cooked-scene validation
// below remains authoritative for every entity, field, hierarchy, binding and opaque record.
bool BoundedSnapshot(std::string_view snapshot) {
  if (snapshot.empty() || snapshot.size() > runtime::kCookedSceneMaximumSnapshotBytes)
    return false;
  std::istringstream stream{std::string(snapshot)};
  stream.imbue(std::locale::classic());
  std::string magic, name;
  unsigned version{};
  bool persistent{};
  std::uint64_t count{};
  return (stream >> magic >> version >> std::quoted(name) >> persistent >> count) &&
         magic == "NEXORA_SCENE" && version == 3 && !name.empty() &&
         count <= runtime::kCookedSceneMaximumEntities && count <= snapshot.size() / 41;
}
std::optional<runtime::AssetUuid> MaterialReference(const OpaqueComponent &record) {
  if (record.type != kMaterialAssetReferenceType ||
      record.type_name != kMaterialAssetReferenceName || record.data.size() != 17 ||
      record.data[0] != 1)
    return std::nullopt;
  runtime::AssetUuid id;
  for (unsigned i = 0; i < 8; ++i) {
    id.high |= std::uint64_t(record.data[1 + i]) << (i * 8);
    id.low |= std::uint64_t(record.data[9 + i]) << (i * 8);
  }
  return Nonzero(id) ? std::optional{id} : std::nullopt;
}
#endif
} // namespace

std::optional<runtime::ByteBuffer> CookStaticProject(const StaticProjectExportInput &input,
                                                     std::string *error) {
  if (error)
    error->clear();
  const auto reject = [&](const char *message) -> std::optional<runtime::ByteBuffer> {
    if (error)
      *error = message;
    return std::nullopt;
  };
#if !NEXORA_ASSET_PIPELINE_ENABLED
  static_cast<void>(input);
  return reject("Static project export requires the Asset Pipeline feature.");
#else
  if (!Nonzero(input.project) || !Nonzero(input.scene_asset) || !input.capture.scene ||
      !input.capture.document_generation ||
      input.capture.nodes.size() > runtime::kCookedSceneMaximumEntities ||
      !BoundedSnapshot(input.capture.runtime_snapshot))
    return reject("Static project export has invalid or oversized captured scene identities/text.");
  if (input.meshes.size() >= runtime::kMaximumProjectPackageAssets ||
      input.materials.size() >= runtime::kMaximumProjectPackageAssets ||
      input.materials.size() > runtime::kMaximumProjectPackageAssets - 1 - input.meshes.size())
    return reject("Static project export exceeds the supplied asset limit (4095 plus scene).");

  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> identities{input.scene_asset};
  std::unordered_map<std::uint64_t, const StaticMeshAsset *> meshes;
  std::unordered_map<runtime::AssetUuid, const StaticMaterialAsset *, runtime::AssetUuidHash>
      materials;
  std::size_t geometry_bytes{};
  for (const auto &mesh : input.meshes) {
    if (!Nonzero(mesh.asset) || !identities.insert(mesh.asset).second ||
        !meshes.emplace(runtime::MeshResourceId(mesh.asset), &mesh).second)
      return reject("Static project export has duplicate UUIDs or colliding mesh resource IDs.");
    if (mesh.geometry.vertices.empty() ||
        mesh.geometry.vertices.size() > runtime::kCookedMeshMaximumVertices ||
        mesh.geometry.indices.empty() ||
        mesh.geometry.indices.size() > runtime::kCookedMeshMaximumIndices)
      return reject("Static project export has oversized or empty supplied geometry.");
    const auto bytes = mesh.geometry.vertices.size() * 48 + mesh.geometry.indices.size() * 2;
    if (bytes > runtime::kMaximumProjectGeometryBytes - geometry_bytes)
      return reject("Static project export exceeds the 128 MiB supplied geometry limit.");
    geometry_bytes += bytes;
  }
  for (const auto &material : input.materials) {
    if (!Nonzero(material.asset) || !identities.insert(material.asset).second)
      return reject("Static project export has duplicate or zero material UUIDs.");
    materials.emplace(material.asset, &material);
  }

  runtime::World candidate{runtime::WorldKind::Play};
  const auto scene_id = candidate.LoadSceneSnapshot(input.capture.runtime_snapshot);
  if (!scene_id)
    return reject("Static project export has an invalid Runtime snapshot.");
  // Scene-local IDs are not serialized. The captured scene ID is provenance, not the new ID.
  const auto &entities = candidate.FindScene(*scene_id)->entities;
  std::unordered_map<runtime::Id, const runtime::Entity *> by_id;
  by_id.reserve(entities.size());
  for (const auto &entity : entities)
    by_id.emplace(entity.id, &entity);

  runtime::CookedScene scene{input.capture.runtime_snapshot, {}, {}};
  std::unordered_set<runtime::Id> node_ids;
  std::unordered_map<runtime::Id, runtime::AssetUuid> references;
  std::size_t record_count{}, opaque_bytes{};
  for (const auto &node : input.capture.nodes) {
    if (!node.key.id || !node.key.entity_generation ||
        node.key.document_generation != input.capture.document_generation ||
        !by_id.contains(node.key.id) || !node_ids.insert(node.key.id).second ||
        node.opaque.size() > runtime::kCookedOpaqueMaximumPerEntity ||
        node.opaque.size() > runtime::kCookedOpaqueMaximumRecords - record_count)
      return reject("Static project export has invalid tracked node identities or opaque counts.");
    record_count += node.opaque.size();
    std::unordered_set<runtime::TypeId> types;
    for (const auto &opaque : node.opaque) {
      if (!opaque.type || !types.insert(opaque.type).second || opaque.type_name.empty() ||
          opaque.type_name.size() > runtime::kCookedOpaqueMaximumNameBytes ||
          opaque.type_name.find_first_of("\r\n") != std::string::npos ||
          opaque.type_name.find('\0') != std::string::npos ||
          opaque.data.size() > runtime::kCookedOpaqueMaximumRecordBytes)
        return reject("Static project export has invalid opaque record framing.");
      const auto bytes = opaque.type_name.size() + opaque.data.size();
      if (bytes > runtime::kCookedOpaqueMaximumBytes - opaque_bytes)
        return reject("Static project export exceeds the 16 MiB opaque names/payload limit.");
      opaque_bytes += bytes;
      if (opaque.type == kMaterialAssetReferenceType ||
          opaque.type_name == kMaterialAssetReferenceName) {
        const auto reference = MaterialReference(opaque);
        if (!reference || !by_id.at(node.key.id)->mesh_renderer ||
            !references.emplace(node.key.id, *reference).second)
          return reject("Static project export has an unsupported reserved material reference.");
      }
      runtime::ByteBuffer bytes_copy;
      bytes_copy.reserve(opaque.data.size());
      for (const auto byte : opaque.data)
        bytes_copy.push_back(static_cast<std::byte>(byte));
      scene.opaque.push_back({node.key.id, opaque.type, opaque.type_name, std::move(bytes_copy)});
    }
  }

  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> used_meshes, used_materials;
  for (const auto &entity : entities) {
    if (!entity.mesh_renderer)
      continue;
    const auto mesh = meshes.find(entity.mesh_data.mesh);
    if (mesh == meshes.end())
      return reject("Static project export is missing an explicitly supplied mesh UUID/resource.");
    std::optional<runtime::AssetUuid> material;
    if (const auto reference = references.find(entity.id); reference != references.end()) {
      if (!materials.contains(reference->second))
        return reject("Static project export is missing an explicitly supplied scalar material.");
      material = reference->second;
      used_materials.insert(*material);
    } else if (entity.mesh_data.material.shader != 0) {
      return reject("StaticView requires a scalar override for a nonzero legacy shader ID.");
    }
    used_meshes.insert(mesh->second->asset);
    scene.bindings.push_back({entity.id, mesh->second->asset, material, entity.mesh_data.mesh});
  }
  auto scene_payload = runtime::EncodeCookedScene(scene);
  if (!scene_payload)
    return reject("Static project export failed shared cooked-scene validation.");

  runtime::StaticProjectPackage package{input.project, input.scene_asset, {}};
  runtime::DerivedDataCache cache;
  runtime::AssetCooker cooker;
  const auto cook = [&](runtime::AssetUuid id, std::string_view type, runtime::ByteBuffer payload,
                        std::vector<runtime::AssetUuid> deps = {}) {
    auto blob = cooker.Cook({id, std::string(type), std::move(deps), std::move(payload)},
                            "portable", "static-view-v1", cache);
    if (!blob)
      return false;
    package.assets.push_back(std::move(*blob));
    return true;
  };
  for (const auto &asset : input.meshes) {
    if (!used_meshes.contains(asset.asset))
      continue;
    renderer::Mesh mesh;
    mesh.indices = asset.geometry.indices;
    mesh.vertices.reserve(asset.geometry.vertices.size());
    for (const auto &vertex : asset.geometry.vertices)
      mesh.vertices.push_back({vertex.position, vertex.normal, vertex.uv});
    const auto tangents = renderer::GenerateMeshTangents(mesh);
    if (!tangents)
      return reject("Static project export has invalid geometry for tangent generation.");
    runtime::CookedMesh cooked;
    cooked.indices = mesh.indices;
    cooked.vertices.reserve(mesh.vertices.size());
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
      const auto &vertex = mesh.vertices[i];
      cooked.vertices.push_back({vertex.position, vertex.normal, vertex.uv, (*tangents)[i]});
    }
    auto payload = runtime::EncodeCookedMesh(cooked);
    if (!payload || !cook(asset.asset, runtime::kCookedMeshType, std::move(payload.Value())))
      return reject("Static project export failed shared mesh cooking.");
  }
  for (const auto &asset : input.materials) {
    if (!used_materials.contains(asset.asset))
      continue;
    const auto &material = asset.material;
    if (!ValidateMaterialAsset(material).valid)
      return reject(
          "Static project export has an unsupported or divergent scalar material schema.");
    auto payload =
        runtime::EncodeCookedScalarPbr({material.base_color, material.emission, material.metallic,
                                        material.roughness, material.occlusion});
    if (!payload || !cook(asset.asset, runtime::kCookedScalarPbrType, std::move(payload.Value())))
      return reject("Static project export failed shared scalar material cooking.");
  }
  std::vector<runtime::AssetUuid> dependencies{used_meshes.begin(), used_meshes.end()};
  dependencies.insert(dependencies.end(), used_materials.begin(), used_materials.end());
  std::ranges::sort(dependencies, LessUuid);
  if (!cook(input.scene_asset, runtime::kCookedSceneType, std::move(scene_payload.Value()),
            std::move(dependencies)))
    return reject("Static project export failed scene asset cooking.");
  return runtime::EncodeStaticProjectPackage(package, error);
#endif
}
} // namespace nexora::editor
