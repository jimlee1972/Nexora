#include "Nexora/Editor/PrefabPropertyPlan.h"
#include <algorithm>
#include <iostream>
#include <sstream>
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
  // Give the target independent edits/components, retaining stable identities for shared fields.
  Require(
      target.SetTransform(target_root, {8, 9, 10, 0, 0, 0, 1, 2, 3, 4}) &&
          target.SetEulerField(std::array{*target.Key(target_root)}, 1, 360) &&
          target.SetOpaqueComponent(*target.Key(target_child), {99, "Edited provider", {27, 0}}) &&
          target.SetOpaqueComponent(*target.Key(target_child), {100, "Unselected provider", {88}}),
      "Targeted independent fields fixture failed");
  current = Assets::Capture(current->id, target, identity);
  Require(current.has_value(), "Targeted capture failed");
  for (std::size_t i = 0; i < current->nodes.size(); ++i) {
    current->nodes[i].id = asset->nodes[i].id;
    for (auto &field : current->nodes[i].properties) {
      const auto old = std::ranges::find(asset->nodes[i].properties, field.field,
                                         &editor::PrefabPropertyIdentity::field);
      if (old != asset->nodes[i].properties.end())
        field.id = old->id;
    }
  }
  auto inactive = *asset;
  const auto marker = inactive.scene_bytes.find("world\n");
  Require(marker != std::string::npos, "Official source world marker missing");
  std::istringstream runtime(inactive.scene_bytes.substr(marker + 6));
  std::string line, rewritten = inactive.scene_bytes.substr(0, marker + 6);
  while (std::getline(runtime, line)) {
    if (line.starts_with(std::to_string(child) + " ")) {
      std::istringstream row(line);
      std::vector<std::string> values;
      std::string value;
      while (row >> value)
        values.push_back(value);
      Require(values.size() == 21, "Official source record framing changed");
      values[12] = "0";
      values[15] = "75";
      line.clear();
      for (const auto &token : values) {
        if (!line.empty())
          line += ' ';
        line += token;
      }
    }
    rewritten += line + '\n';
  }
  inactive.scene_bytes = rewritten;
  Require(Assets::Validate(*current) && Assets::Validate(inactive), "Targeted metadata invalid");
  const auto select = [&](std::size_t node, std::string_view field) {
    const auto property = std::ranges::find(asset->nodes[node].properties, field,
                                            &editor::PrefabPropertyIdentity::field);
    Require(property != asset->nodes[node].properties.end(), "Stable field fixture missing");
    return editor::PrefabPropertySelection{asset->nodes[node].id, property->id};
  };
  const std::array selection{select(0, "name"), select(0, "transform.position"),
                             select(1, "camera"), select(1, "opaque/99")};
  const auto before_targeted = target.PrepareSave()->Bytes();
  const auto selected = editor::BuildPrefabPropertySnapshot(*current, inactive, selection);
  Require(selected && result.ReloadBytes(*selected) &&
              result.Name(target_root) == "  Source root" &&
              result.Name(target_child) == "Edited child" &&
              result.Transform(target_root)->x == 1 && result.Transform(target_root)->y == 2 &&
              result.Transform(target_root)->sx == 2 && result.Transform(target_root)->sy == 3 &&
              result.EulerAngles(target_root) == target.EulerAngles(target_root) &&
              result.Parent(target_child) == 0 && !result.Camera(*result.Key(target_child)) &&
              result_world.FindEntity(target_child)->camera_data.vertical_field_of_view == 75 &&
              !result.Light(*result.Key(target_root)) &&
              !result.MeshRenderer(*result.Key(target_child)),
          "Targeted candidate lost inactive stored data or changed an unselected field");
  const auto unknown = result.OpaqueComponents(*result.Key(target_child));
  Require(unknown && unknown->size() == 2 && (*unknown)[0].type == 99 &&
              (*unknown)[0].type_name == "Missing provider" &&
              (*unknown)[0].data == std::vector<std::uint8_t>{0, 255, 27} &&
              (*unknown)[1].type == 100 && (*unknown)[1].data == std::vector<std::uint8_t>{88} &&
              target.PrepareSave()->Bytes() == before_targeted,
          "Targeted opaque replacement changed unrelated bytes or live document");
  const auto removable = std::ranges::find(current->nodes[1].properties, "opaque/100",
                                           &editor::PrefabPropertyIdentity::field);
  Require(removable != current->nodes[1].properties.end(), "Removable stable field missing");
  const std::array remove{editor::PrefabPropertySelection{current->nodes[1].id, removable->id}};
  const auto removed = editor::BuildPrefabPropertySnapshot(*current, inactive, remove);
  Require(removed && result.ReloadBytes(*removed) &&
              result.OpaqueComponents(*result.Key(target_child))->size() == 1 &&
              result.OpaqueComponents(*result.Key(target_child))->front().type == 99 &&
              result.OpaqueComponents(*result.Key(target_child))->front().data ==
                  std::vector<std::uint8_t>{27, 0},
          "Selected absent-source opaque removal changed unrelated payloads");
  const std::array duplicates{selection[0], selection[0]};
  const std::array wrong{editor::PrefabPropertySelection{asset->nodes[0].id, {999, 1}}};
  const std::array reserved{select(0, "layer")};
  Require(!editor::BuildPrefabPropertySnapshot(*current, inactive, duplicates) &&
              !editor::BuildPrefabPropertySnapshot(*current, inactive, wrong) &&
              !editor::BuildPrefabPropertySnapshot(*current, inactive, reserved) &&
              editor::BuildPrefabPropertySnapshot(
                  *current, inactive, std::span<const editor::PrefabPropertySelection>{}) ==
                  before_targeted,
          "Duplicate, unknown/reserved field or empty selection changed properties");
  const std::array rotation{select(0, "transform.rotation")};
  const auto rotation_only = editor::BuildPrefabPropertySnapshot(*current, inactive, rotation);
  Require(rotation_only && result.ReloadBytes(*rotation_only) &&
              result.EulerAngles(target_root) == source.EulerAngles(root) &&
              result.Transform(target_root)->x == 8 && result.Transform(target_root)->sx == 2,
          "Rotation-only selection lost authored revolutions or unrelated TRS");
  const std::array parent_only{select(1, "parent")};
  const auto hierarchy_only = editor::BuildPrefabPropertySnapshot(*current, inactive, parent_only);
  Require(hierarchy_only && result.ReloadBytes(*hierarchy_only) &&
              result.Parent(target_child) == target_root &&
              result.Transform(target_root) == target.Transform(target_root) &&
              result.Transform(target_child) == target.Transform(target_child) &&
              result.Name(target_root) == "Edited root",
          "Parent-only selection changed local transforms or unrelated names");
  Require(target.Reparent(target_root, target_child), "Inverted hierarchy fixture failed");
  const auto inverted = Assets::Capture(current->id, target, identity, &*current);
  const auto inverted_bytes = target.PrepareSave()->Bytes();
  const std::array all_parents{select(0, "parent"), select(1, "parent")};
  const auto compatible =
      inverted ? editor::BuildPrefabPropertySnapshot(*inverted, inactive, all_parents)
               : std::nullopt;
  Require(
      inverted && !editor::BuildPrefabPropertySnapshot(*inverted, inactive, parent_only) &&
          compatible && result.ReloadBytes(*compatible) && result.Parent(target_root) == 0 &&
          result.Parent(target_child) == target_root &&
          target.PrepareSave()->Bytes() == inverted_bytes && target.Undo() &&
          target.PrepareSave()->Bytes() == before_targeted,
      "Incompatible parent subset planned a cycle, changed live state or rejected a valid group");
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
              target.PrepareSave()->Bytes() == before_targeted,
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
