#include "Nexora/Runtime/CookedSceneAssets.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace nexora::runtime {
namespace {
constexpr std::array<std::byte, 8> kMeshMagic{std::byte{'N'}, std::byte{'X'}, std::byte{'C'},
                                              std::byte{'M'}, std::byte{'E'}, std::byte{'S'},
                                              std::byte{'H'}, std::byte{0}};
constexpr std::array<std::byte, 8> kPbrMagic{std::byte{'N'}, std::byte{'X'}, std::byte{'C'},
                                             std::byte{'P'}, std::byte{'B'}, std::byte{'R'},
                                             std::byte{0},   std::byte{0}};
constexpr std::array<std::byte, 8> kSceneMagic{std::byte{'N'}, std::byte{'X'}, std::byte{'C'},
                                               std::byte{'S'}, std::byte{'C'}, std::byte{'N'},
                                               std::byte{'E'}, std::byte{0}};
constexpr std::size_t kVertexBytes = 48;
constexpr std::size_t kBindingBytes = 52;
constexpr std::size_t kOpaqueHeaderBytes = 24;
constexpr auto kInvalid = foundation::ErrorCode::InvalidArgument;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);

class Reader final {
public:
  explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}
  [[nodiscard]] std::size_t Remaining() const noexcept { return bytes_.size() - offset_; }
  bool View(std::size_t count, std::span<const std::byte> &view) {
    if (count > Remaining())
      return false;
    view = bytes_.subspan(offset_, count);
    offset_ += count;
    return true;
  }
  template <typename T> bool Integer(T &value) {
    std::span<const std::byte> field;
    if (!View(sizeof(T), field))
      return false;
    value = 0;
    for (std::size_t index = 0; index < sizeof(T); ++index)
      value |= static_cast<T>(std::to_integer<unsigned char>(field[index])) << (index * 8);
    return true;
  }
  bool Float(float &value) {
    std::uint32_t bits;
    if (!Integer(bits))
      return false;
    value = std::bit_cast<float>(bits);
    return true;
  }
  bool Header(const std::array<std::byte, 8> &magic) {
    std::span<const std::byte> actual;
    std::uint32_t version;
    return View(magic.size(), actual) && std::ranges::equal(actual, magic) && Integer(version) &&
           version == 1;
  }

private:
  std::span<const std::byte> bytes_;
  std::size_t offset_{};
};

class Writer final {
public:
  explicit Writer(std::size_t count) { bytes.reserve(count); }
  void Raw(std::span<const std::byte> value) {
    bytes.insert(bytes.end(), value.begin(), value.end());
  }
  void Text(std::string_view value) { Raw(std::as_bytes(std::span(value.data(), value.size()))); }
  template <typename T> void Integer(T value) {
    for (std::size_t index = 0; index < sizeof(T); ++index)
      bytes.push_back(static_cast<std::byte>((value >> (index * 8)) & 0xffU));
  }
  void Float(float value) { Integer(std::bit_cast<std::uint32_t>(value)); }
  void Header(const std::array<std::byte, 8> &magic) {
    Raw(magic);
    Integer(std::uint32_t{1});
  }
  ByteBuffer bytes;
};

bool Nonzero(AssetUuid value) { return value.high != 0 || value.low != 0; }

template <std::size_t N> bool Finite(const std::array<float, N> &values) {
  return std::ranges::all_of(values, [](float value) { return std::isfinite(value); });
}

bool ValidMesh(const CookedMesh &mesh) {
  if (mesh.vertices.empty() || mesh.vertices.size() > kCookedMeshMaximumVertices ||
      mesh.indices.empty() || mesh.indices.size() > kCookedMeshMaximumIndices ||
      mesh.indices.size() % 3 != 0)
    return false;
  for (const auto &vertex : mesh.vertices) {
    if (!Finite(vertex.position) || !Finite(vertex.normal) || !Finite(vertex.uv) ||
        !Finite(vertex.tangent) ||
        std::hypot(double(vertex.normal[0]), double(vertex.normal[1]), double(vertex.normal[2])) <=
            1e-12 ||
        std::hypot(double(vertex.tangent[0]), double(vertex.tangent[1]),
                   double(vertex.tangent[2])) <= 1e-12 ||
        (vertex.tangent[3] != -1 && vertex.tangent[3] != 1))
      return false;
  }
  return std::ranges::all_of(mesh.indices,
                             [&](std::uint16_t index) { return index < mesh.vertices.size(); });
}

