#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Editor/StaticProjectExport.h"
#include "Nexora/Runtime/ProjectPackage.h"

#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
#if NEXORA_ASSET_PIPELINE_ENABLED
constexpr runtime::AssetUuid kProject{0x123456789abcdef0ULL, 77};
constexpr runtime::AssetUuid kScene{0xfedcba9876543210ULL, 88};
constexpr runtime::AssetUuid kMesh{0x123456789abcdef0ULL, 99};
constexpr runtime::AssetUuid kMaterial{0xabcdef0123456789ULL, 99};
constexpr runtime::Id kLegacy = std::numeric_limits<runtime::Id>::max();

editor::StaticProjectExportInput Fixture() {
  const auto mesh =
      editor::ImportObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nf 1/1 2/2 3/3\n");
  const auto material = editor::ImportMaterial(
      "NEXORA_MATERIAL 1\nbase_color 0.2 0.4 0.6\nmetallic 0.25\nroughness 0.5\n"
      "occlusion 1\nemission 0 0 2\n");
  Require(mesh.geometry && material.material, "real imported asset fixture failed");
  runtime::World world;
  world.LoadScene("Earlier scene"); // Capture scene ID is deliberately not the loader's ID.
  const auto scene = world.LoadScene("Static export", true);
  editor::SceneDocument document(world, scene);
  const auto root = document.Create("Authoring-only parent");
  const auto child = document.Create("Authoring-only mesh");
  const auto key = *document.Key(child);
  Require(document.SetMeshRenderer(
              key, runtime::MeshComponent{runtime::MeshResourceId(kMesh), {kLegacy}}) &&
              document.SetOpaqueComponent(key, editor::MaterialAssetReference(kMaterial)) &&
              document.SetOpaqueComponent(key, {5, std::string("legacy\xff", 7), {0, 255, 13, 10}}),
          "authoring mesh/material/opaque fixture failed");
  Require(document.Move(key, document.Key(root), 0), "hierarchy fixture failed");
  auto transform = *document.Transform(root);
  transform.x = 42;
  transform.sx = -2;
  Require(document.SetTransform(root, transform), "parent transform fixture failed");
  // This Runtime entity has no tracked Editor metadata, but must still resolve and export.
  auto &untracked = world.CreateEntity(scene);
  untracked.mesh_renderer = true;
  untracked.mesh_data.mesh = runtime::MeshResourceId(kMesh);
  const auto captured = document.CaptureRuntimeScene();
  Require(captured.has_value(), "owning SceneDocument capture failed");
  auto input = editor::StaticProjectExportInput{
      kProject, kScene, *captured, {{kMesh, *mesh.geometry}}, {{kMaterial, *material.material}}};
  Require(document.NewScene(), "capture ownership NewScene fixture failed");
  return input; // All World, Document and import DTOs are destroyed before cooking.
}

