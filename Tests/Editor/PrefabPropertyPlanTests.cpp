#include "Nexora/Editor/PrefabPropertyPlan.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Assets = editor::PrefabAssets;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  runtime::World source_world, target_world, result_world;
  const auto source_scene = source_world.LoadScene("Source identity");
  static_cast<void>(target_world.LoadScene("Unrelated"));
  const auto target_scene = target_world.LoadScene("Preserved target", true);
  editor::SceneDocument source(source_world, source_scene), target(target_world, target_scene);
  const auto root = source.Create("  Source root"),
             child = source.CreateCamera("Source child", root);
  const auto target_root = target.Create("Edited root"),
             target_child = target.Create("Edited child");
  Require(root != target_root && source.SetTransform(root, {1, 2, 3}) &&
              source.SetEulerField(std::array{*source.Key(root)}, 1, 720) &&
              source.SetLight(*source.Key(root), runtime::LightComponent{2.5F}) &&
              source.SetMeshRenderer(*source.Key(child), runtime::MeshComponent{91, {92}}) &&
              source.SetOpaqueComponent(*source.Key(child), {99, "Missing provider", {0, 255, 27}}),
          "Rich remapping fixture failed");
  std::uint64_t next = 100;
  const auto identity = [&] { return foundation::Uuid{901, ++next}; };
  const auto asset = Assets::Capture({902, 1}, source, identity);
  auto current = Assets::Capture({902, 2}, target, identity);
  Require(asset && current && asset->nodes.size() == current->nodes.size(), "Capture failed");
  for (std::size_t i = 0; i < current->nodes.size(); ++i) {
    const auto id = current->nodes[i].serialized_node;
    current->nodes[i] = asset->nodes[i];
    current->nodes[i].serialized_node = id;
  }
  // Current opaque field identities may be absent even though the complete source restores them.
  Require(!Assets::Validate(*current), "Missing-property metadata fixture unexpectedly valid");
  for (auto &node : current->nodes) {
    std::erase_if(node.properties,
                  [](const auto &field) { return field.field.starts_with("opaque/"); });
  }
  Require(Assets::Validate(*current), "Current stable mapping failed");
  const std::string original = target.PrepareSave()->Bytes();
  const auto prepared = editor::BuildPrefabPropertySnapshot(*current, *asset);
  Require(prepared && target.PrepareSave()->Bytes() == original,
          "Planning changed live target or did not remap");
  const auto result_scene = result_world.LoadScene("Result");
  editor::SceneDocument result(result_world, result_scene);
  Require(result.ReloadBytes(*prepared) &&
              result_world.FindScene(result_scene)->name == "Preserved target" &&
              result_world.FindScene(result_scene)->persistent && result.Nodes().size() == 2 &&
              result.Name(target_root) == "  Source root" &&
              result.Name(target_child) == "Source child" &&
              result.Parent(target_child) == target_root &&
              result.Transform(target_root) == source.Transform(root) &&
              result.EulerAngles(target_root) == source.EulerAngles(root) &&
              result.Camera(*result.Key(target_child)).has_value() &&
              result.Light(*result.Key(target_root))->intensity == 2.5F &&
              result.MeshRenderer(*result.Key(target_child))->mesh == 91 &&
              result.OpaqueComponents(*result.Key(target_child)) ==
                  source.OpaqueComponents(*source.Key(child)),
          "Complete candidate lost scene identity, remapped fields or unknown bytes");
  auto bad = *asset;
  bad.nodes[0].id = {903, 1};
  Require(Assets::Validate(bad) && !editor::BuildPrefabPropertySnapshot(*current, bad),
          "Mismatched stable identity set planned");
  bad = *asset;
  bad.scene_bytes = "corrupt";
  Require(!editor::BuildPrefabPropertySnapshot(*current, bad), "Corrupt source planned");
  const auto added = source.Create("Additional identity");
  const auto changed_shape = Assets::Capture(asset->id, source, identity, &*asset);
  Require(added && changed_shape &&
              !editor::BuildPrefabPropertySnapshot(*current, *changed_shape) &&
              target.PrepareSave()->Bytes() == original,
          "Structural mismatch planned partial replacement");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Prefab property revert plans passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
