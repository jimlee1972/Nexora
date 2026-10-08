#include "Nexora/Editor/EditorWorkspace.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
namespace editor = nexora::editor;
namespace runtime = nexora::runtime;
using Capture = editor::SceneDocument::RuntimeSceneCapture;

void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

std::string RuntimeFixture(std::size_t count, runtime::Id first = 100) {
  std::string result = "NEXORA_SCENE 3 \"Runtime capture\" 0 " + std::to_string(count) + '\n';
  for (std::size_t index = 0; index < count; ++index)
    result += std::to_string(first + index) +
              " 0 0 0 0 0 0 0 1 1 1 1 0 0 1 60 0.1 1000 1 18446744073709551614 "
              "18446744073709551613\n";
  return result;
}

void WriteEditorFixture(const std::filesystem::path &path, std::string_view snapshot,
                        const std::vector<runtime::Id> &nodes,
                        const editor::UnknownComponentStore &opaque) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << "NEXORA_EDITOR_SCENE 3\n";
  // Deliberately reverse Editor metadata order: association must follow Runtime storage order.
  for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
    output << "node " << *it << " 0 Authoring-only name\n";
  const auto serialized = opaque.Serialize();
  for (auto offset = serialized.find('\n') + 1; offset < serialized.size();) {
    const auto end = serialized.find('\n', offset);
    Require(end != std::string::npos, "opaque fixture lacks its record terminator");
    output << "opaque " << std::string_view(serialized).substr(offset, end - offset) << '\n';
    offset = end + 1;
  }
  output << "world\n" << snapshot;
  output.close();
  Require(!output.fail(), "Editor capture fixture write failed");
}

void VerifyOwnershipAndHistory(const std::filesystem::path &path) {
  std::optional<Capture> retained;
  {
    runtime::World world;
    const auto scene = world.LoadScene("Capture \"quoted\" scene");
    editor::SceneDocument document(world, scene);
    const auto first = document.Create("First"), second = document.Create("Second");
    const auto first_key = *document.Key(first), second_key = *document.Key(second);
    Require(document.SetMeshRenderer(
                first_key, runtime::MeshComponent{std::numeric_limits<runtime::Id>::max(),
                                                  {std::numeric_limits<runtime::Id>::max() - 1}}),
            "full-width Runtime mesh fixture failed");
    const editor::OpaqueComponent high{std::numeric_limits<runtime::TypeId>::max(),
                                       std::string("legacy\x80\xff", 8),
                                       {0, 255, 13, 10, 127, 128}};
    const editor::OpaqueComponent low{2, "Lower type", {9, 8, 7}};
    Require(document.SetOpaqueComponent(first_key, high) &&
                document.SetOpaqueComponent(first_key, low) &&
                document.Select(std::array{second, first}) && document.Save(path),
            "capture ownership fixture could not be authored/saved");
    const auto selected = std::vector(document.Selection().begin(), document.Selection().end());
    std::string error = "stale error";
    retained = document.CaptureRuntimeScene(&error);
    Require(retained && error.empty() && retained->scene == scene &&
                retained->document_generation == document.Generation() &&
                retained->runtime_snapshot == *world.SaveScene(scene) &&
                retained->nodes.size() == 2 && retained->nodes[0].key == first_key &&
                retained->nodes[1].key == second_key &&
                retained->nodes[0].opaque == std::vector{low, high} &&
                retained->nodes[1].opaque.empty() && !document.Dirty() &&
                std::vector(document.Selection().begin(), document.Selection().end()) == selected,
            "capture altered saved state/selection or lost exact bytes/full opaque association");
    Require(document.Rename(first_key, "Changed only Editor name") &&
                document.SetTransformValues(std::array{second_key}, {}, {0, 0, 720}) &&
                document.CaptureRuntimeScene() == retained,
            "authoring-only names/Euler hints leaked into Runtime capture identity/data");
    auto changed = *document.Transform(first);
    changed.x = 42.125;
    Require(document.SetTransform(first, changed) && document.Dirty() && document.Undo(),
            "capture history fixture failed");
    const auto before_redo = document.CaptureRuntimeScene();
    Require(before_redo == retained && document.Redo() &&
                document.CaptureRuntimeScene()->runtime_snapshot != retained->runtime_snapshot &&
                document.Generation() == retained->document_generation && document.Undo(),
            "capture cleared Redo or confused document identity with an authoring revision");
    Require(document.SetOpaqueComponent(first_key, {high.type, "Replacement", {1}}) &&
                retained->nodes[0].opaque == std::vector{low, high},
            "capture borrowed opaque data across later edits");
    Require(document.NewScene() && document.Generation() != retained->document_generation &&
                document.CaptureRuntimeScene()->nodes.empty() && document.Reload(path) &&
                document.CaptureRuntimeScene()->document_generation !=
                    retained->document_generation,
            "capture did not survive New/Reload document generation boundaries");
  }
  runtime::World reopened;
  const auto scene = reopened.LoadSceneSnapshot(retained->runtime_snapshot);
  Require(scene && reopened.SaveScene(*scene) == retained->runtime_snapshot &&
              retained->nodes[0].opaque.back().data ==
                  std::vector<std::uint8_t>{0, 255, 13, 10, 127, 128},
          "capture borrowed World/Document storage across destruction");
}