runtime::ByteBuffer VerifyRoundtrip(editor::StaticProjectExportInput input) {
  const auto snapshot = input.capture.runtime_snapshot;
  const auto nodes = input.capture.nodes;
  std::string error = "stale";
  auto bytes = editor::CookStaticProject(input, &error);
  Require(bytes && error.empty() && input.capture.runtime_snapshot == snapshot &&
              input.capture.nodes == nodes,
          "producer mutated capture or failed valid owning inputs");
  auto loaded = runtime::DecodeStaticProjectPackage(*bytes, &error);
  Require(loaded && error.empty() && loaded->ProjectId() == kProject &&
              loaded->SceneAssetId() == kScene && loaded->AssetCount() == 3 &&
              loaded->SceneId() != input.capture.scene &&
              loaded->WorldView().FindScene(loaded->SceneId())->entities.size() == 3 &&
              loaded->RenderItems().size() == 2 && loaded->InactiveComponentCount() == 1 &&
              loaded->SceneData().world_snapshot == snapshot,
          "actual Runtime package loader lost ownership, closure, provenance or opaque bytes");
  const auto &item = loaded->RenderItems()[0];
  Require(item.material && item.material->base_color == std::array{.2F, .4F, .6F} &&
              item.mesh->vertices.size() == 3 && item.mesh->indices.size() == 3 &&
              item.mesh->vertices[0].tangent == std::array{1.F, 0.F, 0.F, 1.F} &&
              loaded->WorldView().FindEntity(item.entity)->mesh_data.material.shader == kLegacy &&
              item.world_transform[12] == 42 &&
              loaded->SceneData().opaque[0].data ==
                  runtime::ByteBuffer{std::byte{0}, std::byte{255}, std::byte{13}, std::byte{10}} &&
              !loaded->RenderItems()[1].material,
          "scalar override, tangent, hierarchy, full legacy ID or neutral untracked mesh changed");
  const auto retained = item.mesh;
  const auto retained_material = item.material;
  std::reverse(input.capture.nodes.begin(), input.capture.nodes.end());
  for (auto &node : input.capture.nodes)
    std::reverse(node.opaque.begin(), node.opaque.end());
  auto unused_mesh = input.meshes[0];
  unused_mesh.asset = {444, 333};
  input.meshes.insert(input.meshes.begin(), unused_mesh);
  auto unused_material = input.materials[0];
  unused_material.asset = {222, 111};
  input.materials.insert(input.materials.begin(), unused_material);
  Require(editor::CookStaticProject(input) == bytes,
          "supplied order, metadata order or unused assets changed exact canonical closure");
  input = {};
  loaded.reset();
  Require(retained->vertices[1].position[0] == 1 && retained_material->emission[2] == 2,
          "loaded render values borrowed producer input/package lifetime");
  return std::move(*bytes);
}

