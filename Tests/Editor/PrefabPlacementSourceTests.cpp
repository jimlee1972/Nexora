#include "../../Engine/Editor/src/PrefabPlacementSourceTestAccess.h"
#include "Nexora/Editor/PrefabPlacementOverrides.h"
#include "Nexora/Editor/PrefabPlacementRebase.h"
#include "Nexora/Editor/ProjectPrefabPlacement.h"
#include "Nexora/Editor/SceneFiles.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <new>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
using namespace nexora;
using Assets = editor::PrefabAssets;
using Overrides = editor::PrefabPlacementOverrides;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  Require(file.good(), "Source fixture unavailable");
  return {std::istreambuf_iterator<char>(file), {}};
}
void Write(const std::filesystem::path &path, std::string_view bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  Require(file.good(), "Source fixture write failed");
}
void Write(const std::filesystem::path &path, const editor::PrefabAsset &asset) {
  const auto bytes = Assets::Encode(asset);
  Require(bytes.has_value(), "Replacement asset invalid");
  Write(path, {reinterpret_cast<const char *>(bytes->data()), bytes->size()});
}
std::map<std::filesystem::path, std::string> Files(const std::filesystem::path &root) {
  std::map<std::filesystem::path, std::string> result;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file())
      result.emplace(entry.path().lexically_relative(root), Read(entry.path()));
  return result;
}
void Dormant(editor::SceneDocument &document, runtime::Id id, int fov, int intensity,
             std::uint64_t mesh, std::uint64_t shader) {
  // Official setters reset absent component storage. Seed dormant values through actual
  // canonical serialization and the production loader, before establishing the live baseline.
  const auto prepared = *document.PrepareSave();
  const auto start = prepared.Bytes().find("world\n") + 6;
  std::istringstream input(prepared.Bytes().substr(start));
  std::string line, output = prepared.Bytes().substr(0, start);
  Require(static_cast<bool>(std::getline(input, line)) && line.starts_with("NEXORA_SCENE 3 "),
          "Dormant fixture requires canonical Runtime3 output");
  output += line + '\n';
  while (std::getline(input, line)) {
    std::istringstream fields(line);
    std::vector<std::string> tokens{std::istream_iterator<std::string>(fields), {}};
    Require(tokens.size() == 21, "Dormant fixture runtime record framing changed");
    if (tokens[0] == std::to_string(id)) {
      tokens[12] = tokens[13] = tokens[14] = "0";
      tokens[15] = std::to_string(fov);
      tokens[18] = std::to_string(intensity);
      tokens[19] = std::to_string(mesh);
      tokens[20] = std::to_string(shader);
    }
    for (std::size_t index = 0; index < tokens.size(); ++index)
      output += (index ? " " : "") + tokens[index];
    output += '\n';
  }
  Require(document.ReloadBytes(output), "Dormant fixture failed production load");
}
struct Fixture final {
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-instance-source-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup final {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(path, error);
    }
  } cleanup{root};
  editor::ProjectWorkspace workspace;
  runtime::World source_world, world;
  editor::SceneDocument source{source_world, source_world.LoadScene("Source identity")};
  editor::SceneDocument target{world, world.LoadScene("Live identity")};
  editor::PrefabAsset asset;
  runtime::Id parent{}, child{}, free{}, live{}, live_child{};
  std::uint64_t serial{100};
  foundation::Uuid Next() { return {9000, ++serial}; }
  explicit Fixture(bool variant = false, bool nested = false, std::uint64_t revision = 1) {
    Require(workspace.Create(root / std::filesystem::path(u8"Project µ"), "Instance source"),
            "Project creation failed");
    parent = source.Create("Source parent");
    child = source.Create("Source child", parent);
    Require(source.SetTransform(parent, {1, 2, 3}) && source.SetTransform(child, {.5, 1, 2}) &&
                source.SetOpaqueComponent(*source.Key(parent), {42, "Missing", {0, 255, 42}}) &&
                source.SetOpaqueComponent(*source.Key(parent), {43, "Kept missing", {13}}),
            "Source authoring fixture failed");
    Dormant(source, parent, 91, 1, 101, 102);
    const auto captured = Assets::Capture({9001, 1}, source, [&] { return Next(); });
    Require(captured.has_value(), "Source capture failed");
    asset = *captured;
    if (variant) {
      Require(Assets::Publish(workspace, asset), "Base publication failed");
      const auto derived = Assets::Capture({9001, 2}, source, [&] { return Next(); }, &asset);
      Require(derived.has_value(), "Variant capture failed");
      asset = *derived;
    }
    if (nested) {
      runtime::World nested_world;
      editor::SceneDocument nested_source(nested_world, nested_world.LoadScene("Nested"));
      nested_source.Create("Nested node");
      const auto nested_asset = Assets::Capture({9001, 3}, nested_source, [&] { return Next(); });
      Require(nested_asset && Assets::Publish(workspace, *nested_asset), "Nested source failed");
      const auto attachment =
          std::ranges::find(asset.nodes, parent, &editor::PrefabNodeIdentity::serialized_node)->id;
      asset.nested = {{{9002, 2}, attachment, {nested_asset->id, 1}}};
    }
    asset.revision = revision;
    Require(Assets::Publish(workspace, asset), "Source publication failed");
    free = target.Create("Unbound");
    const auto prepared = editor::ProjectPrefabPlacement::Prepare(workspace, asset.id, target);
    const auto placed = prepared ? editor::ProjectPrefabPlacement::Instantiate(
                                       workspace, target, *prepared, {9002, 1}, true)
                                 : std::nullopt;
    Require(placed.has_value(), "Bound placement failed");
    for (const auto &node : *placed)
      if (node.scope.empty()) {
        const auto original =
            std::ranges::find(asset.nodes, node.node, &editor::PrefabNodeIdentity::id);
        if (original != asset.nodes.end()) {
          if (original->serialized_node == parent)
            live = node.target.id;
          if (original->serialized_node == child)
            live_child = node.target.id;
        }
      }
    Require(live && live_child && live != parent,
            "Source/live identity translation fixture failed");
    Dormant(target, live, 117, 3, 501, 502);
    editor::SceneFileSession files(workspace, target);
    Require(files.SaveAs(files.Token(), "Content/Live.scene").Applied() && !target.Dirty() &&
                target.Select(std::array{live}) && target.CopySelection(),
            "Live saved baseline/clipboard failed");
    auto pose = *target.Transform(live);
    pose.x = 12;
    pose.y = 22;
    pose.sx = -2;
    pose.sy = 3;
    pose.sz = 4;
    Require(target.Rename(*target.Key(live), "Applied µ %s") && target.SetTransform(live, pose) &&
                target.SetEulerField(std::array{*target.Key(live)}, 0, 450) &&
                target.SetOpaqueComponent(*target.Key(live),
                                          {42, "Changed unknown", {0, 1, 255, 0, 42}}) &&
                target.SetOpaqueComponent(*target.Key(live), {43, "Local only", {99}}) &&
                target.SetOpaqueComponent(*target.Key(live), {44, "New local field", {8}}) &&
                target.Rename(*target.Key(live_child), "Local child") &&
                target.Rename(*target.Key(live_child), "Pending redo") && target.Undo(),
            "Live edits and pending Redo failed");
  }
  std::filesystem::path Path() const {
    return workspace.Root() / ".nexora/prefabs" / (asset.id.ToString() + ".nxprefab");
  }
  editor::PrefabPlacementOverrideReview Review(const editor::ProjectWorkspace *view = nullptr) {
    auto result = Overrides::Prepare(view ? *view : workspace, target, *target.Key(live));
    Require(result.has_value(), "Real override review failed");
    return std::move(*result);
  }
  std::size_t Row(const editor::PrefabPlacementOverrideReview &review, std::string_view field) {
    for (std::size_t index = 0; index < review.Rows().size(); ++index)
      if (review.Rows()[index].target.id == live && review.Rows()[index].field == field)
        return index;
    throw std::runtime_error("Expected changed semantic row absent: " + std::string(field));
  }
};
void Run(bool variant) {
  Fixture f(variant);
  auto review = f.Review();
  std::vector<std::size_t> rows;
  for (const auto field : {"name", "transform/position/x", "transform/position/y",
                           "authoring/euler/x", "transform/rotation/x", "transform/scale/x",
                           "camera/field_of_view", "mesh/asset", "opaque/42/payload_hex"})
    rows.push_back(f.Row(review, field));
  const auto expected = *f.target.PrepareSave();
  const auto key = *f.target.Key(f.live);
  const auto scene_path = f.workspace.Root() / "Content/Live.scene";
  const auto scene_bytes = Read(scene_path);
  std::string error;
  const auto conserved = [&] {
    Require(
        f.target.MatchesPreparedSave(expected) && f.target.Key(f.live) == key && f.target.Dirty() &&
            f.target.Selection().size() == 1 && f.target.Selection().front() == f.live &&
            f.target.PrefabPlacements().front().revision == 1 && Read(scene_path) == scene_bytes &&
            f.target.Redo() && f.target.Name(f.live_child) == "Pending redo" && f.target.Undo() &&
            f.target.MatchesPreparedSave(expected),
        "Source application changed live state/binding/baseline/selection/history/source");
  };
  const auto reject = [&](std::span<const std::size_t> selected, bool authorized = true) {
    const auto files = Files(f.workspace.Root());
    Require(!Overrides::ApplySelectedToSource(f.workspace, f.target, review, selected, authorized,
                                              &error) &&
                !error.empty() && Files(f.workspace.Root()) == files,
            "Rejected source application changed project files or lacked a diagnostic");
    conserved();
  };
  reject(rows, false);
  reject({});
  reject(std::array{rows.front(), rows.front()});
  reject(std::array{review.Rows().size()});
  reject(std::vector<std::size_t>(Overrides::kMaximumRows + 1, rows.front()));
  reject(std::array{f.Row(review, "opaque/44/payload_hex")});
  editor::ProjectWorkspace reader;
  Require(reader.Open(f.workspace.Root(), editor::ProjectAccess::ReadOnly),
          "Read-only open failed");
  const auto readonly = f.Review(&reader);
  Require(!Overrides::ApplySelectedToSource(reader, f.target, readonly, rows, true),
          "Read-only review acquired source write authority");
  conserved();
  runtime::World foreign_world;
  editor::SceneDocument foreign(foreign_world, foreign_world.LoadScene("Foreign"));
  Require(foreign.ReloadBytes(expected.Bytes()) &&
              !Overrides::ApplySelectedToSource(f.workspace, foreign, review, rows, true),
          "Byte-identical foreign document consumed live source authority");
  Require(f.target.Rename(key, "Stale") &&
              !Overrides::ApplySelectedToSource(f.workspace, f.target, review, rows, true) &&
              f.target.Undo() && f.target.Rename(*f.target.Key(f.live_child), "Pending redo") &&
              f.target.Undo(),
          "Stale local observation wrote source");
  conserved();
  const auto recovery = f.workspace.Root() / ".nexora/workspace.recovery";
  Write(recovery, "pending");
  reject(rows);
  std::filesystem::remove(recovery);
  const auto current = Read(f.Path());
  Write(f.Path(), "corrupt source");
  reject(rows);
  Write(f.Path(), current);
  runtime::World replacement_world;
  editor::SceneDocument replacement_document(replacement_world,
                                             replacement_world.LoadScene("Replacement"));
  auto replacement = f.asset;
  Require(replacement_document.ReloadBytes(replacement.scene_bytes) &&
              replacement_document.Rename(*replacement_document.Key(f.parent),
                                          "Same revision replacement"),
          "Same revision replacement fixture failed");
  replacement.scene_bytes = replacement_document.PrepareSave()->Bytes();
  Write(f.Path(), replacement);
  reject(rows);
  Write(f.Path(), f.asset);
  const auto before_fault = Files(f.workspace.Root());
  editor::PrefabPlacementSourceTestAccess::FailNextBeforePublish();
  bool allocation{};
  try {
    static_cast<void>(Overrides::ApplySelectedToSource(f.workspace, f.target, review, rows, true));
  } catch (const std::bad_alloc &) {
    allocation = true;
  }
  Require(allocation && Files(f.workspace.Root()) == before_fault,
          "Candidate allocation failure changed source files");
  conserved();
  auto stage = f.Path();
  stage += ".tmp";
  Write(stage, "foreign stage");
  Require(!Overrides::ApplySelectedToSource(f.workspace, f.target, review, rows, true) &&
              Read(stage) == "foreign stage" && Read(f.Path()) == current,
          "Occupied staging changed foreign/current source bytes");
  std::filesystem::remove(stage);
  conserved(); // A harmless retained copy of the already committed old revision is permitted.
  const auto published =
      Overrides::ApplySelectedToSource(f.workspace, f.target, review, rows, true, &error);
  Require(published && published->revision == 2 && published->id == f.asset.id &&
              published->nodes == f.asset.nodes && published->base == f.asset.base &&
              published->nested == f.asset.nested &&
              Assets::Load(f.workspace, f.asset.id) == published &&
              Assets::LoadRevision(f.workspace, {f.asset.id, 1}) == f.asset,
          "Source publication lost stable identities, revision or exact old archive");
  conserved();
  Require(!Overrides::ApplySelectedToSource(f.workspace, f.target, review, rows, true),
          "Old review published twice after the source advanced");
  runtime::World reopened_world;
  const auto reopened_scene = reopened_world.LoadScene("Reopen");
  editor::SceneDocument reopened(reopened_world, reopened_scene);
  Require(reopened.ReloadBytes(published->scene_bytes) &&
              reopened_world.FindScene(reopened_scene)->name == "Source identity" &&
              reopened.Name(f.parent) == "Applied µ %s" &&
              reopened.Transform(f.parent) == f.target.Transform(f.live) &&
              reopened.EulerAngles(f.parent) == f.target.EulerAngles(f.live) &&
              reopened.Name(f.child) == "Source child" && reopened.Parent(f.child) == f.parent &&
              reopened.Transform(f.child) == f.source.Transform(f.child),
          "Source reopen lost selected groups or changed unselected node properties");
  const auto *entity = reopened_world.FindEntity(f.parent);
  Require(entity && !entity->camera && !entity->mesh_renderer &&
              entity->camera_data.vertical_field_of_view == 117 && entity->mesh_data.mesh == 501 &&
              entity->mesh_data.material.shader == 502 && entity->light_data.intensity == 1,
          "Complete dormant component groups or unselected light values changed");
  const auto opaque = reopened.OpaqueComponents(*reopened.Key(f.parent));
  Require(opaque && opaque->size() == 2 &&
              std::ranges::find(
                  *opaque, editor::OpaqueComponent{42, "Changed unknown", {0, 1, 255, 0, 42}}) !=
                  opaque->end() &&
              std::ranges::find(*opaque, editor::OpaqueComponent{43, "Kept missing", {13}}) !=
                  opaque->end(),
          "Source publication changed selected/unselected unknown bytes");
  Require(f.target.Paste() && f.target.Name(f.target.Selection().front()) == "Source parent Copy" &&
              f.target.Undo() && f.target.MatchesPreparedSave(expected),
          "Source publication changed retained clipboard");
  const auto rebase = editor::PrefabPlacementRebase::Prepare(f.workspace, f.target, key);
  const auto resolved = rebase ? editor::PrefabPlacementRebase::Resolve(*rebase, {}) : std::nullopt;
  Require(resolved && resolved->Ready() &&
              editor::PrefabPlacementRebase::Apply(f.workspace, f.target, *resolved, true) &&
              f.target.PrefabPlacements().front().revision == 2 && f.target.Undo() &&
              f.target.MatchesPreparedSave(expected) &&
              Assets::Load(f.workspace, f.asset.id) == published,
          "Explicit existing rebase did not advance binding as one separate Undo");
}
void RemoveOpaque() {
  Fixture f;
  const std::array keys{*f.target.Key(f.live)};
  Require(f.target.RemoveOpaqueComponents(keys, 42), "Local opaque removal fixture failed");
  const auto review = f.Review();
  const auto expected = *f.target.PrepareSave();
  const auto published = Overrides::ApplySelectedToSource(
      f.workspace, f.target, review, std::array{f.Row(review, "opaque/42/payload_hex")}, true);
  Require(published && published->revision == 2 && f.target.MatchesPreparedSave(expected) &&
              Assets::LoadRevision(f.workspace, {f.asset.id, 1}) == f.asset,
          "Selected opaque removal lost live state or the exact archived payload");
  runtime::World world;
  editor::SceneDocument document(world, world.LoadScene("Removed reopen"));
  Require(document.ReloadBytes(published->scene_bytes) &&
              document.Name(f.parent) == "Source parent" &&
              document.Transform(f.parent) == f.source.Transform(f.parent),
          "Opaque-only publication changed unselected known groups");
  const auto opaque = document.OpaqueComponents(*document.Key(f.parent));
  Require(opaque && *opaque == std::vector{editor::OpaqueComponent{43, "Kept missing", {13}}},
          "Opaque-only publication removed unselected bytes or added local-only fields");
  for (const auto &node : published->nodes) {
    const auto old = std::ranges::find(f.asset.nodes, node.id, &editor::PrefabNodeIdentity::id);
    Require(old != f.asset.nodes.end() && old->serialized_node == node.serialized_node,
            "Opaque removal changed stable node identity");
    for (const auto &property : node.properties)
      Require(std::ranges::find(old->properties, property) != old->properties.end(),
              "Opaque removal invented or changed a retained field identity");
  }
  // The removed opaque field changes the source schema. Existing property-only rebase must
  // continue to reject that structural identity change, preserving the old instance binding.
  Require(!editor::PrefabPlacementRebase::Prepare(f.workspace, f.target, keys.front()) &&
              f.target.PrefabPlacements().front().revision == 1,
          "Opaque field deletion bypassed structural rebase validation");
}
void EnableComponents() {
  Fixture f;
  const auto key = *f.target.Key(f.live);
  const runtime::CameraComponent camera{75, .2, 333};
  const runtime::LightComponent light{3.25};
  runtime::MeshComponent mesh;
  mesh.mesh = std::numeric_limits<std::uint64_t>::max() - 1;
  mesh.material.shader = std::numeric_limits<std::uint64_t>::max() - 2;
  Require(f.target.SetCamera(key, camera) && f.target.SetLight(key, light) &&
              f.target.SetMeshRenderer(key, mesh),
          "Enabled component authoring fixture failed");
  const auto review = f.Review();
  const auto expected = *f.target.PrepareSave();
  const auto published = Overrides::ApplySelectedToSource(
      f.workspace, f.target, review,
      std::array{f.Row(review, "camera/enabled"), f.Row(review, "light/enabled"),
                 f.Row(review, "mesh/enabled")},
      true);
  Require(published && published->nodes == f.asset.nodes &&
              f.target.MatchesPreparedSave(expected) && f.target.Undo() &&
              !f.world.FindEntity(f.live)->mesh_renderer && f.target.Redo() &&
              f.target.MatchesPreparedSave(expected),
          "Component source publication changed identities or local history");
  runtime::World world;
  editor::SceneDocument document(world, world.LoadScene("Enabled source reopen"));
  Require(document.ReloadBytes(published->scene_bytes), "Enabled source reopen failed");
  const auto *entity = world.FindEntity(f.parent);
  Require(entity && entity->camera && entity->light && entity->mesh_renderer &&
              entity->camera_data.vertical_field_of_view == 75 &&
              entity->camera_data.near_plane == .2 && entity->camera_data.far_plane == 333 &&
              entity->light_data.intensity == 3.25 && entity->mesh_data.mesh == mesh.mesh &&
              entity->mesh_data.material.shader == mesh.material.shader &&
              document.Transform(f.parent) == f.source.Transform(f.parent) &&
              document.OpaqueComponents(*document.Key(f.parent)) ==
                  f.source.OpaqueComponents(*f.source.Key(f.parent)) &&
              Assets::LoadRevision(f.workspace, {f.asset.id, 1}) == f.asset,
          "Enabled component groups lost presence/data/64-bit references or changed other fields");
}
void RejectUnsupported() {
  for (const bool nested : {false, true}) {
    Fixture f(false, nested, nested ? 1 : std::numeric_limits<std::uint64_t>::max());
    const auto review = f.Review();
    const auto files = Files(f.workspace.Root());
    const auto expected = *f.target.PrepareSave();
    Require(!Overrides::ApplySelectedToSource(f.workspace, f.target, review,
                                              std::array{f.Row(review, "name")}, true) &&
                Files(f.workspace.Root()) == files && f.target.MatchesPreparedSave(expected),
            "Nested or exhausted source was partially published");
  }
  Fixture f;
  Require(f.target.Move(f.live_child, f.free, 0), "Structural fixture failed");
  const auto review = f.Review();
  const auto files = Files(f.workspace.Root());
  Require(!Overrides::ApplySelectedToSource(f.workspace, f.target, review,
                                            std::array{f.Row(review, "name")}, true) &&
              Files(f.workspace.Root()) == files,
          "Structural override was partially published");
}
} // namespace
int main() {
  try {
    Run(false);
    Run(true);
    RemoveOpaque();
    EnableComponents();
    RejectUnsupported();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
