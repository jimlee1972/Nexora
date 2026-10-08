#include "Nexora/Runtime/CookedSceneAssets.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using namespace nexora::runtime;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

void U32(ByteBuffer &bytes, std::size_t offset, std::uint32_t value) {
  for (std::size_t index = 0; index < 4; ++index)
    bytes.at(offset + index) = static_cast<std::byte>((value >> (index * 8)) & 0xff);
}

CookedMesh Triangle() {
  CookedMesh mesh;
  mesh.vertices = {{{-1, 0, 0}, {0, 0, 1}, {0, 0}, {1, 0, 0, 1}},
                   {{1, 0, 0}, {0, 0, 1}, {1, 0}, {1, 0, 0, 1}},
                   {{0, 1, 0}, {0, 0, 1}, {0.5F, 1}, {1, 0, 0, 1}}};
  mesh.indices = {0, 1, 2};
  return mesh;
}

template <typename Decoder> void BrokenFraming(const ByteBuffer &valid, Decoder decode) {
  for (std::size_t length = 0; length < valid.size(); ++length)
    Require(!decode(std::span(valid).first(length)), "truncated wire accepted");
  auto bad = valid;
  bad.push_back(std::byte{0});
  Require(!decode(bad), "trailing wire bytes accepted");
  bad = valid;
  bad.front() = std::byte{'?'};
  Require(!decode(bad), "unknown wire magic accepted");
  bad = valid;
  U32(bad, 8, 2);
  Require(!decode(bad), "unknown wire schema accepted");
}