bool ValidMaterial(const CookedScalarPbr &material) {
  const auto unit = [](float value) { return std::isfinite(value) && value >= 0 && value <= 1; };
  return std::ranges::all_of(material.base_color, unit) &&
         std::ranges::all_of(
             material.emission,
             [](float value) { return std::isfinite(value) && value >= 0 && value <= 65504; }) &&
         unit(material.metallic) && unit(material.roughness) && unit(material.occlusion);
}

std::string_view Text(std::span<const std::byte> value) {
  return {reinterpret_cast<const char *>(value.data()), value.size()};
}

bool ValidOpaqueName(std::string_view name) {
  return !name.empty() && name.size() <= kCookedOpaqueMaximumNameBytes &&
         name.find_first_of("\r\n") == std::string_view::npos &&
         name.find('\0') == std::string_view::npos;
}

// Bound the text's declared entity count and IDs before asking World to allocate/parse a candidate.
// World remains the authority for normalized transforms, duplicate IDs and hierarchy validation.
struct DecodedWorld final {
  World world{WorldKind::Play};
  Id scene{};
};

std::optional<DecodedWorld> ReadWorld(std::string_view snapshot) {
  if (snapshot.empty() || snapshot.size() > kCookedSceneMaximumSnapshotBytes)
    return std::nullopt;
  std::istringstream input{std::string(snapshot)};
  input.imbue(std::locale::classic());
  std::string magic, name;
  unsigned version;
  bool persistent;
  std::uint64_t count;
  if (!(input >> magic >> version >> std::quoted(name) >> persistent >> count) ||
      magic != "NEXORA_SCENE" || version != 3 || name.empty() ||
      count > kCookedSceneMaximumEntities || count > snapshot.size() / 41)
    return std::nullopt;
  for (std::uint64_t index = 0; index < count; ++index) {
    Entity entity;
    if (!(input >> entity.id >> entity.parent >> entity.transform.x >> entity.transform.y >>
          entity.transform.z >> entity.transform.qx >> entity.transform.qy >> entity.transform.qz >>
          entity.transform.qw >> entity.transform.sx >> entity.transform.sy >>
          entity.transform.sz >> entity.camera >> entity.light >> entity.mesh_renderer >>
          entity.camera_data.vertical_field_of_view >> entity.camera_data.near_plane >>
          entity.camera_data.far_plane >> entity.light_data.intensity >> entity.mesh_data.mesh >>
          entity.mesh_data.material.shader) ||
        entity.id == 0 || entity.id == std::numeric_limits<Id>::max() ||
        entity.parent == std::numeric_limits<Id>::max() ||
        !std::isfinite(entity.camera_data.vertical_field_of_view) ||
        !std::isfinite(entity.camera_data.near_plane) ||
        !std::isfinite(entity.camera_data.far_plane) || !std::isfinite(entity.light_data.intensity))
      return std::nullopt;
    if (entity.camera &&
        (entity.camera_data.vertical_field_of_view <= 0 ||
         entity.camera_data.vertical_field_of_view >= 180 || entity.camera_data.near_plane <= 0 ||
         entity.camera_data.far_plane <= entity.camera_data.near_plane))
      return std::nullopt;
    if (entity.light && entity.light_data.intensity < 0)
      return std::nullopt;
  }
  input >> std::ws;
  if (!input.eof())
    return std::nullopt;
  DecodedWorld result;
  const auto scene = result.world.LoadSceneSnapshot(snapshot);
  if (!scene)
    return std::nullopt;
  result.scene = *scene;
  return result;
}