void VerifyRejection(const editor::StaticProjectExportInput &valid) {
  const auto reject = [](editor::StaticProjectExportInput bad) {
    const auto snapshot = bad.capture.runtime_snapshot;
    const auto nodes = bad.capture.nodes;
    std::string error;
    Require(!editor::CookStaticProject(bad, &error) && !error.empty() &&
                bad.capture.runtime_snapshot == snapshot && bad.capture.nodes == nodes,
            "invalid export published bytes, omitted diagnostics or mutated inputs");
  };
  auto bad = valid;
  bad.project = {};
  reject(bad);
  bad = valid;
  bad.scene_asset = kMesh;
  reject(bad);
  bad = valid;
  bad.meshes.push_back(bad.meshes[0]);
  reject(bad);
  bad = valid;
  auto collision = bad.meshes[0];
  collision.asset.high ^= 1;
  collision.asset.low ^= std::rotl(std::uint64_t{1}, 23);
  Require(collision.asset != kMesh &&
              runtime::MeshResourceId(collision.asset) == runtime::MeshResourceId(kMesh),
          "full UUID collision fixture did not collide");
  bad.meshes.push_back(collision);
  reject(bad);
  bad = valid;
  bad.meshes.clear();
  reject(bad);
  bad = valid;
  bad.materials.clear();
  reject(bad);
  bad = valid;
  bad.materials[0].material.base_color[0] = .9F; // Divergence from actual reflected schema.
  reject(bad);
  bad = valid;
  bad.materials[0].material.schema.textures.push_back({"albedo", "external-file"});
  reject(bad);
  bad = valid;
  bad.meshes[0].geometry.vertices[0].normal = {};
  reject(bad);
  bad = valid;
  bad.meshes[0].geometry.vertices[0].uv[0] = std::numeric_limits<float>::infinity();
  reject(bad);
  bad = valid;
  bad.meshes[0].geometry.indices[0] = 99;
  reject(bad);
  bad = valid;
  bad.meshes[0].geometry.indices.pop_back();
  reject(bad);
  bad = valid;
  bad.capture.nodes[0].key.document_generation++;
  reject(bad);
  bad = valid;
  bad.capture.nodes.push_back(bad.capture.nodes[0]);
  reject(bad);
  bad = valid;
  bad.capture.nodes[0].key.id = 999;
  reject(bad);
  for (int mutation = 0; mutation < 6; ++mutation) {
    bad = valid;
    auto &records = bad.capture.nodes[1].opaque;
    auto &reference = records[1];
    Require(reference.type == editor::kMaterialAssetReferenceType,
            "reserved fixture record ordering changed");
    if (mutation == 0)
      reference.type_name = "wrong alias";
    if (mutation == 1)
      reference.type = 123;
    if (mutation == 2)
      reference.data[0] = 2;
    if (mutation == 3)
      reference.data.push_back(0);
    if (mutation == 4)
      std::fill(reference.data.begin() + 1, reference.data.end(), 0);
    if (mutation == 5)
      records.erase(records.begin() + 1); // Nonzero legacy shader cannot silently become neutral.
    reject(bad);
  }
  bad = valid;
  bad.capture.nodes[0].opaque = {editor::MaterialAssetReference(kMaterial)};
  reject(bad); // Reserved reference on a non-mesh parent.
  bad = valid;
  bad.capture.runtime_snapshot.append("trailing garbage");
  reject(bad);
  bad = valid;
  bad.capture.runtime_snapshot = "NEXORA_SCENE 3 \"Too many\" 0 100001\n";
  reject(bad);
  bad = valid;
  bad.capture.runtime_snapshot.resize(runtime::kCookedSceneMaximumSnapshotBytes + 1, ' ');
  reject(std::move(bad));
  bad = valid;
  bad.capture.nodes[0].opaque = {{7, "Oversized", std::vector<std::uint8_t>(1024 * 1024 + 1)}};
  reject(std::move(bad));
  bad = valid;
  bad.capture.nodes[0].opaque.resize(65);
  reject(std::move(bad));
  bad = valid;
  bad.meshes.resize(4096);
  reject(std::move(bad));

  bad = valid;
  bad.meshes[0].geometry.vertices.resize(runtime::kCookedMeshMaximumVertices + 1);
  reject(std::move(bad));
  bad = valid;
  bad.meshes[0].geometry.indices.resize(runtime::kCookedMeshMaximumIndices + 1);
  reject(std::move(bad));
  bad = valid;
  bad.capture.nodes[0].opaque = {{7, std::string(257, 'x'), {}}};
  reject(std::move(bad));
  bad = valid;
  bad.capture.nodes[0].opaque = {{7, std::string("A\0B", 3), {}}};
  reject(std::move(bad));

  auto maximum = valid;
  maximum.meshes[0].geometry.vertices.resize(runtime::kCookedMeshMaximumVertices,
                                             maximum.meshes[0].geometry.vertices[0]);
  Require(editor::CookStaticProject(maximum).has_value(), "maximum mesh vertex count rejected");
  maximum.meshes[0].geometry.indices.resize(runtime::kCookedMeshMaximumIndices - 1);
  for (std::size_t i = 0; i < maximum.meshes[0].geometry.indices.size(); ++i)
    maximum.meshes[0].geometry.indices[i] = static_cast<std::uint16_t>(i % 3);
  Require(editor::CookStaticProject(maximum).has_value(), "maximum aligned index count rejected");

  bad = valid;
  bad.meshes[0].geometry.vertices.resize(runtime::kCookedMeshMaximumVertices,
                                         bad.meshes[0].geometry.vertices[0]);
  for (std::uint64_t i = 1; i < 43; ++i) {
    auto extra = bad.meshes[0];
    extra.asset = {900, i};
    bad.meshes.push_back(std::move(extra));
  }
  reject(std::move(bad)); // Supplied geometry accounting also bounds unused catalog entries.
}

