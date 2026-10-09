#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/SceneComparison.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Choice = editor::SceneComparisonChoice;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Bytes(const editor::SceneDocument &document) {
  const auto prepared = document.PrepareSave();
  Require(prepared.has_value(), "Real scene preparation failed");
  return prepared->Bytes();
}
const editor::SceneComparisonRow &Row(const editor::SceneComparison &comparison,
                                      const std::string &path) {
  const auto found =
      std::ranges::find(comparison.rows, path, &editor::SceneComparisonRow::stable_path);
  Require(found != comparison.rows.end(), "Expected stable semantic field is missing");
  return *found;
}
void Run() {
  runtime::World world;
  const auto scene = world.LoadScene("Semantic scene", true);
  Require(world.Activate(scene), "Scene activation failed");
  editor::SceneDocument document(world, scene);
  const auto camera = document.CreateCamera("Camera Unicode 相機");
  const auto node = document.Create("Child", camera);
  Require(camera && node &&
              document.SetOpaqueComponent(*document.Key(camera), {8001, "Unknown.Empty", {}}),
          "Actual scene fixture failed");
  const auto base = Bytes(document);
  const std::string prefix = "entities/" + std::to_string(camera) + '/';
  const auto key = *document.Key(camera);
  Require(document.Rename(key, "Local 相機") && document.SetEulerField(std::array{key}, 1, 720) &&
              document.SetOpaqueComponent(key, {8001, "Unknown.Empty", {0, 127, 255}}),
          "Local authoring failed");
  auto transform = *document.Transform(camera);
  transform.x = 12;
  Require(document.SetTransform(camera, transform), "Local transform authoring failed");
  const auto local = Bytes(document);
  Require(document.ReloadBytes(base), "In-memory production reload failed");
  const auto remote_key = *document.Key(camera);
  Require(document.Rename(remote_key, "Remote Camera") &&
              document.SetOpaqueComponent(remote_key, {8001, "Unknown.Empty", {4}}),
          "Remote authoring failed");
  transform = *document.Transform(camera);
  transform.z = -7;
  Require(document.SetTransform(camera, transform), "Remote transform authoring failed");
  const auto remote = Bytes(document);
  const auto generation = document.Generation();
  const auto selection = std::array{node};
  Require(document.Select(selection), "Selection fixture failed");
  std::string error = "old error";
  const auto comparison = editor::CompareSceneRevisions(base, local, remote, &error);
  Require(comparison && error.empty() && comparison->conflicts == 2,
          "Actual revisions did not retain independent edits and conflicts");
  Require(Row(*comparison, prefix + "name").choice == Choice::Unresolved &&
              Row(*comparison, prefix + "name").base == "Camera Unicode 相機" &&
              Row(*comparison, prefix + "name").local == "Local 相機" &&
              Row(*comparison, prefix + "name").remote == "Remote Camera" &&
              Row(*comparison, prefix + "transform/position/x").choice == Choice::Local &&
              Row(*comparison, prefix + "transform/position/z").choice == Choice::Remote &&
              Row(*comparison, prefix + "authoring/euler/y").local == "720" &&
              Row(*comparison, prefix + "opaque/8001/payload_hex").base == "" &&
              Row(*comparison, prefix + "opaque/8001/payload_hex").local == "007fff" &&
              Row(*comparison, prefix + "opaque/8001/payload_hex").remote == "04",
          "Stable fields, authored turns or exact opaque bytes were lost");
  Require(Bytes(document) == remote && document.Generation() == generation &&
              document.Selection().size() == 1 && document.Selection().front() == node &&
              document.Undo() && document.Transform(camera)->z == 0 && document.Redo() &&
              Bytes(document) == remote,
          "Read-only comparison mutated the caller document/history/selection");
  Require(
      editor::CompareSceneRevisions(base, local, remote) == comparison &&
          std::ranges::is_sorted(comparison->rows, {}, &editor::SceneComparisonRow::stable_path),
      "Owning semantic output is not deterministic");
  auto caller_local = local;
  const auto retained = editor::CompareSceneRevisions(base, caller_local, remote);
  caller_local.assign("caller replaced its borrowed bytes");
  Require(retained == comparison, "Comparison retained a caller-owned source borrow");
  const auto unchanged = editor::CompareSceneRevisions(base, base, base);
  Require(unchanged && unchanged->rows.empty() && !unchanged->conflicts,
          "Unchanged sources fabricated differences");
  const auto shared = editor::CompareSceneRevisions(base, local, local);
  Require(shared && !shared->conflicts &&
              std::ranges::all_of(shared->rows,
                                  [](const auto &row) { return row.choice == Choice::Shared; }),
          "Identical concurrent changes were treated as conflicts");
  const auto missing = editor::CompareSceneRevisions(std::nullopt, std::nullopt, base);
  Require(missing && Row(*missing, "scene/present").base == std::nullopt &&
              Row(*missing, prefix + "opaque/8001/payload_hex").remote == "" &&
              Row(*missing, prefix + "opaque/8001/payload_hex").choice == Choice::Remote,
          "Missing source and existing empty component were conflated");
  const auto removed = editor::CompareSceneRevisions(base, std::nullopt, base);
  Require(removed && Row(*removed, "scene/present").choice == Choice::Local &&
              !Row(*removed, "scene/present").local,
          "Whole-source deletion did not retain presence semantics");
  const auto deleted_edit = editor::CompareSceneRevisions(base, std::nullopt, remote);
  Require(deleted_edit && Row(*deleted_edit, prefix + "name").choice == Choice::Unresolved,
          "Deletion versus edited object lost its conflict");
  Require(document.ReloadBytes(base) && document.Select(std::array{node}) &&
              document.DeleteSelection(),
          "Object deletion fixture failed");
  const auto deleted = editor::CompareSceneRevisions(base, Bytes(document), base);
  Require(deleted &&
              Row(*deleted, "entities/" + std::to_string(node) + "/present").local == std::nullopt,
          "Stable object deletion is absent from semantic differences");
  for (const auto &bad :
       {std::string{}, std::string("unsupported"), std::string("NEXORA_EDITOR_SCENE 999\nworld\n"),
        std::string("NEXORA_EDITOR_SCENE 3\nnode 999 0 Ghost\nworld\n"
                    "NEXORA_SCENE 3 \"Empty\" 0 0\n"),
        base.substr(0, base.find("world\n") + 6),
        std::string(editor::SceneComparison::kMaximumSourceBytes + 1, 'x')}) {
    const auto before = Bytes(document);
    Require(!editor::CompareSceneRevisions(base, bad, remote, &error) && !error.empty() &&
                Bytes(document) == before && !editor::CompareSceneRevisions(bad, base, remote) &&
                !editor::CompareSceneRevisions(base, remote, bad),
            "Rejected revision leaked output or changed live state");
  }
  const auto before = Bytes(document);
  const auto token = document.Generation();
  Require(!document.ReloadBytes("corrupt") && Bytes(document) == before &&
              document.Generation() == token && document.Undo(),
          "Rejected in-memory reload changed the document or lost Undo");
  // A genuine legacy scene is migrated by the production parser; its canonical revision
  // compares equal instead of showing the old informational parent/world-space representation.
  const std::string legacy =
      "NEXORA_EDITOR_SCENE 1\nnode 5 0 Parent\nnode 6 5 Child\nworld\n"
      "NEXORA_SCENE 2 \"Legacy\" 0 2\n"
      "5 10 0 0 0 0.70710678118654752 0 0.70710678118654752 2 2 2 0 0 0 60 0.1 1000 1 0 0\n"
      "6 10 0 -2 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
  Require(document.ReloadBytes(legacy), "Legacy scene migration failed");
  const auto migrated = editor::CompareSceneRevisions(legacy, Bytes(document), legacy);
  Require(migrated && migrated->rows.empty(), "Migration representation fabricated scene edits");
  Require(document.ReloadBytes(base), "Known field fixture reload failed");
  const auto sibling = document.Create("Second child", camera);
  Require(sibling, "Sibling fixture failed");
  const auto ordered = Bytes(document);
  runtime::WorldCommandBuffer fields;
  fields.SetCamera(camera, runtime::CameraComponent{75, .2, 200});
  fields.SetLight(node, runtime::LightComponent{3});
  fields.SetMeshRenderer(node, runtime::MeshComponent{99, {77}});
  fields.SetSiblingIndex(node, 1);
  Require(fields.Apply(world), "Real component/order update failed");
  const auto known = editor::CompareSceneRevisions(ordered, Bytes(document), ordered);
  const auto child = "entities/" + std::to_string(node) + '/';
  Require(known && Row(*known, prefix + "camera/field_of_view").local == "75" &&
              Row(*known, child + "light/intensity").local == "3" &&
              Row(*known, child + "mesh/asset").local == "99" &&
              Row(*known, child + "mesh/shader").local == "77" &&
              Row(*known, child + "sibling/index").local == "1" &&
              Row(*known, child + "sibling/index").choice == Choice::Local,
          "Known component fields or authoritative sibling order were lost");
  auto padded = base;
  padded.resize(editor::SceneComparison::kMaximumSourceBytes, ' ');
  const auto source_limit = editor::CompareSceneRevisions(base, padded, base);
  Require(source_limit && source_limit->rows.empty(), "Exact source budget rejected");
  Require(document.NewScene(), "Budget fixture reset failed");
  const auto bounded = document.Create("Bounded");
  const auto bounded_key = *document.Key(bounded);
  Require(document.SetOpaqueComponent(
              bounded_key, {9001, "Unknown.Budget", std::vector<std::uint8_t>(32768, 255)}),
          "Exact opaque value fixture failed");
  const auto limit = Bytes(document);
  const auto exact = editor::CompareSceneRevisions(std::nullopt, limit, std::nullopt);
  Require(exact && Row(*exact, "entities/" + std::to_string(bounded) + "/opaque/9001/payload_hex")
                           .local->size() == 65536,
          "Exact opaque value budget rejected");
  Require(document.SetOpaqueComponent(
              bounded_key, {9001, "Unknown.Budget", std::vector<std::uint8_t>(32769, 255)}) &&
              !editor::CompareSceneRevisions(base, Bytes(document), remote),
          "Oversized opaque semantic value admitted");
  Require(document.NewScene(), "Snapshot capacity fixture reset failed");
  const auto many = document.Create("Many opaque fields");
  const auto many_key = *document.Key(many);
  for (std::size_t i = 0; i < 63; ++i)
    Require(document.SetOpaqueComponent(
                many_key, {10000 + i, "Unknown.Snapshot", std::vector<std::uint8_t>(32768, 0)}),
            "Real snapshot capacity fixture failed");
  Require(editor::CompareSceneRevisions(std::nullopt, Bytes(document), std::nullopt).has_value(),
          "Under-budget real snapshot rejected");
  Require(document.SetOpaqueComponent(
              many_key, {10063, "Unknown.Snapshot", std::vector<std::uint8_t>(32768, 0)}) &&
              !editor::CompareSceneRevisions(std::nullopt, Bytes(document), std::nullopt),
          "Aggregate semantic snapshot overflow admitted");
  Require(document.NewScene(), "Entity capacity fixture reset failed");
  // Use real Runtime entities, including untracked nodes, rather than trusting a declared count.
  for (std::size_t i = 0; i < editor::SceneComparison::kMaximumEntities; ++i)
    static_cast<void>(world.CreateEntity(scene));
  const auto capacity = Bytes(document);
  Require(editor::CompareSceneRevisions(capacity, capacity, capacity).has_value(),
          "Actual 4096-entity semantic capacity rejected");
  static_cast<void>(world.CreateEntity(scene));
  Require(!editor::CompareSceneRevisions(capacity, Bytes(document), capacity),
          "Actual 4097-entity comparison admitted");
  const auto distinct_capacity = [](std::size_t offset) {
    runtime::World other;
    const auto reserved = other.LoadScene("Reserved identities");
    for (std::size_t i = 0; i < offset; ++i)
      static_cast<void>(other.CreateEntity(reserved));
    const auto target = other.LoadScene("Capacity");
    editor::SceneDocument revision(other, target);
    for (std::size_t i = 0; i < editor::SceneComparison::kMaximumEntities; ++i)
      static_cast<void>(other.CreateEntity(target));
    return Bytes(revision);
  };
  const auto local_capacity = distinct_capacity(16384);
  const auto remote_capacity = distinct_capacity(32768);
  Require(
      editor::CompareSceneRevisions(local_capacity, local_capacity, local_capacity).has_value() &&
          !editor::CompareSceneRevisions(capacity, local_capacity, remote_capacity),
      "Combined real revision result-row overflow admitted");
  Require(editor::CompareSceneRevisions(std::nullopt, std::nullopt, std::nullopt)->rows.empty(),
          "All-absent revisions fabricated changes");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Bounded owning semantic scene comparison passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