bool ValidScene(const CookedScene &scene) {
  if (scene.bindings.size() > kCookedSceneMaximumEntities ||
      scene.opaque.size() > kCookedOpaqueMaximumRecords)
    return false;
  auto world = ReadWorld(scene.world_snapshot);
  if (!world)
    return false;
  const auto &entities = world->world.FindScene(world->scene)->entities;
  std::unordered_map<Id, const Entity *> by_id;
  by_id.reserve(entities.size());
  for (const auto &entity : entities)
    by_id.emplace(entity.id, &entity);
  std::unordered_set<Id> bound;
  for (const auto &binding : scene.bindings) {
    const auto found = by_id.find(binding.entity);
    const auto *entity = found == by_id.end() ? nullptr : found->second;
    if (!entity || !entity->mesh_renderer || !Nonzero(binding.mesh) ||
        (binding.material && !Nonzero(*binding.material)) || binding.mesh_resource == 0 ||
        binding.mesh_resource != MeshResourceId(binding.mesh) ||
        entity->mesh_data.mesh != binding.mesh_resource || !bound.insert(binding.entity).second)
      return false;
  }
  for (const auto &entity : entities)
    if (entity.mesh_renderer && !bound.contains(entity.id))
      return false;
  std::size_t opaque_bytes{};
  std::unordered_map<Id, std::unordered_set<std::uint64_t>> types;
  for (const auto &opaque : scene.opaque) {
    if (!by_id.contains(opaque.entity) || opaque.type == 0 || !ValidOpaqueName(opaque.type_name) ||
        opaque.data.size() > kCookedOpaqueMaximumRecordBytes)
      return false;
    const auto size = opaque.type_name.size() + opaque.data.size();
    if (size > kCookedOpaqueMaximumBytes - opaque_bytes)
      return false;
    opaque_bytes += size;
    auto &entity_types = types[opaque.entity];
    if (entity_types.size() == kCookedOpaqueMaximumPerEntity ||
        !entity_types.insert(opaque.type).second)
      return false;
  }
  return true;
}

bool ReadBinding(Reader &reader, SceneAssetBinding &binding) {
  std::uint32_t present;
  AssetUuid material;
  if (!reader.Integer(binding.entity) || !reader.Integer(binding.mesh.high) ||
      !reader.Integer(binding.mesh.low) || !reader.Integer(present) || present > 1 ||
      !reader.Integer(material.high) || !reader.Integer(material.low) ||
      !reader.Integer(binding.mesh_resource) || (present == 0 && Nonzero(material)))
    return false;
  if (present)
    binding.material = material;
  return true;
}
} // namespace

foundation::Result<ByteBuffer> EncodeCookedMesh(const CookedMesh &mesh) {
  if (!ValidMesh(mesh))
    return kInvalid;
  Writer writer(20 + mesh.vertices.size() * kVertexBytes + mesh.indices.size() * 2);
  writer.Header(kMeshMagic);
  writer.Integer(static_cast<std::uint32_t>(mesh.vertices.size()));
  writer.Integer(static_cast<std::uint32_t>(mesh.indices.size()));
  for (const auto &vertex : mesh.vertices) {
    for (const auto value : vertex.position)
      writer.Float(value);
    for (const auto value : vertex.normal)
      writer.Float(value);
    for (const auto value : vertex.uv)
      writer.Float(value);
    for (const auto value : vertex.tangent)
      writer.Float(value);
  }
  for (const auto index : mesh.indices)
    writer.Integer(index);
  return std::move(writer.bytes);
}

foundation::Result<CookedMesh> DecodeCookedMesh(std::span<const std::byte> bytes) {
  Reader reader(bytes);
  std::uint32_t vertices, indices;
  if (!reader.Header(kMeshMagic) || !reader.Integer(vertices) || !reader.Integer(indices) ||
      vertices == 0 || vertices > kCookedMeshMaximumVertices || indices == 0 ||
      indices > kCookedMeshMaximumIndices || indices % 3 != 0 ||
      vertices > reader.Remaining() / kVertexBytes)
    return kInvalid;
  const auto vertex_bytes = std::size_t(vertices) * kVertexBytes;
  if (indices > (reader.Remaining() - vertex_bytes) / 2 ||
      reader.Remaining() - vertex_bytes != std::size_t(indices) * 2)
    return kInvalid;
  CookedMesh mesh;
  mesh.vertices.resize(vertices);
  mesh.indices.resize(indices);
  for (auto &vertex : mesh.vertices) {
    for (auto &value : vertex.position)
      if (!reader.Float(value))
        return kInvalid;
    for (auto &value : vertex.normal)
      if (!reader.Float(value))
        return kInvalid;
    for (auto &value : vertex.uv)
      if (!reader.Float(value))
        return kInvalid;
    for (auto &value : vertex.tangent)
      if (!reader.Float(value))
        return kInvalid;
  }
  for (auto &index : mesh.indices)
    if (!reader.Integer(index))
      return kInvalid;
  if (!ValidMesh(mesh))
    return kInvalid;
  return mesh;
}

