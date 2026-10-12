#include "Nexora/Editor/PrefabPlacementInspection.h"
#include "Nexora/Editor/PrefabPlacementOverrides.h"
#include "Nexora/Editor/ProjectPrefabPlacement.h"
#include "Nexora/Editor/SceneFiles.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
namespace {
using namespace nexora;
using Assets = editor::PrefabAssets;
using Owner = editor::ProjectPrefabPlacement;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::map<std::filesystem::path, std::string> Files(const std::filesystem::path &root) {
  std::map<std::filesystem::path, std::string> result;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file()) {
      std::ifstream file(entry.path(), std::ios::binary);
      result.emplace(entry.path().lexically_relative(root),
                     std::string{std::istreambuf_iterator<char>(file), {}});
    }
  return result;
}
void Write(const std::filesystem::path &path, const editor::PrefabAsset &asset) {
  const auto encoded = Assets::Encode(asset);
  Require(encoded.has_value(), "Replacement fixture was not a valid prefab asset");
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char *>(encoded->data()),
             static_cast<std::streamsize>(encoded->size()));
  Require(static_cast<bool>(file), "Replacement fixture write failed");
}
void Run() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-project-placement-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(path, error);
    }
  } cleanup{root};
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root / "Project", "Placement"), "Project fixture failed");
  runtime::World source_world, nested_world, target_world;
  editor::SceneDocument source(source_world, source_world.LoadScene("Source"));
  editor::SceneDocument nested(nested_world, nested_world.LoadScene("Nested"));
  editor::SceneDocument target(target_world, target_world.LoadScene("Target"));
  const auto attachment = source.Create("Attachment"), child = nested.Create("Old nested");
  Require(nested.SetOpaqueComponent(*nested.Key(child), {99, "Absent.Provider", {0, 255, 27}}),
          "Unknown nested fixture failed");
  Require(nested.SetEulerField(std::array{*nested.Key(child)}, 2, 37.5),
          "Retained source rotation fixture failed");
  auto source_transform = *nested.Transform(child);
  source_transform.x = 2.75;
  source_transform.y = 3.25;
  source_transform.z = -7.125;
  source_transform.sx = 2;
  source_transform.sy = -3;
  source_transform.sz = .5;
  Require(nested.SetTransform(child, source_transform), "Retained source TRS fixture failed");
  const auto source_capture = nested.CaptureRuntimeScene();
  Require(source_capture.has_value(), "Dormant source fixture capture failed");
  std::istringstream source_input(source_capture->runtime_snapshot);
  std::string source_header, source_record;
  std::getline(source_input, source_header);
  std::getline(source_input, source_record);
  std::istringstream record_input(source_record);
  std::vector<std::string> tokens;
  for (std::string token; record_input >> token;)
    tokens.push_back(token);
  Require(tokens.size() == 21 && tokens[12] == "0" && tokens[13] == "0" && tokens[14] == "0",
          "Dormant source snapshot schema changed");
  tokens[15] = "91";
  tokens[16] = "0.2";
  tokens[17] = "950";
  tokens[18] = "3.5";
  tokens[19] = "123456789012345";
  tokens[20] = "987654321098765";
  std::string dormant_runtime = source_header + '\n';
  for (std::size_t i = 0; i < tokens.size(); ++i)
    dormant_runtime += (i ? " " : "") + tokens[i];
  dormant_runtime += '\n';
  Require(nested_world.ReplaceSceneSnapshot(source_capture->scene, dormant_runtime),
          "Dormant stored component fixture failed");
  std::uint64_t serial = 100;
  const auto factory = [&] { return foundation::Uuid{1900, ++serial}; };
  auto parent = Assets::Capture({1901, 1}, source, factory);
  const auto old = Assets::Capture({1901, 2}, nested, factory);
  Require(parent && old && Assets::Publish(workspace, *old), "First nested publication failed");
  Require(nested.Rename(*nested.Key(child), "New nested"), "Nested advance failed");
  const auto newer = Assets::Capture(old->id, nested, factory, &*old);
  Require(newer && Assets::Publish(workspace, *newer, &*old), "Nested archive publication failed");
  const auto attach_id =
      std::ranges::find(parent->nodes, attachment, &editor::PrefabNodeIdentity::serialized_node)
          ->id;
  parent->nested = {{{1902, 1}, attach_id, {old->id, 1}}, {{1902, 2}, attach_id, {old->id, 2}}};
  Require(Assets::Publish(workspace, *parent), "Parent publication failed");
  const auto seed = target.Create("Seed");
  const auto seed_key = *target.Key(seed);
  Require(target.Select(std::array{seed}) && target.CopySelection() &&
              target.Rename(seed_key, "Pending redo") && target.Undo(),
          "History fixture failed");
  const auto expected = *target.PrepareSave();
  auto review = Owner::Prepare(workspace, parent->id, target);
  Require(review && review->Source() == editor::PrefabRevisionReference{parent->id, 1} &&
              review->NodeCount() == 3,
          "Exact retained project closure preparation failed");
  const auto original_files = Files(workspace.Root());
  Require(!Owner::Prepare(workspace, {}, target) &&
              !Owner::Instantiate(workspace, target, *review, {1903, 1}, false) &&
              !Owner::Instantiate(workspace, target, *review, {}, true) &&
              target.MatchesPreparedSave(expected) && target.Redo() && target.Undo(),
          "Rejected placement changed content or consumed Redo");
  runtime::World foreign_world;
  editor::SceneDocument foreign(foreign_world, foreign_world.LoadScene("Foreign"));
  Require(!Owner::Instantiate(workspace, foreign, *review, {1903, 1}, true),
          "Foreign document consumed a prepared placement");
  editor::ProjectWorkspace observer;
  Require(observer.Open(workspace.Root(), editor::ProjectAccess::ReadOnly), "Observer open failed");
  const auto readonly = Owner::Prepare(observer, parent->id, target);
  Require(readonly && !Owner::Instantiate(observer, target, *readonly, {1903, 1}, true) &&
              target.MatchesPreparedSave(expected),
          "Read-only observation acquired write authority");
  const auto recovery = workspace.Root() / ".nexora/workspace.recovery";
  {
    std::ofstream file(recovery);
    file << "pending";
  }
  Require(!Owner::Prepare(workspace, parent->id, target) &&
              !Owner::Instantiate(workspace, target, *review, {1903, 1}, true),
          "Pending recovery allowed placement");
  std::filesystem::remove(recovery);
  const auto state = workspace.Root() / ".nexora/workspace";
  const auto timestamp = std::filesystem::last_write_time(state);
  std::filesystem::last_write_time(state, timestamp + std::chrono::seconds(10));
  Require(!Owner::Prepare(workspace, parent->id, target) &&
              !Owner::Instantiate(workspace, target, *review, {1903, 1}, true),
          "Externally changed project allowed placement");
  std::filesystem::last_write_time(state, timestamp);
  const auto archive =
      workspace.Root() / ".nexora/prefabs/revisions" / old->id.ToString() / "1.nxprefab";
  auto replaced = *old;
  runtime::World tamper_world;
  editor::SceneDocument tamper(tamper_world, tamper_world.LoadScene("Tamper"));
  Require(tamper.ReloadBytes(old->scene_bytes) &&
              tamper.Rename(*tamper.Key(child), "Valid replacement"),
          "Replacement fixture failed");
  replaced.scene_bytes = tamper.PrepareSave()->Bytes();
  Write(archive, replaced);
  Require(!Owner::Instantiate(workspace, target, *review, {1903, 1}, true) &&
              target.MatchesPreparedSave(expected),
          "Valid same-revision transitive replacement retained stale placement authority");
  Write(archive, *old);
  Require(target.Rename(seed_key, "Stale local") &&
              !Owner::Instantiate(workspace, target, *review, {1903, 1}, true) && target.Undo(),
          "Stale target accepted placement");
  const auto placed = Owner::Instantiate(workspace, target, *review, {1903, 1}, true);
  Require(placed && placed->size() == 3 && target.PrefabPlacements().size() == 1 &&
              target.PrefabPlacements().front().nodes.size() == 3 &&
              target.PrefabPlacements().front().revision == 1 && target.Key(seed) == seed_key &&
              target.Dirty() && Files(workspace.Root()) == original_files,
          "Placement lost scoped bindings, existing keys or wrote before Scene Save");
  const auto published = *target.PrepareSave();
  using Overrides = editor::PrefabPlacementOverrides;
  std::string override_error;
  const auto clean_review =
      Overrides::Prepare(observer, target, placed->front().target, &override_error);
  Require(clean_review && clean_review->Rows().empty() &&
              clean_review->Instance() == foundation::Uuid{1903, 1} &&
              clean_review->Source() == editor::PrefabRevisionReference{parent->id, 1} &&
              Overrides::Matches(workspace, target, *clean_review) && override_error.empty(),
          "Clean nested placement fabricated property/parent overrides from translated IDs");
  const auto old_node = std::ranges::find_if(*placed, [](const auto &entry) {
    return entry.scope == std::vector{foundation::Uuid{1902, 1}};
  });
  Require(old_node != placed->end() && target.Rename(old_node->target, "Local nested override") &&
              target.SetEulerField(std::array{old_node->target}, 1, 720) &&
              target.SetOpaqueComponent(old_node->target, {99, "Absent.Provider", {0, 255, 29}}) &&
              target.SetOpaqueComponent(placed->front().target, {123, "Local.Provider", {42}}),
          "Live nested override fixture failed");
  const auto edited = *target.PrepareSave();
  const auto property_review =
      Overrides::Prepare(observer, target, old_node->target, &override_error);
  Require(property_review && !property_review->Rows().empty() &&
              !Overrides::Matches(workspace, target, *clean_review) &&
              Overrides::Matches(observer, target, *property_review) &&
              target.MatchesPreparedSave(edited) && Files(workspace.Root()) == original_files,
          "Read-only override review mutated target/files or retained stale content");
  const auto named = std::ranges::find_if(property_review->Rows(), [&](const auto &row) {
    return row.target == old_node->target && row.field == "name";
  });
  const auto name_identity = std::ranges::find(old->nodes.front().properties, "name",
                                               &editor::PrefabPropertyIdentity::field);
  Require(named != property_review->Rows().end() && named->scope == old_node->scope &&
              named->source == editor::PrefabRevisionReference{old->id, 1} &&
              named->node == old_node->node &&
              name_identity != old->nodes.front().properties.end() &&
              named->property == name_identity->id && named->retained == "Old nested" &&
              named->local == "Local nested override" && !named->structural,
          "Override review lost exact scoped source node/field identity");
  Require(
      std::ranges::any_of(property_review->Rows(),
                          [&](const auto &row) {
                            return row.target == old_node->target &&
                                   row.field == "authoring/euler/y" && row.retained == "0" &&
                                   row.local == "720" && row.property.has_value();
                          }) &&
          std::ranges::any_of(property_review->Rows(),
                              [&](const auto &row) {
                                return row.target == old_node->target &&
                                       row.field == "opaque/99/payload_hex" &&
                                       row.retained == "00ff1b" && row.local == "00ff1d" &&
                                       row.property.has_value();
                              }) &&
          std::ranges::any_of(property_review->Rows(),
                              [&](const auto &row) {
                                return row.target == placed->front().target &&
                                       row.field == "opaque/123/payload_hex" && !row.retained &&
                                       row.local == "2a" && !row.property;
                              }),
      "Override review lost authored Euler, unknown bytes or explicit local component addition");
  const auto selection_before_revert =
      std::vector(target.Selection().begin(), target.Selection().end());
  const auto bindings_before_revert =
      std::vector(target.PrefabPlacements().begin(), target.PrefabPlacements().end());
  const auto name_index = static_cast<std::size_t>(named - property_review->Rows().begin());
  Require(!Overrides::RevertSelected(workspace, target, *property_review,
                                     std::array{name_index, name_index}, true) &&
              !Overrides::RevertSelected(workspace, target, *property_review,
                                         std::array{property_review->Rows().size()}, true) &&
              !Overrides::RevertSelected(observer, target, *property_review, std::array{name_index},
                                         true) &&
              !Overrides::RevertSelected(workspace, target, *property_review,
                                         std::array{name_index}, false) &&
              target.MatchesPreparedSave(edited) &&
              Overrides::RevertSelected(workspace, target, *property_review, std::array{name_index},
                                        true) &&
              target.Name(old_node->target.id) == "Old nested" &&
              target.EulerAngles(old_node->target.id)->at(1) == 720 &&
              target.OpaqueComponents(old_node->target)->front().data ==
                  std::vector<std::uint8_t>{0, 255, 29} &&
              target.OpaqueComponents(placed->front().target)->size() == 1,
          "Selected Name revert changed unselected Euler/opaque or admitted invalid scope");
  const auto named_reverted = *target.PrepareSave();
  Require(target.Undo() && target.MatchesPreparedSave(edited) && target.Redo() &&
              target.MatchesPreparedSave(named_reverted) && target.Undo() &&
              Overrides::RevertSelected(workspace, target, *property_review, {}, true) &&
              target.Redo() && target.MatchesPreparedSave(named_reverted) && target.Undo() &&
              target.MatchesPreparedSave(edited) && Files(workspace.Root()) == original_files,
          "Selected Name revert/no-op lost one history boundary or pending Redo");
  const auto added_row = std::ranges::find_if(property_review->Rows(), [](const auto &row) {
    return row.field == "opaque/123/payload_hex";
  });
  const auto rotation_row = std::ranges::find_if(
      property_review->Rows(), [](const auto &row) { return row.field == "authoring/euler/y"; });
  Require(added_row != property_review->Rows().end() &&
              rotation_row != property_review->Rows().end(),
          "Selected stable property fixtures absent");
  const auto added_index = static_cast<std::size_t>(added_row - property_review->Rows().begin());
  const auto rotation_index =
      static_cast<std::size_t>(rotation_row - property_review->Rows().begin());
  Require(Overrides::RevertSelected(workspace, target, *property_review,
                                    std::array{added_index, rotation_index}, true) &&
              target.OpaqueComponents(placed->front().target)->empty() &&
              target.EulerAngles(old_node->target.id)->at(1) == 0 &&
              target.EulerAngles(old_node->target.id)->at(2) == 37.5 &&
              target.Name(old_node->target.id) == "Local nested override" &&
              target.OpaqueComponents(old_node->target)->front().data ==
                  std::vector<std::uint8_t>{0, 255, 29} &&
              target.Undo() && target.MatchesPreparedSave(edited),
          "Selected rotation/addition groups lost exact hints or unselected scoped values");
  auto local_transform = *target.Transform(old_node->target.id);
  local_transform.x = 9;
  local_transform.y = 8;
  local_transform.z = 7;
  local_transform.sx = 4;
  local_transform.sy = 5;
  local_transform.sz = 6;
  Require(target.SetTransform(old_node->target.id, local_transform),
          "Selected position/scale fixture failed");
  const auto positioned = *target.PrepareSave();
  const auto position_review = Overrides::Prepare(workspace, target, old_node->target);
  Require(position_review.has_value(), "Position review failed");
  std::vector<std::size_t> position_rows;
  for (std::size_t i = 0; i < position_review->Rows().size(); ++i)
    if (position_review->Rows()[i].field.starts_with("transform/position/"))
      position_rows.push_back(i);
  Require(position_rows.size() == 3 &&
              position_review->Rows()[position_rows[0]].property ==
                  position_review->Rows()[position_rows[1]].property &&
              Overrides::RevertSelected(workspace, target, *position_review,
                                        std::span(position_rows).first(2), true),
          "Distinct lanes of one stable position group did not coalesce");
  const auto position_result = *target.Transform(old_node->target.id);
  Require(position_result.x == 2.75 && position_result.y == 3.25 && position_result.z == -7.125 &&
              position_result.sx == 4 && position_result.sy == 5 && position_result.sz == 6 &&
              target.EulerAngles(old_node->target.id)->at(1) == 720 &&
              target.Name(old_node->target.id) == "Local nested override" && target.Undo() &&
              target.MatchesPreparedSave(positioned) && target.Undo() &&
              target.MatchesPreparedSave(edited) && Files(workspace.Root()) == original_files,
          "Selected position group changed unselected scale/Euler/name or lost one Undo");
  Require(!Overrides::Revert(observer, target, *property_review, true, &override_error) &&
              !Overrides::Revert(workspace, target, *property_review, false, &override_error) &&
              !Overrides::Revert(workspace, foreign, *property_review, true, &override_error) &&
              target.MatchesPreparedSave(edited) && Files(workspace.Root()) == original_files &&
              Overrides::Revert(workspace, target, *property_review, true, &override_error) &&
              override_error.empty() && target.MatchesPreparedSave(published) && target.Dirty() &&
              std::ranges::equal(target.Selection(), selection_before_revert) &&
              std::ranges::equal(target.PrefabPlacements(), bindings_before_revert) &&
              target.Key(seed) == seed_key && target.Key(old_node->target.id) == old_node->target &&
              Files(workspace.Root()) == original_files && target.Undo() &&
              target.MatchesPreparedSave(edited) && target.Redo() &&
              target.MatchesPreparedSave(published),
          "Whole instance revert lost exact source/dormant values, keys, files or one Undo/Redo");
  Require(target.Rename(seed_key, "No-op redo sentinel") && target.Undo() &&
              Overrides::Revert(workspace, target, *clean_review, true) && target.Redo() &&
              target.Name(seed) == "No-op redo sentinel" && target.Undo() && target.Undo() &&
              target.MatchesPreparedSave(edited),
          "No-op prefab revert consumed Redo or changed saved/history boundaries");
  Require(target.Undo() && target.Undo() && target.Undo() && target.Undo() &&
              target.MatchesPreparedSave(published) && target.Redo() && target.Redo() &&
              target.Redo() && target.Redo() && target.MatchesPreparedSave(edited),
          "Override review changed authoring Undo/Redo boundaries");
  const auto repeated_review = Overrides::Prepare(workspace, target, old_node->target);
  Require(repeated_review && std::ranges::equal(repeated_review->Rows(), property_review->Rows()) &&
              target.Undo() && target.Undo() && target.Undo() && target.Undo() &&
              target.MatchesPreparedSave(published),
          "Owning override review was not deterministic across actual history replay");
  Write(archive, replaced);
  Require(!Overrides::Matches(workspace, target, *clean_review) &&
              !Overrides::Revert(workspace, target, *clean_review, true),
          "Valid same-revision source replacement retained an old override review");
  std::filesystem::remove(archive);
  Require(!Overrides::Prepare(observer, target, placed->front().target, &override_error) &&
              !override_error.empty() &&
              !Overrides::Revert(workspace, target, *clean_review, true) &&
              target.MatchesPreparedSave(published),
          "Missing retained source produced a partial review or changed the target");
  Write(archive, *old);
  Require(target.SetOpaqueComponent(old_node->target,
                                    {99, "Absent.Provider", std::vector<std::uint8_t>(40000)}) &&
              !Overrides::Prepare(workspace, target, old_node->target, &override_error) &&
              !override_error.empty() && target.Undo() && target.MatchesPreparedSave(published) &&
              Files(workspace.Root()) == original_files,
          "Over-budget semantic value produced a partial review or mutated source/history");
  auto stale_override_key = placed->front().target;
  ++stale_override_key.document_generation;
  Require(!Overrides::Prepare(workspace, target, seed_key) &&
              !Overrides::Prepare(workspace, target, stale_override_key) &&
              target.Reparent(old_node->target.id, seed),
          "Unbound/stale review or hierarchy fixture failed");
  const auto hierarchy_review = Overrides::Prepare(workspace, target, old_node->target);
  Require(hierarchy_review &&
              std::ranges::any_of(hierarchy_review->Rows(),
                                  [&](const auto &row) {
                                    return row.target == old_node->target &&
                                           row.field == "parent" && row.structural &&
                                           row.property.has_value() &&
                                           row.local == std::to_string(seed);
                                  }) &&
              !Overrides::Revert(workspace, target, *hierarchy_review, true) && target.Undo() &&
              target.MatchesPreparedSave(published),
          "Hierarchy change was silently treated as a supported value override");
  runtime::World active_world;
  editor::SceneDocument active_stage(active_world, active_world.LoadScene("Active fixture"));
  Require(active_stage.ReloadBytes(published.Bytes()), "Active fixture staging failed");
  const auto active_key = *active_stage.Key(old_node->target.id);
  Require(active_stage.SetCamera(active_key, runtime::CameraComponent{72, .3, 400}) &&
              active_stage.SetLight(active_key, runtime::LightComponent{9}) &&
              active_stage.SetMeshRenderer(active_key, runtime::MeshComponent{8123, {9456}}) &&
              target.ApplyPropertySnapshot(published, active_stage.PrepareSave()->Bytes(), true),
          "Active component override fixture failed");
  const auto active = *target.PrepareSave();
  const auto active_review = Overrides::Prepare(workspace, target, old_node->target);
  Require(active_review && !active_review->Rows().empty() &&
              !Overrides::Revert(workspace, target, *clean_review, true),
          "Changed active components retained stale review authority");
  const auto camera_row = std::ranges::find_if(
      active_review->Rows(), [](const auto &row) { return row.field == "camera/enabled"; });
  Require(camera_row != active_review->Rows().end(), "Selected Camera presence row absent");
  const auto camera_index = static_cast<std::size_t>(camera_row - active_review->Rows().begin());
  Require(Overrides::RevertSelected(workspace, target, *active_review, std::array{camera_index},
                                    true) &&
              !target.Camera(old_node->target) && target.Light(old_node->target)->intensity == 9 &&
              target.MeshRenderer(old_node->target)->mesh == 8123 &&
              target_world.FindEntity(old_node->target.id)->camera_data.vertical_field_of_view ==
                  91 &&
              target.Undo() && target.MatchesPreparedSave(active),
          "Selected Camera group lost dormant data or changed unselected Light/Mesh");
  {
    std::ofstream file(recovery);
    file << "pending";
  }
  Require(!Overrides::Revert(workspace, target, *active_review, true) &&
              target.MatchesPreparedSave(active),
          "Recovery admitted live instance property revert");
  std::filesystem::remove(recovery);
  std::filesystem::last_write_time(state, timestamp + std::chrono::seconds(10));
  Require(!Overrides::Revert(workspace, target, *active_review, true) &&
              target.MatchesPreparedSave(active),
          "External project change admitted live instance property revert");
  std::filesystem::last_write_time(state, timestamp);
  Require(Overrides::Revert(workspace, target, *active_review, true),
          "Active component revert failed");
  Require(target.MatchesPreparedSave(published) && Files(workspace.Root()) == original_files,
          "Active-to-dormant revert lost stored values or changed files");
  Require(target.Undo() && target.MatchesPreparedSave(active) && target.Redo() &&
              target.MatchesPreparedSave(published),
          "Active-to-dormant revert lost atomic Undo/Redo");
  Require(target.Undo() && target.Undo() && target.MatchesPreparedSave(published),
          "Complete active component fixture history lost dormant stored values");
  using Inspector = editor::PrefabPlacementInspector;
  std::optional<editor::PrefabPlacementInspection> inspected;
  for (const auto &entry : *placed) {
    auto value = Inspector::Inspect(workspace, target, entry.target);
    const auto scoped = entry.scope.empty() ? editor::PrefabRevisionReference{parent->id, 1}
                        : entry.scope.front() == foundation::Uuid{1902, 1}
                            ? editor::PrefabRevisionReference{old->id, 1}
                            : editor::PrefabRevisionReference{old->id, 2};
    Require(value && value->Resolved() && value->Instance() == foundation::Uuid{1903, 1} &&
                value->Source() == editor::PrefabRevisionReference{parent->id, 1} &&
                value->SourceNode() == entry.node && value->ScopedSource() == scoped &&
                std::ranges::equal(value->SourceScope(), entry.scope) &&
                value->PublishedRevision() == 1 && value->MappedNodes() == 3 &&
                Inspector::Matches(workspace, target, *value),
            "Owning placement inspection lost exact mixed-revision scoped identity");
    inspected = std::move(value);
  }
  Require(!Inspector::Inspect(workspace, target, seed_key) &&
              !Inspector::Inspect(workspace, foreign, placed->front().target) &&
              Inspector::Inspect(observer, target, placed->front().target) &&
              target.MatchesPreparedSave(published) && Files(workspace.Root()) == original_files,
          "Inspection acquired authority, changed files or resolved an unbound/foreign node");
  auto mismatched = published.Bytes();
  const auto source_token = " " + placed->front().node.ToString() + " " +
                            std::to_string(placed->front().target.id) + "\n";
  const auto token_position = mismatched.find(source_token);
  Require(token_position != std::string::npos, "Scoped metadata mismatch fixture absent");
  mismatched.replace(token_position + 1, 36, foundation::Uuid{1904, 99}.ToString());
  runtime::World mismatch_world;
  editor::SceneDocument mismatch(mismatch_world, mismatch_world.LoadScene("Mismatch"));
  Require(mismatch.ReloadBytes(mismatched), "Syntactically valid source mismatch fixture failed");
  const auto incompatible =
      Inspector::Inspect(workspace, mismatch, *mismatch.Key(placed->front().target.id));
  Require(incompatible && !incompatible->Resolved() && !incompatible->ScopedSource() &&
              Inspector::Matches(workspace, mismatch, *incompatible),
          "A valid scene binding to an absent scoped source node was falsely resolved");
  Write(archive, replaced);
  Require(!Inspector::Matches(workspace, target, *inspected) &&
              target.MatchesPreparedSave(published),
          "Valid transitive same-revision replacement retained stale inspection");
  Write(archive, *old);
  const auto original_archive =
      Files(workspace.Root()).at(archive.lexically_relative(workspace.Root()));
  std::filesystem::remove(archive);
  auto missing = Inspector::Inspect(workspace, target, placed->front().target);
  Require(missing && !missing->Resolved() && !missing->ScopedSource() &&
              missing->Instance() == foundation::Uuid{1903, 1} && missing->Source().revision == 1 &&
              Inspector::Matches(workspace, target, *missing) &&
              !Inspector::Matches(workspace, target, *inspected),
          "Missing retained nested source hid binding identity or remained falsely resolved");
  Write(archive, *old);
  Require(Files(workspace.Root()).at(archive.lexically_relative(workspace.Root())) ==
                  original_archive &&
              !Inspector::Matches(workspace, target, *missing) &&
              Inspector::Matches(workspace, target, *inspected),
          "Restored closure did not revoke unresolved inspection or preserve exact bytes");
  Require(target.Undo() && target.MatchesPreparedSave(expected) &&
              target.PrefabPlacements().empty() && target.Selection().front() == seed &&
              target.Redo() && target.MatchesPreparedSave(published) && target.Paste() &&
              target.Name(target.Selection().front()) == "Seed Copy" && target.Undo() &&
              target.MatchesPreparedSave(published),
          "Placement was not one complete Undo/Redo or erased clipboard");
  Require(Inspector::Matches(workspace, target, *inspected),
          "Owning inspection lost exact restored document context after history replay");
  auto stale_source = Owner::Prepare(workspace, parent->id, target);
  auto next_parent = *parent;
  next_parent.revision = 2;
  Require(stale_source && Assets::Publish(workspace, next_parent, &*parent) &&
              !Owner::Instantiate(workspace, target, *stale_source, {1903, 2}, true) &&
              target.MatchesPreparedSave(published),
          "Advancing published root retained stale authority");
  const auto advanced = Inspector::Inspect(workspace, target, placed->front().target);
  Require(advanced && advanced->Resolved() && advanced->Source().revision == 1 &&
              advanced->PublishedRevision() == 2 &&
              !Inspector::Matches(workspace, target, *inspected),
          "Inspection confused current publication with the placement's retained revision");
  Require(target.Rename(old_node->target, "Local edit after source advance"),
          "Advanced retained source override fixture failed");
  const auto advanced_edited = *target.PrepareSave();
  const auto advanced_review = Overrides::Prepare(workspace, target, old_node->target);
  const auto advanced_files = Files(workspace.Root());
  Require(advanced_review && advanced_review->Source().revision == 1 &&
              advanced_review->PublishedRevision() == 2 &&
              Overrides::Revert(workspace, target, *advanced_review, true) &&
              target.MatchesPreparedSave(published) && Files(workspace.Root()) == advanced_files &&
              target.Undo() && target.MatchesPreparedSave(advanced_edited) && target.Redo() &&
              target.MatchesPreparedSave(published),
          "Revert confused exact retained source with advanced current publication");
  auto fresh = Owner::Prepare(workspace, parent->id, target);
  Require(fresh && fresh->Source().revision == 2 &&
              Owner::Instantiate(workspace, target, *fresh, {1903, 2}, true) &&
              target.PrefabPlacements().size() == 2 &&
              target.PrefabPlacements().back().revision == 2,
          "Fresh repeated source placement failed");
  editor::SceneFileSession files(workspace, target);
  Require(files.SaveAs(files.Token(), "Content/Placed.scene").Applied() && !target.Dirty(),
          "Explicit Scene Save failed bound publication");
  runtime::World reopen_world;
  editor::SceneDocument reopen(reopen_world, reopen_world.LoadScene("Reopen"));
  Require(reopen.Reload(workspace.Root() / "Content/Placed.scene") && !reopen.Dirty() &&
              std::ranges::equal(reopen.PrefabPlacements(), target.PrefabPlacements()),
          "Saved project placement did not reopen exact scoped source revisions");
  Require(!Inspector::Inspect(workspace, reopen, placed->front().target),
          "Reopened scene accepted a stale generation key");
  const auto reopened_inspection =
      Inspector::Inspect(workspace, reopen, *reopen.Key(placed->front().target.id));
  Require(reopened_inspection && reopened_inspection->Resolved() &&
              reopened_inspection->Source().revision == 1 &&
              reopened_inspection->PublishedRevision() == 2,
          "Clean scene reopen did not expose its exact retained placement source");
  const auto clean_saved = *target.PrepareSave();
  const auto saved_files = Files(workspace.Root());
  Require(target.Rename(old_node->target, "Saved instance override") && target.Dirty(),
          "Saved instance override fixture failed");
  const auto saved_review = Overrides::Prepare(workspace, target, old_node->target);
  Require(saved_review && Overrides::Revert(workspace, target, *saved_review, true) &&
              target.MatchesPreparedSave(clean_saved) && !target.Dirty() &&
              Files(workspace.Root()) == saved_files && target.Undo() && target.Dirty() &&
              target.Redo() && !target.Dirty() && files.Save(files.Token()).Applied(),
          "Revert changed the saved baseline or wrote before explicit Scene Save");
  Require(reopen.Reload(workspace.Root() / "Content/Placed.scene") &&
              reopen.PrepareSave()->Bytes() == clean_saved.Bytes() &&
              std::ranges::equal(reopen.PrefabPlacements(), target.PrefabPlacements()),
          "Reverted instance Save/reopen lost exact retained properties or bindings");
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