void VerifyTrackedSubsetAndLargeScene(const std::filesystem::path &path) {
  runtime::World world;
  const auto scene = world.LoadSceneSnapshot(RuntimeFixture(3, (runtime::Id{1} << 63) + 100));
  Require(scene.has_value(), "full-width Runtime scene fixture did not load");
  editor::SceneDocument document(world, *scene);
  const auto untracked = document.CaptureRuntimeScene();
  Require(untracked && untracked->nodes.empty() &&
              untracked->runtime_snapshot == *world.SaveScene(*scene),
          "constructor capture fabricated metadata or dropped untracked Runtime entities");
  const auto id = world.FindScene(*scene)->entities[1].id;
  editor::UnknownComponentStore opaque;
  Require(opaque.Set(id, {100, "Opaque subset", {0, 4, 255}}), "subset payload fixture failed");
  WriteEditorFixture(path, *world.SaveScene(*scene), {id}, opaque);
  Require(document.Reload(path), "tracked subset fixture did not reload");
  const auto captured = document.CaptureRuntimeScene();
  Require(captured && captured->nodes.size() == 1 && captured->nodes[0].key.id == id &&
              captured->nodes[0].opaque[0].data == std::vector<std::uint8_t>{0, 4, 255} &&
              captured->runtime_snapshot == *world.SaveScene(*scene),
          "tracked subset capture dropped untracked components or reassociated opaque data");

  runtime::World large;
  const auto large_scene = large.LoadSceneSnapshot(
      RuntimeFixture(editor::SceneDocument::kMaximumRuntimeCaptureEntities));
  Require(large_scene.has_value(), "100k Runtime fixture did not load");
  editor::SceneDocument large_document(large, *large_scene);
  const auto maximum = large_document.CaptureRuntimeScene();
  Require(maximum && maximum->nodes.empty() &&
              maximum->runtime_snapshot == *large.SaveScene(*large_scene),
          "100k functional capture failed or invented Editor metadata");
  large.CreateEntity(*large_scene);
  std::string error;
  Require(!large_document.CaptureRuntimeScene(&error) && error.find("100000") != std::string::npos,
          "capture did not reject entity-count overflow before serialization");
}

void VerifyOpaqueBoundsAndOrdering(const std::filesystem::path &path) {
  editor::UnknownComponentStore opaque;
  std::vector<runtime::Id> nodes;
  for (std::size_t entity = 0; entity < 64; ++entity) {
    nodes.push_back(100 + entity);
    for (runtime::TypeId type = 64; type != 0; --type)
      Require(opaque.Set(nodes.back(), {type, "T", {static_cast<std::uint8_t>(entity)}}),
              "maximum opaque-record fixture failed");
  }
  WriteEditorFixture(path, RuntimeFixture(nodes.size()), nodes, opaque);
  runtime::World world;
  const auto scene = world.LoadScene("Opaque limits");
  editor::SceneDocument document(world, scene);
  Require(document.Reload(path), "maximum opaque-record fixture did not reload");
  const auto captured = document.CaptureRuntimeScene();
  Require(captured && captured->nodes.size() == nodes.size(),
          "maximum opaque-record capture failed");
  std::unordered_set<runtime::Id> ids;
  std::size_t records = 0;
  for (std::size_t index = 0; index < captured->nodes.size(); ++index) {
    const auto &node = captured->nodes[index];
    Require(node.key.id == nodes[index] && node.key.entity_generation != 0 &&
                node.key.document_generation == captured->document_generation &&
                ids.insert(node.key.id).second && node.opaque.size() == 64,
            "capture keys/order/cardinality are invalid");
    for (std::size_t type = 0; type < node.opaque.size(); ++type)
      Require(node.opaque[type].type == type + 1 &&
                  node.opaque[type].data ==
                      std::vector<std::uint8_t>{static_cast<std::uint8_t>(index)},
              "capture opaque types are not uniquely sorted or lost entity association");
    records += node.opaque.size();
  }
  Require(records == 4096 &&
              !document.SetOpaqueComponent(captured->nodes.front().key, {65, "Extra", {1}}) &&
              document.CaptureRuntimeScene() == captured,
          "opaque record overflow changed the last-good capture");

  editor::UnknownComponentStore maximum_payload;
  const std::string longest_name(256, static_cast<char>(0xff));
  for (runtime::TypeId type = 1; type <= 16; ++type) {
    const auto size =
        editor::UnknownComponentStore::kMaximumComponentBytes - (type == 16 ? 4096 : 0);
    Require(maximum_payload.Set(100, {type, longest_name, std::vector<std::uint8_t>(size, 255)}),
            "exact total opaque payload fixture failed");
  }
  WriteEditorFixture(path, RuntimeFixture(1), {100}, maximum_payload);
  Require(document.Reload(path), "exact opaque payload fixture did not reload");
  const auto maximum = document.CaptureRuntimeScene();
  Require(maximum && maximum->nodes.size() == 1 && maximum->nodes[0].opaque.size() == 16 &&
              maximum->nodes[0].opaque.front().type_name == longest_name &&
              maximum->nodes[0].opaque.front().data.size() == 1024 * 1024,
          "capture rejected exact raw names+payload limit or legacy non-UTF type names");
  auto too_large = maximum->nodes[0].opaque.back();
  too_large.data.push_back(0);
  Require(!document.SetOpaqueComponent(maximum->nodes[0].key, std::move(too_large)) &&
              document.CaptureRuntimeScene() == maximum,
          "names+payload overflow changed the previous owning capture");
  auto oversized_component = maximum->nodes[0].opaque.front();
  oversized_component.data.push_back(0);
  Require(!document.SetOpaqueComponent(maximum->nodes[0].key, std::move(oversized_component)) &&
              document.CaptureRuntimeScene() == maximum,
          "individual component overflow changed the previous owning capture");
  for (const auto &bad_name : {std::string("CR\rName"), std::string("LF\nName"),
                               std::string("NUL\0Name", 8), std::string(257, 'x')})
    Require(!document.SetOpaqueComponent(maximum->nodes[0].key, {1, bad_name, {}}) &&
                document.CaptureRuntimeScene() == maximum,
            "invalid opaque type-name admission changed the capture");
}