foundation::Result<ByteBuffer> EncodeCookedScalarPbr(const CookedScalarPbr &material) {
  if (!ValidMaterial(material))
    return kInvalid;
  Writer writer(48);
  writer.Header(kPbrMagic);
  for (const auto value : material.base_color)
    writer.Float(value);
  for (const auto value : material.emission)
    writer.Float(value);
  writer.Float(material.metallic);
  writer.Float(material.roughness);
  writer.Float(material.occlusion);
  return std::move(writer.bytes);
}

foundation::Result<CookedScalarPbr> DecodeCookedScalarPbr(std::span<const std::byte> bytes) {
  Reader reader(bytes);
  CookedScalarPbr material;
  if (!reader.Header(kPbrMagic) || reader.Remaining() != 36)
    return kInvalid;
  for (auto &value : material.base_color)
    if (!reader.Float(value))
      return kInvalid;
  for (auto &value : material.emission)
    if (!reader.Float(value))
      return kInvalid;
  if (!reader.Float(material.metallic) || !reader.Float(material.roughness) ||
      !reader.Float(material.occlusion) || !ValidMaterial(material))
    return kInvalid;
  return material;
}

foundation::Result<ByteBuffer> EncodeCookedScene(const CookedScene &scene) {
  if (!ValidScene(scene))
    return kInvalid;
  std::vector<const SceneAssetBinding *> bindings;
  std::vector<const PreservedOpaqueRecord *> opaque;
  std::size_t size = 24 + scene.world_snapshot.size() + scene.bindings.size() * kBindingBytes;
  for (const auto &binding : scene.bindings)
    bindings.push_back(&binding);
  for (const auto &record : scene.opaque) {
    opaque.push_back(&record);
    size += kOpaqueHeaderBytes + record.type_name.size() + record.data.size();
  }
  std::ranges::sort(bindings, {}, [](const auto *binding) { return binding->entity; });
  std::ranges::sort(opaque, {},
                    [](const auto *record) { return std::pair(record->entity, record->type); });
  Writer writer(size);
  writer.Header(kSceneMagic);
  writer.Integer(static_cast<std::uint32_t>(scene.world_snapshot.size()));
  writer.Integer(static_cast<std::uint32_t>(bindings.size()));
  writer.Integer(static_cast<std::uint32_t>(opaque.size()));
  writer.Text(scene.world_snapshot);
  for (const auto *binding : bindings) {
    writer.Integer(binding->entity);
    writer.Integer(binding->mesh.high);
    writer.Integer(binding->mesh.low);
    writer.Integer(std::uint32_t(binding->material ? 1 : 0));
    writer.Integer(binding->material ? binding->material->high : std::uint64_t{0});
    writer.Integer(binding->material ? binding->material->low : std::uint64_t{0});
    writer.Integer(binding->mesh_resource);
  }
  for (const auto *record : opaque) {
    writer.Integer(record->entity);
    writer.Integer(record->type);
    writer.Integer(static_cast<std::uint32_t>(record->type_name.size()));
    writer.Integer(static_cast<std::uint32_t>(record->data.size()));
    writer.Text(record->type_name);
    writer.Raw(record->data);
  }
  return std::move(writer.bytes);
}

