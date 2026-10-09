#include "MaterialScenePreview.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool ok, const char *message) {
  if (!ok)
    throw std::runtime_error(message);
}
} // namespace
int main() {
  try {
    using namespace nexora;
    const auto parsed = editor::ImportMaterial(
        "NEXORA_MATERIAL 1 base_color 1 0 0 metallic 0 roughness 0.5 occlusion 1 emission 0 0 0");
    Require(parsed.material.has_value(), "material catalog source failed");
    auto material = std::make_shared<const editor::MaterialAsset>(*parsed.material);
    std::vector<editor::ContentItem> items;
    for (unsigned i = 1; i <= 65; ++i)
      items.push_back({{1, i},
                       std::to_string(i) + ".nmaterial",
                       ".nmaterial",
                       "old",
                       editor::ThumbnailState::Ready,
                       {},
                       material});
    editor::ContentBrowserModel content;
    Require(content.Reset(items, 7), "material catalog content failed");
    editor::MaterialAssetCatalog catalog;
    Require(catalog.PublishContent(content), "material catalog publication failed");
    const auto retained = catalog.ResolveAsset({1, 1}, 7);
    Require(retained && !catalog.ResolveAsset({1, 1}, 6) && !catalog.ResolveAsset({}, 7),
            "material catalog generation failed");
    auto invalid = *material;
    invalid.base_color[0] = std::numeric_limits<float>::quiet_NaN();
    items[0].material = std::make_shared<const editor::MaterialAsset>(invalid);
    Require(content.Reset(items, 8) && !catalog.PublishContent(content) &&
                catalog.Generation() == 7 &&
                catalog.ResolveAsset({1, 1}, 7)->material == retained->material,
            "invalid typed material changed the last good catalog");
    for (unsigned mutation = 0; mutation < 5; ++mutation) {
      auto divergent = *material;
      if (mutation == 0)
        divergent.schema.shader_id = "unsupported.shader";
      if (mutation == 1)
        divergent.schema.shader_profile = "unsupported.profile";
      if (mutation == 2)
        divergent.schema.shading_model = renderer::ShadingModel::Unlit;
      if (mutation == 3)
        divergent.schema.features = renderer::FeatureBit(renderer::MaterialFeature::IBL);
      if (mutation == 4)
        divergent.schema.parameters[0].value = std::array<float, 3>{0, 1, 0};
      items[0].material = std::make_shared<const editor::MaterialAsset>(divergent);
      Require(content.Reset(items, 8) && !catalog.PublishContent(content) &&
                  catalog.Generation() == 7 &&
                  catalog.ResolveAsset({1, 1}, 7)->material == retained->material,
              "unsupported/divergent schema replaced the live canonical material");
    }
    items[0].material = material;
    Require(content.Reset(items, 7), "material catalog content restore failed");
    runtime::World world;
    const auto id = world.LoadScene("Palette budget");
    Require(world.Activate(id), "material palette scene failed");
    editor::SceneDocument scene(world, id);
    std::vector<runtime::Id> entities;
    for (unsigned i = 1; i <= 65; ++i) {
      const auto entity = scene.Create("Material object");
      entities.push_back(entity);
      Require(scene.SetOpaqueComponent(*scene.Key(entity), editor::MaterialAssetReference({1, i})),
              "palette reference failed");
    }
    Require(scene.SetOpaqueComponent(
                *scene.Key(entities[0]),
                {991, "Large unrelated plugin", std::vector<std::uint8_t>(1024 * 1024, 72)}),
            "large opaque plugin fixture failed");
    Require(editor::ReadMaterialAssetReference(scene, *scene.Key(entities[0])) ==
                runtime::AssetUuid{1, 1},
            "unrelated opaque payload prevented a bounded material reference lookup");
    auto oversized_reference = editor::MaterialAssetReference({1, 1});
    const editor::OpaqueComponentInfo deceptive{oversized_reference.type,
                                                oversized_reference.type_name, 1024 * 1024,
                                                oversized_reference.data, entities[0]};
    Require(!editor::ReadMaterialAssetReference(deceptive),
            "oversized opaque prefix became a valid material reference");
    const auto before = world.SaveScene(id);
    const auto palette = editor::preview::PrepareMaterialPalette(scene, catalog, 7, entities);
    Require(palette.authored && palette.materials.size() == 64 && palette.unavailable == 2 &&
                palette.entities.at(entities[62]) == 63 && palette.entities.at(entities[63]) == 0 &&
                palette.entities.at(entities[64]) == 0 && world.SaveScene(id) == before,
            "palette budget rewrote references or exceeded native slots");
    Require(
        scene.SetOpaqueComponent(*scene.Key(entities[1]), editor::MaterialAssetReference({1, 1})),
        "duplicate reference failed");
    const auto duplicate =
        editor::preview::PrepareMaterialPalette(scene, catalog, 7, std::span(entities).first(2));
    Require(duplicate.materials.size() == 2 &&
                duplicate.entities.at(entities[0]) == duplicate.entities.at(entities[1]),
            "palette did not deduplicate UUIDs");
    auto unsupported = editor::MaterialAssetReference({1, 1});
    unsupported.data[0] = 2;
    Require(scene.SetOpaqueComponent(*scene.Key(entities[0]), unsupported) &&
                !editor::ReadMaterialAssetReference(scene, *scene.Key(entities[0])) &&
                scene.OpaqueComponents(*scene.Key(entities[0]))->front() == unsupported,
            "unsupported reference was interpreted or changed");
    editor::preview::Geometry bad;
    bad.vertices.resize(3);
    bad.indices = {0, 1, 2};
    const auto untouched = bad.vertices[0].tangent[0];
    Require(!editor::preview::PrepareMaterialTangents(bad) &&
                bad.vertices[0].tangent[0] == untouched,
            "invalid tangent conversion partially changed geometry");
    catalog.Clear();
    Require(!catalog.ResolveAsset({1, 1}, 7) && retained->material->base_color[0] == 1,
            "unload invalidated an owning material snapshot");
    std::cout << "PASS: material catalog atomicity, generations, snapshot ownership, palette "
                 "bounds/dedup and unknown references\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
