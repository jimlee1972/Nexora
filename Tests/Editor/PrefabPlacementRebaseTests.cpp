#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/PrefabPlacementRebase.h"
#include "Nexora/Editor/ProjectPrefabPlacement.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
using namespace nexora;
using Rebase = editor::PrefabPlacementRebase;
using Decision = editor::PrefabPlacementRebaseDecision;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::map<std::filesystem::path, std::string> Files(const std::filesystem::path &root) {
  std::map<std::filesystem::path, std::string> result;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file()) {
      std::ifstream input(entry.path(), std::ios::binary);
      result.emplace(entry.path().lexically_relative(root),
                     std::string{std::istreambuf_iterator<char>(input), {}});
    }
  return result;
}
void Dormant(runtime::World &world, runtime::Id scene, runtime::Id target) {
  std::istringstream input(*world.SaveScene(scene));
  std::string line, snapshot;
  Require(static_cast<bool>(std::getline(input, line)), "Dormant source header absent");
  snapshot = line + '\n';
  while (std::getline(input, line)) {
    std::istringstream row(line);
    std::vector<std::string> fields;
    for (std::string field; row >> field;)
      fields.push_back(std::move(field));
    Require(fields.size() == 21, "Dormant canonical schema changed");
    if (fields[0] == std::to_string(target)) {
      fields[15] = "91";
      fields[16] = "0.2";
      fields[17] = "950";
    }
    for (std::size_t i = 0; i < fields.size(); ++i)
      snapshot += (i ? " " : "") + fields[i];
    snapshot += '\n';
  }
  Require(world.ReplaceSceneSnapshot(scene, snapshot), "Dormant source rejected");
}
void Write(const std::filesystem::path &path, const editor::PrefabAsset &asset) {
  const auto bytes = editor::PrefabAssets::Encode(asset);
  Require(bytes.has_value(), "Replacement asset invalid");
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char *>(bytes->data()),
               static_cast<std::streamsize>(bytes->size()));
  Require(static_cast<bool>(output), "Replacement asset write failed");
}
void Run() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-placement-rebase-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace writer, reader;
  Require(writer.Create(root, "Rebase"), "Workspace failed");
  runtime::World source_world, nested_world, world;
  const auto source_scene = source_world.LoadScene("Source");
  editor::SceneDocument source(source_world, source_scene);
  editor::SceneDocument nested(nested_world, nested_world.LoadScene("Nested"));
  editor::SceneDocument scene(world, world.LoadScene("Main"));
  const auto source_node = source.Create("Source name"), nested_node = nested.Create("Nested one");
  Require(source.SetOpaqueComponent(*source.Key(source_node), {91, "Unavailable", {0, 255, 27}}) &&
              source.SetEulerField(std::array{*source.Key(source_node)}, 2, 37.5),
          "Retained source failed");
  std::uint64_t serial{};
  const auto factory = [&] { return foundation::Uuid{8920, ++serial}; };
  auto original = editor::PrefabAssets::Capture({8921, 1}, source, factory);
  const auto old_nested = editor::PrefabAssets::Capture({8921, 2}, nested, factory);
  Require(original && old_nested && editor::PrefabAssets::Publish(writer, *old_nested) &&
              nested.Rename(*nested.Key(nested_node), "Nested two"),
          "Nested revision one failed");
  const auto new_nested =
      editor::PrefabAssets::Capture(old_nested->id, nested, factory, &*old_nested);
  Require(new_nested && editor::PrefabAssets::Publish(writer, *new_nested, &*old_nested),
          "Nested revision two failed");
  original->nested = {{{8923, 1}, original->nodes.front().id, {old_nested->id, 1}},
                      {{8923, 2}, original->nodes.front().id, {old_nested->id, 2}}};
  Require(editor::PrefabAssets::Publish(writer, *original), "Root source publication failed");
  const auto placement = editor::ProjectPrefabPlacement::Prepare(writer, original->id, scene);
  const auto placed =
      placement
          ? editor::ProjectPrefabPlacement::Instantiate(writer, scene, *placement, {8922, 1}, true)
          : std::nullopt;
  Require(placed && placed->size() == 3, "Mixed scoped placement failed");
  const auto target = placed->front().target;
  const auto nested_target = std::ranges::find_if(*placed, [](const auto &node) {
    return node.scope == std::vector<foundation::Uuid>{{8923, 1}};
  });
  Require(nested_target != placed->end(), "Retained scoped node absent");
  auto local = *scene.Transform(target.id);
  local.x = 9;
  Require(scene.Rename(target, "Local name") && scene.SetTransform(target.id, local) &&
              scene.SetEulerField(std::array{target}, 1, 720) &&
              scene.SetCamera(target, runtime::CameraComponent{85, .3, 600}) &&
              scene.SetOpaqueComponent(target, {92, "Locally.Added", {3, 255, 19}}) &&
              scene.Rename(nested_target->target, "Local nested") &&
              scene.Select(std::array{target}) && scene.CopySelection() &&
              scene.Save(root / "Main.scene"),
          "Local override/baseline failed");
  const auto before = *scene.PrepareSave();
  const auto generation = scene.Generation();
  auto remote = *source.Transform(source_node);
  remote.y = 7;
  remote.sx = 2.5;
  remote.sy = -3.25;
  remote.sz = .5;
  Require(
      source.Rename(*source.Key(source_node), "Published name") &&
          source.SetTransform(source_node, remote) &&
          source.SetOpaqueComponent(*source.Key(source_node), {91, "Unavailable", {0, 255, 28}}),
      "Published complete-group changes failed");
  Dormant(source_world, source_scene, source_node);
  const auto advanced = editor::PrefabAssets::Capture(original->id, source, factory, &*original);
  Require(advanced && editor::PrefabAssets::Publish(writer, *advanced, &*original) &&
              reader.Open(root, editor::ProjectAccess::ReadOnly),
          "Newer source publication failed");
  const auto files = Files(root);
  const auto review = Rebase::Prepare(reader, scene, target);
  Require(review && !review->Ready() && review->Unresolved() == 3 &&
              review->Source().revision == 1 && review->PublishedSource().revision == 2 &&
              scene.MatchesPreparedSave(before) && Files(root) == files,
          "Owning read-only rebase report lost scope/conflicts or mutated files");
  const auto index = [&](std::string_view group) {
    const auto found = std::ranges::find_if(review->Rows(), [&](const auto &row) {
      return row.target == target && row.group == group;
    });
    Require(found != review->Rows().end() && found->property && found->conflict,
            "Expected stable complete property conflict absent");
    return static_cast<std::size_t>(found - review->Rows().begin());
  };
  const std::array<editor::PrefabPlacementRebaseChoice, 3> choices{
      {{index("name"), Decision::KeepLocal},
       {index("transform.position"), Decision::TakeSource},
       {index("camera"), Decision::TakeSource}}};
  const auto resolved = Rebase::Resolve(*review, choices);
  Require(resolved && resolved->Ready() && resolved->Unresolved() == 0 && !review->Ready() &&
              scene.MatchesPreparedSave(before) && Files(root) == files,
          "Pure owning conflict resolution mutated original review/document/files");
  const std::array<editor::PrefabPlacementRebaseChoice, 2> duplicate{choices[0], choices[0]};
  const std::array<editor::PrefabPlacementRebaseChoice, 1> invalid{
      {{review->Rows().size(), Decision::KeepLocal}}};
  Require(!Rebase::Resolve(*review, duplicate) && !Rebase::Resolve(*review, invalid) &&
              !Rebase::Apply(writer, scene, *review, true) &&
              !Rebase::Apply(reader, scene, *resolved, true) &&
              !Rebase::Apply(writer, scene, *resolved, false) &&
              scene.MatchesPreparedSave(before) && Files(root) == files,
          "Invalid/unresolved/read-only/unauthorized rebase changed state");
  Require(scene.Rename(target, "Stale local") && !Rebase::Matches(writer, scene, *resolved) &&
              !Rebase::Apply(writer, scene, *resolved, true) && scene.Undo() &&
              scene.MatchesPreparedSave(before),
          "Stale target rebase lost authoring or history");
  const auto rejected = [&] {
    Require(!Rebase::Matches(writer, scene, *resolved) &&
                !Rebase::Apply(writer, scene, *resolved, true) && scene.MatchesPreparedSave(before),
            "Changed source/project rebase mutated live history");
  };
  const auto recovery = root / ".nexora/workspace.recovery";
  {
    std::ofstream output(recovery);
    output << "pending";
  }
  rejected();
  std::filesystem::remove(recovery);
  const auto workspace_state = root / ".nexora/workspace";
  const auto timestamp = std::filesystem::last_write_time(workspace_state);
  std::filesystem::last_write_time(workspace_state, timestamp + std::chrono::seconds(10));
  rejected();
  std::filesystem::last_write_time(workspace_state, timestamp);
  const auto archive = root / ".nexora/prefabs/revisions" / original->id.ToString() / "1.nxprefab";
  std::filesystem::remove(archive);
  rejected();
  Write(archive, *original);
  auto tampered = *original;
  tampered.scene_bytes = advanced->scene_bytes;
  Write(archive, tampered);
  rejected();
  Write(archive, *original);
  const auto nonconflict =
      std::ranges::find_if(review->Rows(), [](const auto &row) { return !row.conflict; });
  Require(nonconflict != review->Rows().end(), "Source-only row absent");
  const std::array<editor::PrefabPlacementRebaseChoice, 1> unnecessary{
      {{static_cast<std::size_t>(nonconflict - review->Rows().begin()), Decision::TakeSource}}};
  const std::array<editor::PrefabPlacementRebaseChoice, 1> bad_decision{
      {{choices.front().row, static_cast<Decision>(255)}}};
  Require(!Rebase::Resolve(*review, unnecessary) && !Rebase::Resolve(*review, bad_decision) &&
              Files(root) == files && Rebase::Matches(writer, scene, *resolved),
          "Invalid choices or restored sources changed owning authority");
  const auto current_path = root / ".nexora/prefabs" / (original->id.ToString() + ".nxprefab");
  auto replacement = *advanced;
  replacement.scene_bytes = original->scene_bytes;
  Write(current_path, replacement);
  rejected();
  replacement = *advanced;
  ++replacement.revision;
  Write(current_path, replacement);
  rejected();
  replacement = *advanced;
  replacement.nested.front().source.revision = 2;
  Write(current_path, replacement);
  rejected();
  Require(!Rebase::Prepare(writer, scene, target),
          "Changed scoped dependency references produced a partial rebase");
  replacement = *advanced;
  replacement.nodes.front().properties.front().id = {8929, 77};
  Write(current_path, replacement);
  rejected();
  Require(!Rebase::Prepare(writer, scene, target),
          "Changed stable property identity produced a partial rebase");
  Write(current_path, *advanced);
  Require(Rebase::Matches(writer, scene, *resolved) && Files(root) == files,
          "Restored current source failed exact observation revalidation");
  Require(Rebase::Apply(writer, scene, *resolved, true), "Resolved current rebase rejected");
  const auto after = *scene.PrepareSave();
  const auto *entity = world.FindEntity(target.id);
  Require(scene.PrefabPlacements().front().revision == 2 && scene.Name(target.id) == "Local name" &&
              scene.Name(nested_target->target.id) == "Local nested" && entity->transform.x == 0 &&
              entity->transform.y == 7 && entity->transform.sx == 2.5 &&
              entity->transform.sy == -3.25 && entity->transform.sz == .5 && !entity->camera &&
              entity->camera_data.vertical_field_of_view == 91 &&
              scene.EulerAngles(target.id)->at(1) == 720 && scene.Key(target.id) == target &&
              scene.Generation() == generation && scene.Dirty() && Files(root) == files,
          "Rebase mixed complete groups, lost local/scoped/dormant values or wrote files");
  const auto opaque = scene.OpaqueComponents(target);
  const auto source_opaque =
      std::ranges::find(*opaque, runtime::TypeId{91}, &editor::OpaqueComponent::type);
  Require(source_opaque != opaque->end() &&
              source_opaque->data == std::vector<std::uint8_t>({0, 255, 28}) &&
              std::ranges::find(*opaque, runtime::TypeId{92}, &editor::OpaqueComponent::type) !=
                  opaque->end(),
          "Rebase lost source-only opaque update or unknown local addition");
  for (int i = 0; i < 20; ++i)
    Require(scene.Undo() && scene.MatchesPreparedSave(before) && !scene.Dirty() &&
                scene.PrefabPlacements().front().revision == 1 && scene.Redo() &&
                scene.MatchesPreparedSave(after) && scene.PrefabPlacements().front().revision == 2,
            "Rebase property/revision history diverged");
  Require(!Rebase::Prepare(writer, scene, target) && scene.Paste() && scene.Undo() &&
              scene.MatchesPreparedSave(after) && scene.Save(root / "Reopened.scene"),
          "Up-to-date review changed history/clipboard or save failed");
  runtime::World reopened_world;
  editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Reopened"));
  Require(reopened.Reload(root / "Reopened.scene") &&
              reopened.PrepareSave()->Bytes() == after.Bytes() &&
              reopened.PrefabPlacements().front().revision == 2 && scene.Undo() &&
              scene.MatchesPreparedSave(before),
          "Rebase exact save/reopen or Undo after Save failed");
}
} // namespace
int main() {
  try {
    Run();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