foundation::Result<CookedScene> DecodeCookedScene(std::span<const std::byte> bytes) {
  Reader reader(bytes);
  std::uint32_t snapshot_size, binding_count, opaque_count;
  std::span<const std::byte> snapshot;
  if (!reader.Header(kSceneMagic) || !reader.Integer(snapshot_size) ||
      !reader.Integer(binding_count) || !reader.Integer(opaque_count) || snapshot_size == 0 ||
      snapshot_size > kCookedSceneMaximumSnapshotBytes ||
      binding_count > kCookedSceneMaximumEntities || opaque_count > kCookedOpaqueMaximumRecords ||
      !reader.View(snapshot_size, snapshot) || binding_count > reader.Remaining() / kBindingBytes)
    return kInvalid;
  const auto binding_reader = reader;
  Id previous_entity{};
  for (std::uint32_t index = 0; index < binding_count; ++index) {
    SceneAssetBinding binding;
    if (!ReadBinding(reader, binding) || binding.entity <= previous_entity)
      return kInvalid;
    previous_entity = binding.entity;
  }
  if (opaque_count > reader.Remaining() / kOpaqueHeaderBytes)
    return kInvalid;
  const auto opaque_reader = reader;
  std::pair<Id, std::uint64_t> previous{};
  std::size_t opaque_bytes{};
  std::size_t entity_opaque_count{};
  for (std::uint32_t index = 0; index < opaque_count; ++index) {
    Id entity;
    std::uint64_t type;
    std::uint32_t name_size, data_size;
    std::span<const std::byte> name, data;
    if (!reader.Integer(entity) || !reader.Integer(type) || !reader.Integer(name_size) ||
        !reader.Integer(data_size) || entity == 0 || type == 0 ||
        std::pair(entity, type) <= previous || name_size > kCookedOpaqueMaximumNameBytes ||
        data_size > kCookedOpaqueMaximumRecordBytes || !reader.View(name_size, name) ||
        !ValidOpaqueName(Text(name)) || !reader.View(data_size, data))
      return kInvalid;
    const auto required = std::size_t(name_size) + data_size;
    if (required > kCookedOpaqueMaximumBytes - opaque_bytes)
      return kInvalid;
    opaque_bytes += required;
    entity_opaque_count = entity == previous.first ? entity_opaque_count + 1 : 1;
    if (entity_opaque_count > kCookedOpaqueMaximumPerEntity)
      return kInvalid;
    previous = {entity, type};
  }
  if (reader.Remaining() != 0)
    return kInvalid;

  // Every wire count and declared byte slice has been bounded against actual remaining input
  // before owning candidate allocations. Full semantic validation precedes return/publication.
  CookedScene scene;
  scene.world_snapshot = Text(snapshot);
  scene.bindings.reserve(binding_count);
  auto bindings = binding_reader;
  for (std::uint32_t index = 0; index < binding_count; ++index) {
    SceneAssetBinding binding;
    if (!ReadBinding(bindings, binding))
      return kInvalid;
    scene.bindings.push_back(binding);
  }
  auto records = opaque_reader;
  scene.opaque.reserve(opaque_count);
  for (std::uint32_t index = 0; index < opaque_count; ++index) {
    PreservedOpaqueRecord record;
    std::uint32_t name_size, data_size;
    std::span<const std::byte> name, data;
    if (!records.Integer(record.entity) || !records.Integer(record.type) ||
        !records.Integer(name_size) || !records.Integer(data_size) ||
        !records.View(name_size, name) || !records.View(data_size, data))
      return kInvalid;
    record.type_name = Text(name);
    record.data.assign(data.begin(), data.end());
    scene.opaque.push_back(std::move(record));
  }
  if (!ValidScene(scene))
    return kInvalid;
  return scene;
}

foundation::Result<renderer::Mesh> ToRendererMesh(const CookedMesh &mesh) {
  if (!ValidMesh(mesh))
    return kInvalid;
  renderer::Mesh result;
  result.vertices.reserve(mesh.vertices.size());
  for (const auto &vertex : mesh.vertices)
    result.vertices.push_back({vertex.position, vertex.normal, vertex.uv});
  result.indices = mesh.indices;
  return result;
}

foundation::Result<renderer::MaterialSchema> ToRendererMaterial(const CookedScalarPbr &material) {
  if (!ValidMaterial(material))
    return kInvalid;
  renderer::MaterialSchema result;
  result.shader_id = "nexora.runtime.scalar-pbr";
  if (std::ranges::any_of(material.emission, [](float value) { return value != 0; }))
    result.features = renderer::FeatureBit(renderer::MaterialFeature::Emission);
  result.parameters = {
      {"base_color", std::array<float, 4>{material.base_color[0], material.base_color[1],
                                          material.base_color[2], 1}},
      {"metallic", material.metallic},
      {"roughness", material.roughness},
      {"occlusion", material.occlusion},
      {"emission", material.emission}};
  return result;
}
} // namespace nexora::runtime