void VerifyLargeAndOpaqueLimits() {
  std::string text = "NEXORA_SCENE 3 \"Large\" 0 100000\n";
  for (std::size_t i = 0; i < 100000; ++i)
    text += std::to_string(i + 100) + " 0 0 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
  runtime::World world;
  const auto scene = world.LoadSceneSnapshot(text);
  Require(scene.has_value(), "100k no-mesh World fixture failed");
  editor::SceneDocument document(world, *scene);
  editor::StaticProjectExportInput input{kProject, kScene, *document.CaptureRuntimeScene(), {}, {}};
  const auto bytes = editor::CookStaticProject(input);
  Require(bytes.has_value(), "100k functional pure producer failed");
  const auto loaded = runtime::DecodeStaticProjectPackage(*bytes);
  Require(loaded && loaded->AssetCount() == 1 && loaded->RenderItems().empty() &&
              loaded->WorldView().FindScene(loaded->SceneId())->entities.size() == 100000,
          "100k producer did not preserve the complete owning World");

  input = Fixture();
  auto &records = input.capture.nodes[0].opaque;
  const std::string name(256, '\xff');
  for (std::uint64_t i = 1; i <= 16; ++i)
    records.push_back(
        {i, name, std::vector<std::uint8_t>(1024 * 1024 - (i == 16 ? 4113 : 0), 255)});
  // Other node has 7+4 and 21+17 bytes; leave exactly their 49 bytes in the global 16 MiB budget.
  records.back().data.resize(records.back().data.size() - 32);
  const auto bounded = editor::CookStaticProject(input);
  Require(bounded.has_value(), "exact opaque aggregate budget was rejected");
  records.back().data.push_back(0);
  std::string error;
  Require(!editor::CookStaticProject(input, &error), "opaque aggregate overflow was admitted");

  runtime::World record_world;
  const auto record_scene = record_world.LoadScene("Record bounds");
  for (int i = 0; i < 65; ++i)
    record_world.CreateEntity(record_scene);
  editor::SceneDocument record_document(record_world, record_scene);
  input = {kProject, kScene, *record_document.CaptureRuntimeScene(), {}, {}};
  const auto &entities = record_world.FindScene(record_scene)->entities;
  for (std::size_t i = 0; i < 64; ++i) {
    input.capture.nodes.push_back({{entities[i].id, 1, input.capture.document_generation}, {}});
    for (runtime::TypeId type = 1; type <= 64; ++type)
      input.capture.nodes.back().opaque.push_back({type, "T", {0, 255}});
  }
  const auto maximum_records = editor::CookStaticProject(input);
  Require(maximum_records.has_value(), "exact 4096-record producer limit rejected");
  const auto record_package = runtime::DecodeStaticProjectPackage(*maximum_records);
  Require(record_package && record_package->InactiveComponentCount() == 4096,
          "maximum opaque record set did not reach the actual package consumer");
  input.capture.nodes.push_back(
      {{entities.back().id, 1, input.capture.document_generation}, {{1, "T", {}}}});
  Require(!editor::CookStaticProject(input, &error), "global opaque record overflow admitted");
}
#endif
} // namespace

int main(int argc, char **argv) {
  try {
#if NEXORA_ASSET_PIPELINE_ENABLED
    const auto input = Fixture();
    const auto bytes = VerifyRoundtrip(input);
    VerifyRejection(input);
    VerifyLargeAndOpaqueLimits();
    if (argc == 3 && std::string_view(argv[1]) == "--write-package") {
      std::ofstream output(std::filesystem::path(argv[2]), std::ios::binary | std::ios::trunc);
      output.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
      output.close();
      Require(!output.fail(), "production-package fixture output failed");
    } else {
      Require(argc == 1, "invalid fixture arguments");
    }
#else
    static_cast<void>(argc);
    static_cast<void>(argv);
    std::string error;
    Require(!editor::CookStaticProject({}, &error) && !error.empty(),
            "feature-stripped producer returned bytes or omitted its diagnostic");
#endif
    std::cout << "Static project producer contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