void VerifyInvalidScenesAndByteAuthority() {
  std::string error = "stale";
  runtime::World missing;
  editor::SceneDocument no_scene(missing, 999);
  Require(!no_scene.CaptureRuntimeScene(&error) && !error.empty(),
          "missing scene capture succeeded");
  runtime::World world;
  const auto scene = world.LoadScene("Lifecycle");
  editor::SceneDocument document(world, scene);
  const auto id = document.Create("Tracked");
  runtime::SceneEditor external(world);
  Require(external.DestroyEntity(scene, id), "missing tracked-node fixture failed");
  world.CreateEntity(scene); // Keeps counts equal while the tracked identity is absent.
  Require(!document.CaptureRuntimeScene(&error) && error.find("tracked node") != std::string::npos,
          "capture accepted a missing tracked identity with equal counts");
  const auto foreign = world.LoadScene("Foreign");
  world.CreateEntity(foreign).id = id;
  Require(world.FindEntity(id) && !document.CaptureRuntimeScene(&error),
          "capture used World-wide FindEntity to accept a foreign tracked identity");

  runtime::World lifecycle;
  const auto live = lifecycle.LoadScene("Inactive");
  editor::SceneDocument live_document(lifecycle, live);
  Require(live_document.CaptureRuntimeScene().has_value() && lifecycle.Activate(live) &&
              live_document.CaptureRuntimeScene().has_value() && lifecycle.RequestUnload(live) &&
              !live_document.CaptureRuntimeScene(&error),
          "capture lifecycle admission failed");
  lifecycle.EndFrame();
  Require(!live_document.CaptureRuntimeScene(&error), "unloaded scene capture succeeded");
  auto play = world.CloneForPlay();
  editor::SceneDocument play_document(play, scene);
  Require(!play_document.CaptureRuntimeScene(&error) &&
              error.find("Editor World") != std::string::npos,
          "Play World capture was admitted");

  runtime::World identity;
  const auto invalid = identity.LoadScene("Identity");
  editor::SceneDocument identity_document(identity, invalid);
  auto &zero_identity = identity.CreateEntity(invalid);
  zero_identity.id = 0;
  Require(!identity_document.CaptureRuntimeScene(&error), "zero Runtime identity was admitted");
  zero_identity.id = 17;
  identity.CreateEntity(invalid).id = 17;
  Require(!identity_document.CaptureRuntimeScene(&error),
          "duplicate Runtime identities were admitted");

  runtime::World bytes;
  const auto bounded_scene =
      bytes.LoadScene(std::string(editor::SceneDocument::kMaximumRuntimeCaptureBytes, 'x'));
  editor::SceneDocument bounded_document(bytes, bounded_scene);
  Require(!bytes.SaveScene(bounded_scene, editor::SceneDocument::kMaximumRuntimeCaptureBytes) &&
              !bounded_document.CaptureRuntimeScene(&error) &&
              error.find("64 MiB") != std::string::npos,
          "capture ignored the authoritative World output-byte bound");
}
} // namespace

int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-scene-runtime-capture-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup final {
    std::filesystem::path root;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(root, error);
    }
  } cleanup{root};
  try {
    std::filesystem::create_directories(root);
    VerifyOwnershipAndHistory(root / "ownership.scene");
    VerifyTrackedSubsetAndLargeScene(root / "subset.scene");
    VerifyOpaqueBoundsAndOrdering(root / "opaque.scene");
    VerifyInvalidScenesAndByteAuthority();
    std::cout << "Owning bounded Runtime scene capture passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
