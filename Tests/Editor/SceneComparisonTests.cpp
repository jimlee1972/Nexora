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
void PrefabBindings() {
  const std::string prefix = "NEXORA_EDITOR_SCENE 4\nnode 11 0 Bound\nnode 12 0 Unbound\n"
                             "opaque 11 781 \"Unknown.Provider\" 00ff11\n"
                             "prefab-placement 10000000-0000-0000-0000-000000000001 "
                             "20000000-0000-0000-0000-000000000001 ";
  const std::string suffix = "\nprefab-node 10000000-0000-0000-0000-000000000001 0 "
                             "30000000-0000-0000-0000-000000000001 11\nworld\n"
                             "NEXORA_SCENE 3 \"Bindings\" 0 2\n"
                             "11 0 0 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n"
                             "12 0 0 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
  const auto base = prefix + "7" + suffix;
  const auto local = prefix + "8" + suffix;
  const auto remote = prefix + "9" + suffix;
  const auto comparison = editor::CompareSceneRevisions(base, local, remote);
  Require(comparison && comparison->rows.size() == 1 && comparison->conflicts == 1,
          "Different retained prefab revisions compared equal");
  const auto revision = Row(*comparison, "prefabs/10000000-0000-0000-0000-000000000001/revision");
  Require(revision.base == "7" && revision.local == "8" && revision.remote == "9" &&
              revision.choice == Choice::Unresolved,
          "Prefab revision comparison lost exact source values");
  const std::string path = "prefabs/10000000-0000-0000-0000-000000000001/";
  const std::string instance = "10000000-0000-0000-0000-000000000001";
  const std::string source = "20000000-0000-0000-0000-000000000001";
  const std::string source_node = "30000000-0000-0000-0000-000000000001";
  const auto replace = [](std::string text, const std::string &old, const std::string &next) {
    const auto position = text.find(old);
    Require(position != std::string::npos && text.find(old, position + old.size()) == text.npos,
            "Independent binding fixture replacement is not unique");
    text.replace(position, old.size(), next);
    return text;
  };
  const auto maximum = prefix + "18446744073709551615" + suffix;
  const auto exact = editor::CompareSceneRevisions(base, maximum, base);
  Require(exact && exact->rows.size() == 1 &&
              Row(*exact, path + "revision").local == "18446744073709551615" &&
              Row(*exact, path + "revision").choice == Choice::Local,
          "U64 prefab revision was rounded or misclassified");
  const auto changed_source = replace(base, source, "20000000-0000-0000-0000-000000000002");
  const auto asset = editor::CompareSceneRevisions(base, base, changed_source);
  Require(asset && asset->rows.size() == 1 && Row(*asset, path + "source").choice == Choice::Remote,
          "Source asset replacement compared equal");
  const auto mapped = replace(base, source_node, "30000000-0000-0000-0000-000000000002");
  const auto mapping = editor::CompareSceneRevisions(base, mapped, base);
  Require(mapping && mapping->rows.size() == 1 &&
              Row(*mapping, path + "nodes/11/source_node").choice == Choice::Local,
          "Scoped source node replacement compared equal");
  const auto scoped =
      replace(base, " 0 " + source_node, " 2 " + source + ' ' + instance + ' ' + source_node);
  const auto reversed =
      replace(base, " 0 " + source_node, " 2 " + instance + ' ' + source + ' ' + source_node);
  const auto scopes = editor::CompareSceneRevisions(base, scoped, reversed);
  Require(scopes && scopes->rows.size() == 1 && scopes->conflicts == 1 &&
              Row(*scopes, path + "nodes/11/scope").base == "" &&
              Row(*scopes, path + "nodes/11/scope").local == source + '/' + instance &&
              Row(*scopes, path + "nodes/11/scope").remote == instance + '/' + source,
          "Ordered nested scope identity was flattened or discarded");
  const auto remapped = replace(base, source_node + " 11", source_node + " 12");
  const auto targets = editor::CompareSceneRevisions(base, remapped, base);
  Require(targets && targets->rows.size() == 6 && !Row(*targets, path + "nodes/11/present").local &&
              Row(*targets, path + "nodes/12/present").local == "true",
          "Target mapping replacement did not retain removed and added identities");
  auto removed = base;
  const auto begin = removed.find("prefab-placement ");
  removed.erase(begin, removed.find("world\n", begin) - begin);
  // Version4 requires a placement; the writer uses version3 for remaining opaque data without one.
  removed = replace(removed, "NEXORA_EDITOR_SCENE 4", "NEXORA_EDITOR_SCENE 3");
  const auto deletion = editor::CompareSceneRevisions(base, removed, local);
  Require(deletion && deletion->rows.size() == 6 && deletion->conflicts == 1 &&
              !Row(*deletion, path + "present").local &&
              Row(*deletion, path + "revision").choice == Choice::Unresolved,
          "Binding deletion versus revision edit was not an explicit conflict");
  const auto added = editor::CompareSceneRevisions(removed, base, base);
  Require(added && added->rows.size() == 6 && !added->conflicts &&
              std::ranges::all_of(added->rows,
                                  [](const auto &row) {
                                    return !row.base && row.local && row.local == row.remote &&
                                           row.choice == Choice::Shared;
                                  }),
          "Shared new binding lost absence versus empty scope distinctions");
  const auto whole = editor::CompareSceneRevisions(std::nullopt, base, std::nullopt);
  Require(whole && Row(*whole, "entities/11/opaque/781/payload_hex").local == "00ff11" &&
              Row(*whole, path + "nodes/11/scope").local == "",
          "Owning binding projection lost unrelated exact unknown bytes");
  const auto extra_node = "prefab-node " + instance + " 1 " + source + ' ' + source_node + " 12\n";
  const auto two_nodes = replace(base, "world\n", extra_node + "world\n");
  const auto first_node = "prefab-node " + instance + " 0 " + source_node + " 11\n";
  const auto reordered = replace(two_nodes, first_node + extra_node, extra_node + first_node);
  const auto equal = editor::CompareSceneRevisions(two_nodes, reordered, two_nodes);
  Require(equal && equal->rows.empty(), "Node record order fabricated binding changes");
  const std::string second_instance = "10000000-0000-0000-0000-000000000002";
  const auto changed_instance =
      replace(replace(base, "prefab-placement " + instance, "prefab-placement " + second_instance),
              "prefab-node " + instance, "prefab-node " + second_instance);
  const auto instances = editor::CompareSceneRevisions(base, changed_instance, base);
  Require(instances && instances->rows.size() == 12 && !Row(*instances, path + "present").local &&
              Row(*instances, "prefabs/" + second_instance + "/present").local == "true",
          "Persistent instance identity replacement compared equal");
  const auto second_records = "prefab-placement " + second_instance + ' ' + source + " 8\n" +
                              "prefab-node " + second_instance + " 0 " + source_node + " 12\n";
  const auto first_records = "prefab-placement " + instance + ' ' + source + " 7\n" + first_node;
  const auto two_instances = replace(base, "world\n", second_records + "world\n");
  const auto reverse_instances =
      replace(two_instances, first_records + second_records, second_records + first_records);
  const auto same_instances =
      editor::CompareSceneRevisions(two_instances, reverse_instances, two_instances);
  Require(same_instances && same_instances->rows.empty(),
          "Placement record order fabricated binding changes");
  for (const auto &invalid :
       {prefix + "0" + suffix, replace(base, source_node + " 11", source_node + " 99"),
        replace(base, source, "00000000-0000-0000-0000-000000000000"),
        replace(base, "world\n", first_node + "world\n")})
    Require(!editor::CompareSceneRevisions(base, invalid, remote),
            "Invalid binding revision/target/identity/duplicate produced a partial comparison");
}
void PrefabBindingBudgets() {
  const auto literal = [](std::size_t count, std::size_t depth) {
    std::string scene = "NEXORA_EDITOR_SCENE 4\n";
    for (std::size_t i = 1; i <= count; ++i)
      scene += "node " + std::to_string(i) + " 0 Bound\n";
    scene += "prefab-placement 10000000-0000-0000-0000-000000000001 "
             "20000000-0000-0000-0000-000000000001 1\n";
    std::string scope;
    for (std::size_t i = 1; i <= depth; ++i)
      scope += ' ' + foundation::Uuid{5, i}.ToString();
    for (std::size_t i = 1; i <= count; ++i)
      scene += "prefab-node 10000000-0000-0000-0000-000000000001 " + std::to_string(depth) + scope +
               ' ' + foundation::Uuid{3, i}.ToString() + ' ' + std::to_string(i) + '\n';
    scene += "world\nNEXORA_SCENE 3 \"Budget\" 0 " + std::to_string(count) + '\n';
    for (std::size_t i = 1; i <= count; ++i)
      scene += std::to_string(i) + " 0 0 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
    return scene;
  };
  const auto shallow = literal(2048, 0);
  const auto deep = literal(1024, 32);
  const auto oversized = literal(2048, 32);
  runtime::World world;
  editor::SceneDocument document(world, world.LoadScene("Independent budget validation"));
  Require(document.ReloadBytes(oversized) &&
              document.PrefabPlacements().front().nodes.size() == 2048,
          "Snapshot overflow fixture was not a valid supported scene");
  Require(editor::CompareSceneRevisions(shallow, shallow, shallow).has_value() &&
              editor::CompareSceneRevisions(deep, deep, deep).has_value(),
          "Valid shallow or max-depth binding snapshot rejected");
  std::string error;
  Require(!editor::CompareSceneRevisions(oversized, oversized, oversized, &error) &&
              error.find("bounded snapshot/value budget") != std::string::npos,
          "Binding metadata escaped the existing aggregate semantic snapshot budget");
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
  // Real Runtime revisions with opposite quaternion signs retain identical semantic rotation.
  runtime::World rotation_world;
  const auto rotation_scene = rotation_world.LoadScene("Rotation");
  editor::SceneDocument rotation_document(rotation_world, rotation_scene);
  const auto rotation_entity = rotation_world.CreateEntity(rotation_scene).id;
  for (const auto &rotation : {runtime::Transform{}, runtime::Transform{.qx = 1, .qw = 0},
                               runtime::Transform{.qy = .6, .qw = .8}}) {
    runtime::WorldCommandBuffer positive_commands;
    positive_commands.SetTransform(rotation_entity, rotation);
    Require(positive_commands.Apply(rotation_world), "Positive rotation fixture failed");
    const auto positive = Bytes(rotation_document);
    auto negative_rotation = rotation;
    negative_rotation.qx = -rotation.qx;
    negative_rotation.qy = -rotation.qy;
    negative_rotation.qz = -rotation.qz;
    negative_rotation.qw = -rotation.qw;
    runtime::WorldCommandBuffer negative_commands;
    negative_commands.SetTransform(rotation_entity, negative_rotation);
    Require(negative_commands.Apply(rotation_world), "Negative rotation fixture failed");
    const auto negative = Bytes(rotation_document);
    Require(positive != negative, "Quaternion fixtures must contain different actual bytes");
    const auto equivalent = editor::CompareSceneRevisions(positive, negative, positive);
    Require(equivalent && equivalent->rows.empty() && !equivalent->conflicts,
            "Equivalent quaternion signs fabricated changes/conflicts");
  }
  // Under-8-MiB legacy input must reject its 4097th node before hierarchy validation.
  std::string excessive_nodes = "NEXORA_EDITOR_SCENE 1\n";
  for (std::size_t i = 1; i <= 20000; ++i)
    excessive_nodes += "node " + std::to_string(i) + ' ' + std::to_string(i - 1) + " Deep\n";
  excessive_nodes += "world\nNEXORA_SCENE 2 \"Empty\" 0 0\n";
  Require(excessive_nodes.size() < editor::SceneComparison::kMaximumSourceBytes &&
              !editor::CompareSceneRevisions(excessive_nodes, base, base) &&
              !editor::CompareSceneRevisions(base, excessive_nodes, base) &&
              !editor::CompareSceneRevisions(base, base, excessive_nodes),
          "Oversized legacy authoring chain reached comparison ingestion");
  const auto bounded_generation = document.Generation();
  const auto bounded_bytes = Bytes(document);
  Require(!document.ReloadBytes(excessive_nodes, editor::SceneComparison::kMaximumEntities) &&
              document.Generation() == bounded_generation && Bytes(document) == bounded_bytes,
          "Bounded parser rejection changed the live document");
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
    PrefabBindings();
    PrefabBindingBudgets();
    Run();
    std::cout << "Bounded owning semantic scene comparison passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
