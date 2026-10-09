#include "GameViewPreview.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace

int main() {
  try {
    using namespace nexora;
    using namespace editor;
    constexpr runtime::AssetUuid mesh_uuid{9, 1}, red_uuid{1, 1}, missing_uuid{3, 1};
    const auto red = ImportMaterial("NEXORA_MATERIAL 1 base_color 1 0 0 metallic 0.2 "
                                    "roughness 0.7 occlusion 0.8 emission 0.3 0 0");
    const auto green = ImportMaterial("NEXORA_MATERIAL 1 base_color 0 1 0 metallic 0 "
                                      "roughness 0.5 occlusion 1 emission 0 0.4 0");
    const auto imported = ImportObjMesh("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    Require(red.material && green.material && imported.geometry, "typed sources failed");
    auto red_source = std::make_shared<const MaterialAsset>(*red.material);
    auto green_source = std::make_shared<const MaterialAsset>(*green.material);
    ContentBrowserModel browser;
    Require(
        browser.Reset(
            std::array{ContentItem{mesh_uuid, "Triangle.obj", ".obj", "mesh", ThumbnailState::Ready,
                                   std::make_shared<const MeshGeometry>(*imported.geometry)},
                       ContentItem{red_uuid,
                                   "Paint.nmaterial",
                                   ".nmaterial",
                                   "red",
                                   ThumbnailState::Ready,
                                   {},
                                   red_source}},
            7),
        "typed content failed");
    MeshAssetCatalog meshes;
    MaterialAssetCatalog catalog;
    Require(meshes.PublishContent(browser) && catalog.PublishContent(browser), "catalog failed");
    runtime::World world;
    const auto scene_id = world.LoadScene("Frozen Game materials");
    Require(world.Activate(scene_id), "activation failed");
    SceneDocument document(world, scene_id);
    const auto camera = document.Create("Camera");
    Require(document.SetCamera(*document.Key(camera), runtime::CameraComponent{}) &&
                document.SetTransform(camera, {0, 0, 5}),
            "camera setup failed");
    std::vector<runtime::Id> ids;
    for (unsigned index = 0; index < 4; ++index) {
      const auto id = document.Create("Mesh");
      ids.push_back(id);
      Require(
          document.SetMeshRenderer(*document.Key(id),
                                   runtime::MeshComponent{MeshResourceId(mesh_uuid), {UINT64_MAX}}),
          "mesh setup failed");
      if (index != 3)
        Require(
            document.SetOpaqueComponent(
                *document.Key(id), MaterialAssetReference(index == 2 ? missing_uuid : red_uuid)),
            "assignment failed");
    }
    const auto baseline = document.PrepareSave();
    Require(baseline.has_value(), "scene preparation failed");
    Require(!preview::FreezeGameMaterials(document, catalog, 0) &&
                !preview::FreezeGameMaterials(document, catalog, 6),
            "stale/zero generation accepted");
    auto frozen = preview::FreezeGameMaterials(document, catalog, 7);
    Require(frozen && frozen->materials.size() == 2 && frozen->unavailable == 1 &&
                frozen->entities.at(ids[0]) == frozen->entities.at(ids[1]) &&
                !frozen->entities.contains(camera) &&
                document.PrepareSave()->Bytes() == baseline->Bytes(),
            "frozen assignment dedup, camera filtering or authoring isolation failed");
    const MeshAssetCatalog frozen_meshes = meshes;
    runtime::PlaySession play(world);
    Require(play.Start(1.0 / 60.0,
                       [id = ids[0]](runtime::World &clone, double) {
                         auto pose = clone.FindEntity(id)->transform;
                         ++pose.x;
                         runtime::WorldCommandBuffer move;
                         move.SetTransform(id, pose);
                         return move.Apply(clone);
                       }),
            "Play failed");
    const auto frame = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), frozen_meshes, 2,
                                               camera, &*frozen);
    Require(frame.pbr && frame.materials.size() == 2 && frame.instances.size() == 4 &&
                frame.unavailable_materials == 1 && frame.batches[0].materialIndex == 1 &&
                frame.batches[1].materialIndex == 1 && frame.batches[2].materialIndex == 0 &&
                frame.batches[3].materialIndex == 0 && frame.camera_position[2] == 5 &&
                frame.materials[1].baseColor == std::array<float, 4>{1, 0, 0, 1} &&
                frame.materials[1].metallic == .2F && frame.materials[1].roughness == .7F &&
                frame.materials[1].occlusion == .8F && frame.materials[1].emission[0] == .3F,
            "Game PBR scalar values, neutral fallbacks or camera position failed");
    for (const auto &vertex : frame.geometry.vertices)
      Require(std::isfinite(vertex.tangent[0]) && std::isfinite(vertex.tangent[1]) &&
                  std::isfinite(vertex.tangent[2]) && std::abs(vertex.tangent[3]) == 1,
              "degenerate UV fallback tangent failed");
    Require(browser.PublishArtifact(red_uuid, "green", ThumbnailState::Ready, nullptr, {},
                                    green_source) &&
                catalog.PublishContent(browser),
            "reimport failed");
    Require(
        document.SetOpaqueComponent(*document.Key(ids[0]), MaterialAssetReference(missing_uuid)),
        "reassignment failed");
    auto changed = preview::FreezeGameMaterials(document, catalog, 7);
    Require(changed && changed->entities.at(ids[0]) == 0 && changed->materials[1].baseColor[1] == 1,
            "new snapshot did not observe current authoring/reimport");
    Require(play.Tick() && play.Pause() && play.Step(), "pause/step failed");
    auto retained = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), frozen_meshes, 2,
                                            camera, &*frozen);
    Require(retained.pbr && retained.materials[1].baseColor[0] == 1 &&
                retained.batches[0].materialIndex == 1 &&
                retained.instances[0].model_transform->at(3) == 2 &&
                world.FindEntity(ids[0])->transform.x == 0 &&
                play.PlayWorld()->FindEntity(ids[0])->mesh_data.material.shader == UINT64_MAX,
            "reimport/reassignment escaped snapshot or legacy shader/isolation failed");
    auto bad_tangents = *imported.geometry;
    for (auto &vertex : bad_tangents.vertices)
      vertex.normal = {};
    Require(browser.PublishArtifact(mesh_uuid, "no-tangents", ThumbnailState::Ready, nullptr,
                                    std::make_shared<const MeshGeometry>(bad_tangents)) &&
                meshes.PublishContent(browser),
            "tangent failure source publication failed");
    const auto fallback =
        preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), meshes, 2, camera, &*frozen);
    Require(!fallback.pbr && fallback.materials.empty() && fallback.instances.size() == 4 &&
                fallback.geometry.vertices.size() == 3 && fallback.unavailable_materials == 3 &&
                std::ranges::all_of(fallback.batches,
                                    [](const auto &batch) { return batch.materialIndex == 0; }),
            "tangent failure discarded geometry or retained partially prepared PBR slots");
    Require(browser.Delete(std::array{red_uuid}) && catalog.PublishContent(browser),
            "source deletion failed");
    auto deleted = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), frozen_meshes, 2,
                                           camera, &*frozen);
    Require(deleted.pbr && deleted.materials[1].baseColor[0] == 1,
            "deleted source invalidated Play values");
    changed = preview::FreezeGameMaterials(document, catalog, 7);
    Require(changed && !changed->authored && changed->unavailable == 3,
            "next Play did not expose missing material fallback");
    auto new_id = play.PlayWorld()->CreateEntity(scene_id).id;
    runtime::WorldCommandBuffer create;
    create.SetMeshRenderer(new_id, runtime::MeshComponent{MeshResourceId(mesh_uuid), {91}});
    Require(create.Apply(*play.PlayWorld()), "dynamic mesh failed");
    const auto dynamic = preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), frozen_meshes,
                                                 2, camera, &*frozen);
    Require(dynamic.instances.size() == 5 && dynamic.batches.back().materialIndex == 0 &&
                dynamic.unavailable_materials == 1,
            "new Play entity invented an authored material assignment");
    Require(play.Stop(), "Stop failed");
    frozen.reset();
    catalog.Clear();
    browser = ContentBrowserModel{};
    const auto owned = retained.DrawData({0, 0, 640, 360});
    Require(owned.pbr && owned.materials[1].baseColor[0] == 1 && owned.cameraPosition[2] == 5 &&
                owned.vertices.size() == 3 && owned.instances[0].model_transform->at(3) == 2 &&
                Nexora::Presentation::ValidateSceneMaterials(owned.materials, owned.batches),
            "Game frame borrowed a destroyed palette/catalog/Play World");

    // Native palette admits 63 distinct authored materials plus neutral slot zero.
    std::vector<ContentItem> items;
    for (unsigned index = 1; index <= 65; ++index)
      items.push_back({{8, index},
                       std::to_string(index) + ".nmaterial",
                       ".nmaterial",
                       "red",
                       ThumbnailState::Ready,
                       {},
                       red_source});
    Require(browser.Reset(items, 9) && catalog.PublishContent(browser), "budget catalog failed");
    runtime::World budget_world;
    const auto budget_scene = budget_world.LoadScene("Budget");
    SceneDocument budget(budget_world, budget_scene);
    std::vector<runtime::Id> budget_ids;
    for (unsigned index = 1; index <= 65; ++index) {
      const auto id = budget.Create("Material mesh");
      budget_ids.push_back(id);
      Require(budget.SetMeshRenderer(*budget.Key(id), runtime::MeshComponent{}) &&
                  budget.SetOpaqueComponent(*budget.Key(id), MaterialAssetReference({8, index})),
              "budget assignment failed");
    }
    const auto bounded = preview::FreezeGameMaterials(budget, catalog, 9);
    Require(bounded && bounded->materials.size() == 64 && bounded->unavailable == 2 &&
                bounded->unavailable_entities.contains(budget_ids[63]) &&
                bounded->entities.at(budget_ids[64]) == 0,
            "frozen native material budget exceeded or hid unavailable assignments");
    std::cout << "PASS: frozen Game PBR ownership, reimport/delete/reassignment, fallback, "
                 "camera, tangents, Play isolation and palette budget\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
