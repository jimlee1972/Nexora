#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/SceneMerge.h"
#include "SceneMergeTestAccess.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

namespace {
using namespace nexora;
using Merge = editor::SceneMerge;
using Side = editor::SceneMergeChoice;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Bytes(const editor::SceneDocument &document) {
  const auto prepared = document.PrepareSave();
  Require(prepared.has_value(), "Independent scene preparation failed");
  return prepared->Bytes();
}
std::size_t Row(const editor::SceneMergeReview &review, const std::string &path) {
  const auto &rows = review.Comparison().rows;
  const auto found = std::ranges::find(rows, path, &editor::SceneComparisonRow::stable_path);
  Require(found != rows.end(), "Expected semantic choice row is missing");
  return static_cast<std::size_t>(found - rows.begin());
}
void Run() {
  runtime::World world;
  editor::SceneDocument document(world, world.LoadScene("Merge Unicode 合併", true));
  const auto root = document.Create("Base");
  const auto child = document.Create("Child", root);
  Require(
      root && child &&
          document.SetOpaqueComponent(*document.Key(root), {781, "Unknown.Provider", {0, 255, 17}}),
      "Real merge fixture failed");
  auto base = Bytes(document);
  Require(
      document.Rename(*document.Key(root), "Local") &&
          document.SetOpaqueComponent(*document.Key(root), {781, "Unknown.Provider", {0, 255, 19}}),
      "Local fixture failed");
  auto transform = *document.Transform(root);
  transform.x = 4.5;
  transform.sx = -2;
  Require(document.SetTransform(root, transform), "Local pose failed");
  auto local = Bytes(document);
  runtime::World remote_world;
  editor::SceneDocument remote(remote_world, remote_world.LoadScene("Remote"));
  Require(remote.ReloadBytes(base) && remote.Rename(*remote.Key(root), "Remote") &&
              remote.SetOpaqueComponent(*remote.Key(root), {781, "Unknown.Provider", {0, 255, 21}}),
          "Remote fixture failed");
  transform = *remote.Transform(root);
  transform.z = 7;
  Require(remote.SetTransform(root, transform), "Remote pose failed");
  auto disk = Bytes(remote);
  const auto generation = document.Generation();
  const auto baseline = document.PrepareSave();
  std::string error;
  auto review = Merge::Prepare(base, local, disk, &error);
  Require(review && error.empty() && review->Identity() && review->Comparison().conflicts == 2,
          "Owning merge preparation failed");
  editor::SceneMergeRequest request{1, review->Identity(), {}};
  Require(!Merge::Resolve(*review, request), "Unresolved conflicts produced a candidate");
  const auto prefix = "entities/" + std::to_string(root) + '/';
  request.choices = {{Row(*review, prefix + "name"), Side::Local},
                     {Row(*review, prefix + "opaque/781/payload_hex"), Side::Remote}};
  const auto candidate = Merge::Resolve(*review, request, &error);
  Require(candidate && candidate->bytes && error.empty(), "Resolved candidate was rejected");
  runtime::World staged_world;
  const auto staged_id = staged_world.LoadScene("Candidate");
  editor::SceneDocument staged(staged_world, staged_id);
  Require(staged.ReloadBytes(*candidate->bytes) && staged.Name(root) == "Local" &&
              staged.Name(child) == "Child" && staged_world.Parent(child) == root &&
              staged.Transform(root)->x == 4.5 && staged.Transform(root)->z == 7 &&
              staged.Transform(root)->sx == -2 &&
              staged.OpaqueComponents(*staged.Key(root))->size() == 1 &&
              staged.OpaqueComponents(*staged.Key(root))->front().data ==
                  std::vector<std::uint8_t>({0, 255, 21}) &&
              staged.OpaqueComponents(*staged.Key(root))->front().type_name == "Unknown.Provider" &&
              staged_world.FindScene(staged_id)->persistent,
          "Actual merged scene lost independent chosen/unselected values");
  Require(document.MatchesPreparedSave(*baseline) && document.Generation() == generation &&
              Bytes(document) == local && Bytes(remote) == disk,
          "Pure merge work mutated source Worlds, baseline or history");
  editor::detail::SceneMergeTestAccess::FailNextAfterLoad();
  bool allocation{};
  try {
    static_cast<void>(Merge::Resolve(*review, request));
  } catch (const std::bad_alloc &) {
    allocation = true;
  }
  Require(allocation && document.MatchesPreparedSave(*baseline) && review->BaseBytes() == base &&
              review->LocalBytes() == local && review->RemoteBytes() == disk &&
              Merge::Resolve(*review, request)->bytes == candidate->bytes,
          "Allocation failure leaked a candidate or mutated retained sources/history");
  auto other = Merge::Prepare(base, local, disk);
  Require(other && other->Identity() != review->Identity() && !Merge::Resolve(*other, request),
          "Old row choices retargeted another immutable review");
  auto invalid = request;
  invalid.schema = 2;
  Require(!Merge::Resolve(*review, invalid), "Unknown request schema admitted");
  invalid = request;
  invalid.review = 0;
  Require(!Merge::Resolve(*review, invalid), "Absent review identity admitted");
  invalid = request;
  invalid.choices[0].choice = static_cast<Side>(255);
  Require(!Merge::Resolve(*review, invalid), "Forged source side admitted");
  invalid = request;
  invalid.choices[1].row = invalid.choices[0].row;
  Require(!Merge::Resolve(*review, invalid), "Duplicate source choice admitted");
  invalid = request;
  invalid.choices[0].row = review->Comparison().rows.size();
  Require(!Merge::Resolve(*review, invalid), "Absent source row admitted");
  const auto original_base = base, original_local = local, original_disk = disk;
  base = local = disk = "caller buffers replaced";
  Require(review->BaseBytes() == original_base && review->LocalBytes() == original_local &&
              review->RemoteBytes() == original_disk &&
              Merge::Resolve(*review, request)->bytes == candidate->bytes,
          "Merge review retained caller-owned source views");
  auto owning = std::move(*review);
  Require(!review->Identity() && review->Comparison().rows.empty() && !review->BaseBytes() &&
              !Merge::Resolve(*review, request) &&
              Merge::Resolve(owning, request)->bytes == candidate->bytes,
          "Moved-from review was unsafe or retained choice authority");
  const auto empty = Merge::Prepare({}, {}, {});
  Require(empty && !Merge::Resolve(*empty, {1, empty->Identity(), {}})->bytes,
          "Absent whole scene became an empty present file");
  Require(!Merge::Prepare(std::string_view{}, {}, {}) &&
              !Merge::Prepare("NEXORA_EDITOR_SCENE 999\n", {}, {}) &&
              !Merge::Prepare(std::string(editor::SceneComparison::kMaximumSourceBytes + 1, 'x'),
                              {}, {}),
          "Empty/corrupt/unsupported/oversized source admitted");
  const auto temporary =
      std::filesystem::temp_directory_path() /
      ("nexora-merge-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(temporary);
  struct Cleanup final {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code ignored;
      std::filesystem::remove_all(path, ignored);
    }
  } cleanup{temporary};
  const auto path = temporary / std::filesystem::path(u8"合併.scene");
  runtime::World reopened_world;
  editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Fresh"));
  Require(staged.Save(path) && reopened.Reload(path) && Bytes(reopened) == *candidate->bytes &&
              reopened.Name(root) == "Local" &&
              reopened.OpaqueComponents(*reopened.Key(root))->front().data ==
                  std::vector<std::uint8_t>({0, 255, 21}) &&
              document.Undo() && document.Transform(root)->x == 0 && document.Redo() &&
              document.MatchesPreparedSave(*baseline),
          "Real candidate Save/fresh reopen or source Undo/Redo conservation failed");
}
void Components() {
  for (const std::string flags : {"0 0 0", "1 1 1"}) {
    const auto source = "NEXORA_EDITOR_SCENE 3\nnode 11 0 Components\n"
                        "opaque 11 781 \"Unknown.%s\" 00007fff\nworld\n"
                        "NEXORA_SCENE 3 \"Stored\" 1 1\n11 0 3 4 5 0 0 0 1 -2 3 4 " +
                        flags + " 47 0.25 1700 2.5 18446744073709551614 18446744073709551613\n";
    const auto review = Merge::Prepare(source, source, source);
    Require(review.has_value(), "Independent component literal rejected");
    const auto candidate = Merge::Resolve(*review, {1, review->Identity(), {}});
    runtime::World world;
    const auto scene = world.LoadScene("Components");
    editor::SceneDocument staged(world, scene);
    Require(candidate && candidate->bytes && staged.ReloadBytes(*candidate->bytes),
            "Unchanged complete component candidate rejected");
    const auto *entity = world.FindEntity(11);
    const auto enabled = flags.front() == '1';
    Require(
        entity && entity->camera == enabled && entity->light == enabled &&
            entity->mesh_renderer == enabled && entity->camera_data.vertical_field_of_view == 47 &&
            entity->camera_data.near_plane == .25 && entity->camera_data.far_plane == 1700 &&
            entity->light_data.intensity == 2.5 &&
            entity->mesh_data.mesh == std::numeric_limits<std::uint64_t>::max() - 1 &&
            entity->mesh_data.material.shader == std::numeric_limits<std::uint64_t>::max() - 2 &&
            entity->transform.x == 3 && entity->transform.y == 4 && entity->transform.z == 5 &&
            entity->transform.sx == -2 && entity->transform.sy == 3 && entity->transform.sz == 4 &&
            staged.OpaqueComponents(*staged.Key(11))->front().data ==
                std::vector<std::uint8_t>({0, 0, 127, 255}),
        "Enabled/dormant component fields, U64 references, pose or unknown bytes changed");
  }
}
void InvalidHybrids() {
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Hybrids"));
  const auto a = scene.Create("A"), b = scene.Create("B");
  const auto base = Bytes(scene);
  Require(scene.Reparent(a, b), "Local hierarchy fixture failed");
  const auto local = Bytes(scene);
  Require(scene.ReloadBytes(base) && scene.Reparent(b, a), "Remote hierarchy fixture failed");
  const auto remote = Bytes(scene);
  const auto cycle = Merge::Prepare(base, local, remote);
  Require(cycle && !cycle->Comparison().conflicts &&
              !Merge::Resolve(*cycle, {1, cycle->Identity(), {}}),
          "Independent valid parent edits published a cyclic hybrid");
  Require(scene.ReloadBytes(base) && scene.SetEulerField(std::array{*scene.Key(a)}, 0, 90),
          "Local rotation fixture failed");
  const auto x = Bytes(scene);
  Require(scene.ReloadBytes(base) && scene.SetEulerField(std::array{*scene.Key(a)}, 1, 90),
          "Remote rotation fixture failed");
  const auto y = Bytes(scene);
  const auto rotation = Merge::Prepare(base, x, y);
  Require(rotation && !Merge::Resolve(*rotation, {1, rotation->Identity(), {}}),
          "Incoherent independent quaternion/Euler edits admitted");
  editor::SceneMergeRequest take_local{1, rotation->Identity(), {}};
  for (std::size_t i = 0; i < rotation->Comparison().rows.size(); ++i)
    take_local.choices.push_back({i, Side::Local});
  const auto fixed = Merge::Resolve(*rotation, take_local);
  Require(fixed && fixed->bytes && scene.ReloadBytes(*fixed->bytes) &&
              scene.EulerAngles(a) == editor::EulerDegrees{90, 0, 0},
          "Explicit coherent rotation resolution failed");
}
void GeneralRotations() {
  for (const auto angles :
       {editor::EulerDegrees{13, 27, 41}, editor::EulerDegrees{-163, 17, 85},
        editor::EulerDegrees{720, 55, -107}, editor::EulerDegrees{.1, .2, .3}}) {
    runtime::World world;
    editor::SceneDocument scene(world, world.LoadScene("Rotation stability"));
    const auto id = scene.Create("Rotated");
    for (std::size_t axis = 0; axis < 3; ++axis)
      Require(scene.SetEulerField(std::array{*scene.Key(id)}, axis, angles[axis]),
              "General rotation fixture failed");
    const auto source = Bytes(scene);
    const auto review = Merge::Prepare(source, source, source);
    std::string error;
    const auto candidate =
        review ? Merge::Resolve(*review, {1, review->Identity(), {}}, &error) : std::nullopt;
    if (!candidate)
      std::cerr << "Rotation " << angles[0] << ',' << angles[1] << ',' << angles[2] << ": " << error
                << '\n';
    Require(candidate && candidate->bytes, "Ordinary complete source rotation rejected");
  }
}
void BindingGroups() {
  const std::string instance = "10000000-0000-0000-0000-000000000001";
  const std::string node = "30000000-0000-0000-0000-000000000001";
  const auto prefix = "NEXORA_EDITOR_SCENE 4\nnode 11 0 Bound\n"
                      "opaque 11 781 \"Unknown\" 00ff11\nprefab-placement " +
                      instance + " 20000000-0000-0000-0000-000000000001 ";
  const auto suffix = "\nprefab-node " + instance + " 0 " + node +
                      " 11\nworld\n"
                      "NEXORA_SCENE 3 \"Bound\" 0 1\n"
                      "11 0 0 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
  const auto base = prefix + "7" + suffix;
  const auto local = prefix + "8" + suffix;
  auto remote = base;
  remote.replace(remote.find(node), node.size(), "30000000-0000-0000-0000-000000000002");
  const auto review = Merge::Prepare(base, local, remote);
  Require(review && !review->Comparison().conflicts &&
              !Merge::Resolve(*review, {1, review->Identity(), {}}),
          "Independent source revision and node mapping produced a hybrid binding");
  editor::SceneMergeRequest request{1, review->Identity(), {}};
  for (std::size_t i = 0; i < review->Comparison().rows.size(); ++i)
    request.choices.push_back({i, Side::Local});
  const auto candidate = Merge::Resolve(*review, request);
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Binding resolution"));
  Require(candidate && candidate->bytes && scene.ReloadBytes(*candidate->bytes) &&
              scene.PrefabPlacements().size() == 1 &&
              scene.PrefabPlacements().front().revision == 8 &&
              scene.PrefabPlacements().front().nodes.front().target == 11 &&
              scene.PrefabPlacements().front().nodes.front().source_node.ToString() == node &&
              scene.OpaqueComponents(*scene.Key(11))->front().data ==
                  std::vector<std::uint8_t>({0, 255, 17}),
          "Explicit coherent binding lost identity, revision, mapping or unknown bytes");
  const auto deleted = Merge::Prepare(base, {}, base);
  Require(deleted && !Merge::Resolve(*deleted, {1, deleted->Identity(), {}})->bytes &&
              deleted->BaseBytes() == base && !deleted->LocalBytes(),
          "Whole-file deletion intent lost exact surviving provenance");
}
void CameraConflict() {
  const std::string prefix = "NEXORA_EDITOR_SCENE 2\nnode 11 0 Camera\nworld\n"
                             "NEXORA_SCENE 3 \"Camera\" 0 1\n11 0 0 0 0 0 0 0 1 1 1 1 1 0 0 47 ";
  const auto base = prefix + "0.25 1700 1 0 0\n";
  const auto local = prefix + "1500 1800 1 0 0\n";
  const auto remote = prefix + "0.1 1000 1 0 0\n";
  const auto review = Merge::Prepare(base, local, remote);
  Require(review && review->Comparison().conflicts == 2, "Camera source fixture rejected");
  editor::SceneMergeRequest request{1,
                                    review->Identity(),
                                    {{Row(*review, "entities/11/camera/near_plane"), Side::Local},
                                     {Row(*review, "entities/11/camera/far_plane"), Side::Remote}}};
  Require(!Merge::Resolve(*review, request),
          "Individually valid camera edits produced invalid range");
  request.choices[1].choice = Side::Local;
  Require(Merge::Resolve(*review, request).has_value(),
          "Valid explicit camera resolution rejected");
}
void ResolvedBudget() {
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Resolved snapshot budget"));
  const auto id = scene.Create("Budget");
  const auto base = Bytes(scene);
  const std::vector<std::uint8_t> payload(32768, 17);
  for (std::size_t i = 0; i < 32; ++i)
    Require(scene.SetOpaqueComponent(*scene.Key(id), {20000 + i, "Unknown.Budget", payload}),
            "Local snapshot budget fixture failed");
  const auto local = Bytes(scene);
  Require(scene.ReloadBytes(base), "Budget fixture reset failed");
  for (std::size_t i = 32; i < 64; ++i)
    Require(scene.SetOpaqueComponent(*scene.Key(id), {20000 + i, "Unknown.Budget", payload}),
            "Remote snapshot budget fixture failed");
  const auto remote = Bytes(scene);
  const auto review = Merge::Prepare(base, local, remote);
  std::string error;
  Require(review && !review->Comparison().conflicts &&
              !Merge::Resolve(*review, {1, review->Identity(), {}}, &error) &&
              error.find("aggregate snapshot/value budget") != error.npos &&
              review->LocalBytes() == local && review->RemoteBytes() == remote,
          "Individually bounded sources escaped the complete resolved snapshot budget");
  Require(scene.RemoveOpaqueComponents(std::array{*scene.Key(id)}, 20063),
          "Under-budget remote fixture failed");
  const auto smaller = Bytes(scene);
  const auto bounded = Merge::Prepare(base, local, smaller);
  Require(bounded.has_value(), "Under-budget source pair rejected");
  const auto candidate = Merge::Resolve(*bounded, {1, bounded->Identity(), {}}, &error);
  runtime::World staged_world;
  editor::SceneDocument staged(staged_world, staged_world.LoadScene("Large candidate"));
  Require(candidate && candidate->bytes && staged.ReloadBytes(*candidate->bytes),
          "Supported near-limit resolved candidate rejected");
  const auto components = staged.OpaqueComponents(*staged.Key(id));
  Require(components && components->size() == 63 &&
              std::ranges::all_of(*components,
                                  [&](const auto &component) {
                                    return component.data == payload &&
                                           component.type_name == "Unknown.Budget";
                                  }),
          "Large resolved candidate lost exact unknown component bytes");
}
void GoldenMigration() {
  for (const auto name :
       {"editor1-runtime1.scene", "editor1-runtime2.scene", "editor1-runtime3.scene",
        "editor2-runtime3.scene", "editor3-runtime3.scene", "editor4-runtime3.scene"}) {
    std::ifstream input(std::filesystem::path(NEXORA_SCENE_GOLDEN_DIR) / name, std::ios::binary);
    const std::string source{std::istreambuf_iterator<char>(input), {}};
    Require(!source.empty(), "Independent migration golden missing");
    const auto review = Merge::Prepare(source, source, source);
    const auto candidate =
        review ? Merge::Resolve(*review, {1, review->Identity(), {}}) : std::nullopt;
    Require(candidate && candidate->bytes, "Supported golden migration candidate rejected");
    const auto comparison = editor::CompareSceneRevisions(source, *candidate->bytes, source);
    Require(comparison && comparison->rows.empty(),
            "Canonical merge candidate changed supported legacy source semantics");
  }
}
} // namespace
int main() {
  try {
    Run();
    Components();
    InvalidHybrids();
    GeneralRotations();
    BindingGroups();
    CameraConflict();
    ResolvedBudget();
    GoldenMigration();
    std::cout << "Owning validated semantic merge candidates passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