void MeshAndMaterial() {
  constexpr AssetUuid identity{0x123456789abcdef0ULL, 0xfedcba9876543210ULL};
  static_assert(MeshResourceId(identity) == 0xb0f7765a695fb756ULL);
  static_assert(MeshResourceId({}) == 0);
  auto mesh = Triangle();
  const auto encoded = EncodeCookedMesh(mesh);
  Require(encoded.HasValue() && encoded.Value().size() == 20 + 3 * 48 + 3 * 2,
          "mesh encode did not produce expected explicit wire size");
  const auto &wire = encoded.Value();
  Require(wire[8] == std::byte{1} && wire[12] == std::byte{3} && wire[13] == std::byte{0} &&
              wire[20] == std::byte{0} && wire[23] == std::byte{0xbf},
          "mesh fields are not canonical little-endian binary32");
  auto decoded = DecodeCookedMesh(wire);
  Require(decoded.HasValue() && decoded.Value().vertices[0].position == mesh.vertices[0].position &&
              decoded.Value().vertices[0].tangent == mesh.vertices[0].tangent &&
              decoded.Value().indices == mesh.indices &&
              EncodeCookedMesh(decoded.Value()).Value() == wire,
          "mesh owning deterministic roundtrip failed");
  BrokenFraming(wire, DecodeCookedMesh);
  auto bad_wire = wire;
  U32(bad_wire, 12, std::numeric_limits<std::uint32_t>::max());
  Require(!DecodeCookedMesh(bad_wire), "oversized vertex declaration accepted");
  bad_wire = wire;
  U32(bad_wire, 16, std::numeric_limits<std::uint32_t>::max());
  Require(!DecodeCookedMesh(bad_wire), "oversized index declaration accepted");
  bad_wire = wire;
  U32(bad_wire, 20, 0x7f800000U);
  Require(!DecodeCookedMesh(bad_wire), "infinite wire position accepted");
  bad_wire = wire;
  bad_wire[20 + 3 * 48] = std::byte{3};
  Require(!DecodeCookedMesh(bad_wire), "out-of-range triangle index accepted");
  for (const auto nonfinite :
       {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
    auto bad = mesh;
    bad.vertices[0].normal[0] = nonfinite;
    Require(!EncodeCookedMesh(bad), "nonfinite normal accepted");
    bad = mesh;
    bad.vertices[0].uv[0] = nonfinite;
    Require(!EncodeCookedMesh(bad), "nonfinite UV accepted");
    bad = mesh;
    bad.vertices[0].tangent[0] = nonfinite;
    Require(!EncodeCookedMesh(bad), "nonfinite tangent accepted");
  }
  auto bad = mesh;
  bad.vertices[0].normal = {};
  Require(!EncodeCookedMesh(bad), "zero normal accepted");
  bad = mesh;
  bad.vertices[0].tangent[3] = 0;
  Require(!EncodeCookedMesh(bad), "invalid tangent handedness accepted");
  bad = mesh;
  bad.indices.pop_back();
  Require(!EncodeCookedMesh(bad), "incomplete triangle accepted");
  bad = mesh;
  bad.vertices.resize(kCookedMeshMaximumVertices, mesh.vertices[0]);
  bad.indices = {0, 1, static_cast<std::uint16_t>(kCookedMeshMaximumVertices - 1)};
  auto maximum = EncodeCookedMesh(bad);
  Require(maximum.HasValue() && DecodeCookedMesh(maximum.Value()).HasValue(),
          "actual Editor vertex bound rejected");
  bad.vertices.push_back(mesh.vertices[0]);
  Require(!EncodeCookedMesh(bad), "vertex bound exceeded");
  bad = mesh;
  bad.indices.assign(kCookedMeshMaximumIndices - 1, 0);
  maximum = EncodeCookedMesh(bad);
  Require(maximum.HasValue() && DecodeCookedMesh(maximum.Value()).HasValue(),
          "largest complete-triangle index count rejected");
  bad.indices.insert(bad.indices.end(), 3, 0);
  Require(!EncodeCookedMesh(bad), "index byte/count bound exceeded");

  CookedScalarPbr material{{0.8F, 0.2F, 0.1F}, {0, 1, 65504}, 0.25F, 0.5F, 1};
  auto material_wire = EncodeCookedScalarPbr(material);
  Require(material_wire.HasValue() && material_wire.Value().size() == 48,
          "scalar PBR wire size changed");
  const auto restored = DecodeCookedScalarPbr(material_wire.Value());
  Require(restored.HasValue() && restored.Value().emission == material.emission &&
              restored.Value().base_color == material.base_color &&
              EncodeCookedScalarPbr(restored.Value()).Value() == material_wire.Value(),
          "material deterministic roundtrip failed");
  BrokenFraming(material_wire.Value(), DecodeCookedScalarPbr);
  auto bad_material = material;
  bad_material.metallic = 1.01F;
  Require(!EncodeCookedScalarPbr(bad_material), "out-of-range metallic accepted");
  bad_material = material;
  bad_material.roughness = -0.1F;
  Require(!EncodeCookedScalarPbr(bad_material), "negative roughness accepted");
  bad_material = material;
  bad_material.occlusion = std::numeric_limits<float>::quiet_NaN();
  Require(!EncodeCookedScalarPbr(bad_material), "nonfinite occlusion accepted");
  bad_material = material;
  bad_material.emission[0] = 65505;
  Require(!EncodeCookedScalarPbr(bad_material), "emission bound exceeded");
  bad_material = material;
  bad_material.base_color[0] = -1;
  Require(!EncodeCookedScalarPbr(bad_material), "negative base color accepted");
  auto invalid_material_wire = material_wire.Value();
  U32(invalid_material_wire, 12, 0x7fc00000U);
  Require(!DecodeCookedScalarPbr(invalid_material_wire), "wire NaN material accepted");

  auto renderer_mesh = ToRendererMesh(mesh);
  auto renderer_material = ToRendererMaterial(material);
  Require(renderer_mesh.HasValue() && renderer_material.HasValue() &&
              renderer::ValidateMaterial(renderer_material.Value()).valid &&
              renderer::ReflectMaterial(renderer_material.Value()).parameters.size() == 5 &&
              renderer::GenerateMeshTangents(renderer_mesh.Value()).has_value(),
          "real Renderer contracts rejected public cooked adapters");
  renderer::SceneFrame frame;
  frame.mesh = renderer_mesh.Value();
  Require(renderer::ValidateSceneFrame(frame), "real Renderer scene-frame consumption failed");
  mesh.vertices.clear();
  material.base_color = {};
  Require(renderer_mesh.Value().vertices.size() == 3 &&
              std::get<std::array<float, 4>>(renderer_material.Value().parameters[0].value)[0] ==
                  0.8F,
          "Renderer adapters borrowed cooked input storage");
}

CookedScene SceneFixture() {
  constexpr AssetUuid mesh{1, 2};
  World world;
  const auto scene_id = world.LoadScene("Cooked hierarchy");
  auto &root = world.CreateEntity(scene_id);
  root.transform = {1, 2, 3};
  root.transform.sx = 2;
  const auto parent = root.id;
  auto &first = world.CreateEntity(scene_id);
  first.parent = parent;
  first.transform.x = 2;
  first.mesh_renderer = true;
  first.mesh_data = {MeshResourceId(mesh), {0xfedcba9876543210ULL}};
  const auto first_id = first.id;
  auto &second = world.CreateEntity(scene_id);
  second.parent = parent;
  second.mesh_renderer = true;
  second.mesh_data.mesh = MeshResourceId(mesh);
  const auto second_id = second.id;
  auto &camera = world.CreateEntity(scene_id);
  camera.camera = true;
  camera.transform.z = 5;
  CookedScene scene;
  scene.world_snapshot = *world.SaveScene(scene_id);
  scene.bindings = {{second_id, mesh, std::nullopt, MeshResourceId(mesh)},
                    {first_id, mesh, AssetUuid{3, 4}, MeshResourceId(mesh)}};
  scene.opaque = {{first_id, 10, "vendor.tool.未知", {std::byte{0}, std::byte{255}, std::byte{1}}},
                  {first_id, 9, std::string("legacy.\xff", 8), {}}};
  return scene;
}

void SceneAndCooking() {
  auto scene = SceneFixture();
  auto encoded = EncodeCookedScene(scene);
  Require(encoded.HasValue(), "real World snapshot/bindings encode failed");
  const auto wire = encoded.Value();
  auto decoded = DecodeCookedScene(wire);
  Require(decoded.HasValue() && decoded.Value().world_snapshot == scene.world_snapshot &&
              decoded.Value().bindings.front().entity == scene.bindings.back().entity &&
              decoded.Value().opaque.front().type == 9 &&
              decoded.Value().opaque.back().data == scene.opaque[0].data,
          "cooked scene sorted owning roundtrip failed");
  std::ranges::reverse(scene.bindings);
  std::ranges::reverse(scene.opaque);
  Require(EncodeCookedScene(scene).Value() == wire,
          "unordered owning inputs changed canonical wire");
  BrokenFraming(wire, DecodeCookedScene);
  auto bad_wire = wire;
  U32(bad_wire, 12, std::numeric_limits<std::uint32_t>::max());
  Require(!DecodeCookedScene(bad_wire), "oversized snapshot declaration accepted");
  bad_wire = wire;
  U32(bad_wire, 16, std::numeric_limits<std::uint32_t>::max());
  Require(!DecodeCookedScene(bad_wire), "oversized binding declaration accepted");
  bad_wire = wire;
  U32(bad_wire, 20, std::numeric_limits<std::uint32_t>::max());
  Require(!DecodeCookedScene(bad_wire), "oversized opaque declaration accepted");
  bad_wire = wire;
  U32(bad_wire, 24 + scene.world_snapshot.size() + 24, 2);
  Require(!DecodeCookedScene(bad_wire), "unknown material presence enum accepted");
  auto bad = scene;
  bad.bindings[0].mesh_resource ^= 1;
  Require(!EncodeCookedScene(bad), "UUID/resource mismatch accepted");
  bad = scene;
  bad.bindings.pop_back();
  Require(!EncodeCookedScene(bad), "missing mesh entity binding accepted");
  bad = scene;
  bad.bindings.push_back(bad.bindings.front());
  Require(!EncodeCookedScene(bad), "duplicate mesh binding accepted");
  bad = scene;
  bad.bindings[0].entity = 999;
  Require(!EncodeCookedScene(bad), "foreign entity binding accepted");
  bad = scene;
  bad.bindings[0].material = AssetUuid{};
  Require(!EncodeCookedScene(bad), "zero material UUID accepted");
  bad = scene;
  bad.opaque.push_back(bad.opaque.front());
  Require(!EncodeCookedScene(bad), "duplicate opaque type/entity accepted");
  bad = scene;
  bad.opaque[0].entity = 999;
  Require(!EncodeCookedScene(bad), "foreign opaque entity accepted");
  bad = scene;
  bad.opaque[0].type_name = std::string("bad\0name", 8);
  Require(!EncodeCookedScene(bad), "NUL opaque name accepted");
  bad = scene;
  bad.opaque[0].data.resize(kCookedOpaqueMaximumRecordBytes + 1);
  Require(!EncodeCookedScene(bad), "opaque record byte bound exceeded");
  bad = scene;
  bad.world_snapshot = "NEXORA_SCENE 3 \"too large\" 0 18446744073709551615\n";
  Require(!EncodeCookedScene(bad), "huge declared entity count accepted");
  bad = scene;
  bad.world_snapshot.replace(12, 1, "2");
  Require(!EncodeCookedScene(bad), "legacy World schema accepted as cooked schema3");

  bad = scene;
  bad.opaque.clear();
  const auto entity = scene.bindings[0].entity;
  for (std::uint64_t index = 1; index <= 16; ++index)
    bad.opaque.push_back({entity, index, "a", ByteBuffer(kCookedOpaqueMaximumRecordBytes - 1)});
  auto exact_opaque = EncodeCookedScene(bad);
  Require(exact_opaque.HasValue() && DecodeCookedScene(exact_opaque.Value()).HasValue(),
          "exact aggregate opaque byte bound rejected");
  bad.opaque[0].data.push_back(std::byte{0});
  Require(!EncodeCookedScene(bad), "aggregate opaque bytes excluded name or exceeded bound");
  bad = scene;
  bad.opaque.clear();
  for (std::uint64_t index = 1; index <= 64; ++index)
    bad.opaque.push_back({entity, index, "a", {}});
  Require(EncodeCookedScene(bad).HasValue(), "per-entity opaque exact count rejected");
  bad.opaque.push_back({entity, 65, "a", {}});
  Require(!EncodeCookedScene(bad), "per-entity opaque count exceeded");

  bad = scene;
  bad.opaque = {{entity, 1, std::string(kCookedOpaqueMaximumNameBytes, 'n'),
                 ByteBuffer(kCookedOpaqueMaximumRecordBytes)}};
  const auto exact_record = EncodeCookedScene(bad);
  Require(exact_record.HasValue() && DecodeCookedScene(exact_record.Value()).HasValue(),
          "exact opaque record/name byte bounds rejected");
  bad.opaque[0].type_name.push_back('n');
  Require(!EncodeCookedScene(bad), "opaque name byte bound exceeded");
  bad = scene;
  bad.bindings.clear();
  bad.opaque.clear();
  bad.world_snapshot = "NEXORA_SCENE 3 \"Overflow\" 0 1\n"
                       "18446744073709551615 0 0 0 0 0 0 0 1 1 1 1 0 0 0 1 .1 100 1 0 0\n";
  Require(!EncodeCookedScene(bad), "maximum entity ID wrapped World allocation identity");
  World opaque_world;
  const auto opaque_scene = opaque_world.LoadScene("Opaque budget");
  CookedScene many_opaque;
  for (std::size_t owner = 0; owner < 64; ++owner) {
    const auto id = opaque_world.CreateEntity(opaque_scene).id;
    for (std::uint64_t type = 1; type <= 64; ++type)
      many_opaque.opaque.push_back({id, type, "a", {}});
  }
  many_opaque.world_snapshot = *opaque_world.SaveScene(opaque_scene);
  const auto exact_count = EncodeCookedScene(many_opaque);
  Require(exact_count.HasValue() && DecodeCookedScene(exact_count.Value()).HasValue(),
          "exact global opaque record count rejected");
  many_opaque.opaque.push_back({entity, 65, "a", {}});
  Require(!EncodeCookedScene(many_opaque), "global opaque record count exceeded");
  World invalid_camera;
  const auto camera_scene = invalid_camera.LoadScene("Invalid camera");
  auto &camera = invalid_camera.CreateEntity(camera_scene);
  camera.camera = true;
  camera.camera_data.vertical_field_of_view = 180;
  Require(!EncodeCookedScene({*invalid_camera.SaveScene(camera_scene), {}, {}}),
          "invalid active Camera projection admitted");
  camera.camera_data.vertical_field_of_view = 60;
  Require(EncodeCookedScene({*invalid_camera.SaveScene(camera_scene), {}, {}}).HasValue(),
          "default Runtime degree-based Camera rejected");
  camera.camera_data.near_plane = 0;
  Require(!EncodeCookedScene({*invalid_camera.SaveScene(camera_scene), {}, {}}),
          "zero Camera near plane admitted");
  camera.camera_data.near_plane = 0.1F;
  camera.camera_data.far_plane = 0.1F;
  Require(!EncodeCookedScene({*invalid_camera.SaveScene(camera_scene), {}, {}}),
          "Camera far plane not greater than near admitted");
  camera.camera_data.far_plane = 100;
  camera.camera = false;
  camera.light = true;
  camera.light_data.intensity = -1;
  Require(!EncodeCookedScene({*invalid_camera.SaveScene(camera_scene), {}, {}}),
          "invalid active Light intensity admitted");

  World consumer{WorldKind::Play};
  const auto loaded = consumer.LoadSceneSnapshot(decoded.Value().world_snapshot);
  Require(loaded && consumer.Activate(*loaded), "real Runtime consumer failed to load/activate");
  const auto &binding = decoded.Value().bindings.front();
  const auto *live = consumer.FindEntity(binding.entity);
  Require(live && live->mesh_data.mesh == binding.mesh_resource &&
              live->mesh_data.material.shader == 0xfedcba9876543210ULL && live->parent != 0 &&
              consumer.WorldTransform(live->id)->x == 5,
          "Runtime hierarchy/transform/full legacy shader identity changed");
  scene.world_snapshot.clear();
  scene.bindings.clear();
  scene.opaque.clear();
  Require(!decoded.Value().world_snapshot.empty() && decoded.Value().opaque.back().data.size() == 3,
          "decoded scene borrowed source storage");

  const auto mesh_wire = EncodeCookedMesh(Triangle()).Value();
  const auto material_wire = EncodeCookedScalarPbr({}).Value();
  DerivedDataCache cache;
  AssetCooker cooker;
  const auto mesh_blob = cooker.Cook({{1, 2}, std::string(kCookedMeshType), {}, mesh_wire}, "linux",
                                     "static-view-v1", cache);
  const auto material_blob =
      cooker.Cook({{3, 4}, std::string(kCookedScalarPbrType), {}, material_wire}, "linux",
                  "static-view-v1", cache);
  const auto scene_blob =
      cooker.Cook({{5, 6}, std::string(kCookedSceneType), {{1, 2}, {3, 4}}, wire}, "linux",
                  "static-view-v1", cache);
  Require(mesh_blob && material_blob && scene_blob, "real AssetCooker rejected canonical payloads");
  const auto bundle =
      BundleBuilder::Build("static-scene", 1, {}, {*mesh_blob, *material_blob, *scene_blob});
  Require(bundle && BundleBuilder::Verify(*bundle), "real cooked bundle verification failed");
  const auto restored_blob = AssetCooker::Deserialize(AssetCooker::Serialize(*scene_blob));
  Require(restored_blob && DecodeCookedScene(restored_blob->payload).HasValue() &&
              restored_blob->dependencies.size() == 2,
          "NXAB RuntimeBlob wire lost scene or dependency closure");
}
} // namespace

int main() {
  try {
    MeshAndMaterial();
    SceneAndCooking();
    std::cout << "Cooked static-scene assets passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
