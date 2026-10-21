#include "Nexora/Editor/EditorWorkspace.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary);
  Require(file.good(), "Golden/source file unavailable");
  return {std::istreambuf_iterator<char>(file), {}};
}
void Write(const std::filesystem::path &path, std::string_view bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  Require(file.good(), "Temporary source write failed");
}
void Verify(editor::SceneDocument &document, runtime::World &world, int version,
            int runtime_version) {
  Require(document.Nodes().size() == 2 && document.Name(11) == "Parent µ" &&
              document.Name(12) == "Child 子" && document.Parent(12) == runtime::Id{11},
          "Golden migration lost names or hierarchy");
  runtime::Transform parent{10, 0, 0}, child{1, 0, 0}, world_child{11, 0, 0};
  if (runtime_version == 2) {
    parent.sx = parent.sy = parent.sz = 2;
    child.x = .5;
    child.sx = -.5;
    child.sy = child.sz = .5;
  } else if (runtime_version == 3)
    child.sx = -1;
  if (runtime_version >= 2)
    world_child.sx = -1;
  Require(document.Transform(11) == parent && document.Transform(12) == child &&
              world.WorldTransform(12) == world_child,
          "Golden migration changed independent local/world pose or scale expectations");
  Require(document.EulerAngles(11) == editor::EulerDegrees{version >= 2 ? 720. : 0., 0., 0.},
          "Golden migration lost authored Euler turns");
  const auto opaque = document.OpaqueComponents(*document.Key(12));
  Require(opaque && opaque->size() == (version >= 3 ? 1U : 0U), "Golden opaque presence changed");
  if (version >= 3)
    Require(opaque->front() == editor::OpaqueComponent{42, "Plugin.未知 %s", {0, 1, 127, 255, 42}},
            "Golden unknown identity/raw bytes changed");
  const auto placements = document.PrefabPlacements();
  Require(placements.size() == (version == 4 ? 1U : 0U), "Golden placement presence changed");
  if (version == 4)
    Require(placements.front().instance.ToString() == "10000000-0000-0000-0000-000000000001" &&
                placements.front().source.ToString() == "20000000-0000-0000-0000-000000000001" &&
                placements.front().revision == 7 && placements.front().nodes.size() == 1 &&
                placements.front().nodes.front().scope.empty() &&
                placements.front().nodes.front().target == 12 &&
                placements.front().nodes.front().source_node.ToString() ==
                    "30000000-0000-0000-0000-000000000001",
            "Golden persistent binding changed");
}
void Run(const std::filesystem::path &fixtures) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-scene-golden-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  struct Cleanup final {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(path, error);
    }
  } cleanup{root};
  const std::array names{"editor1-runtime1.scene", "editor1-runtime2.scene",
                         "editor1-runtime3.scene", "editor2-runtime3.scene",
                         "editor3-runtime3.scene", "editor4-runtime3.scene"};
  for (std::size_t index = 0; index < names.size(); ++index) {
    const int version = index < 3 ? 1 : static_cast<int>(index) - 1;
    const int runtime_version = index < 3 ? static_cast<int>(index) + 1 : 3;
    const auto original = Read(fixtures / names[index]);
    const auto source = root / names[index];
    Write(source, original);
    runtime::World world;
    editor::SceneDocument document(world, world.LoadScene("Placeholder"));
    Require(document.Reload(source) && !document.Dirty(), "Supported golden failed to load");
    Verify(document, world, version, runtime_version);
    // Authoring metadata is a subset: untracked Runtime entities remain legitimate and persist.
    auto extra = original;
    const std::string count = "\"Golden\" 0 2\n";
    extra.replace(extra.find(count), count.size(), "\"Golden\" 0 3\n");
    extra += runtime_version == 1   ? "13 20 0 0 0 0 0 60 0.1 1000 1 0 0\n"
             : runtime_version == 2 ? "13 20 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n"
                                    : "13 0 20 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n";
    runtime::World extra_world;
    editor::SceneDocument extra_document(extra_world, extra_world.LoadScene("Extra"));
    Require(extra_document.ReloadBytes(extra) && extra_world.FindEntity(13) &&
                extra_world.FindEntity(13)->transform.x == 20.,
            "Valid Runtime-only entity was rejected");
    Verify(extra_document, extra_world, version, runtime_version);
    const auto extra_saved = *extra_document.PrepareSave();
    Require(extra_document.ReloadBytes(extra_saved.Bytes()) && extra_world.FindEntity(13) &&
                extra_world.FindEntity(13)->transform.x == 20.,
            "Save/reopen lost the Runtime-only entity");
    if (index == 2) {
      const auto untracked = "NEXORA_EDITOR_SCENE 1\n" + original.substr(original.find("world\n"));
      Require(extra_document.ReloadBytes(untracked) && extra_document.Nodes().empty() &&
                  extra_world.FindEntity(11) && extra_world.FindEntity(12),
              "Valid scene with no authoring nodes was rejected");
    }
    Require(Read(source) == original && Read(fixtures / names[index]) == original,
            "Reading/upgrading a golden changed its source");
    const auto upgrade = document.PrepareSave();
    Require(upgrade.has_value(), "Golden upgrade preparation failed");
    const auto output = root / "upgraded.scene";
    Require(document.Save(output) && Read(output) == upgrade->Bytes(), "Golden atomic save failed");
    runtime::World reopened_world;
    editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Fresh"));
    Require(reopened.Reload(output) && !reopened.Dirty() &&
                reopened.PrepareSave()->Bytes() == upgrade->Bytes(),
            "Golden upgrade/save/reopen was not deterministic");
    Verify(reopened, reopened_world, version, runtime_version);
    const auto key = *document.Key(11), child_key = *document.Key(12);
    const std::array selected{runtime::Id{11}, runtime::Id{12}};
    Require(document.Select(selected) && document.CopySelection() &&
                document.Rename(key, "Dirty") && document.Rename(child_key, "Redo") &&
                document.Undo() && document.Dirty(),
            "Live rejection/history fixture failed");
    const auto expected = *document.PrepareSave();
    auto stage = output;
    stage += ".tmp";
    Write(stage, "foreign occupied staging");
    Require(!document.SavePrepared(output, expected) && document.MatchesPreparedSave(expected) &&
                document.Dirty() && Read(stage) == "foreign occupied staging" &&
                Read(output) == upgrade->Bytes(),
            "Failed publication changed staging, last-good output or the dirty baseline");
    std::filesystem::remove(stage);
    const auto unrelated = world.LoadScene("Unrelated");
    world.CreateEntity(unrelated).transform.x = 123.;
    const auto unrelated_bytes = world.SaveScene(unrelated);
    std::size_t rejected{};
    const auto reject = [&](std::string_view candidate,
                            std::string_view kind = "required truncation") {
      Write(source, candidate);
      if (document.Reload(source) || document.ReloadBytes(candidate))
        throw std::runtime_error(std::string(names[index]) + ": accepted " + std::string(kind));
      Require(document.MatchesPreparedSave(expected) && document.Key(11) == key &&
                  document.Key(12) == child_key && document.Generation() == expected.Generation() &&
                  std::ranges::equal(document.Selection(), selected) && document.Dirty() &&
                  world.SaveScene(unrelated) == unrelated_bytes && Read(source) == candidate &&
                  Read(output) == upgrade->Bytes(),
              "Rejected load changed last-good source/document/generations/selection");
      Require(document.Redo() && document.Name(12) == "Redo" && document.Undo() &&
                  document.MatchesPreparedSave(expected) && document.Undo() && !document.Dirty() &&
                  document.Redo() && document.MatchesPreparedSave(expected) && document.Dirty(),
              "Rejected load changed Undo/Redo or the saved baseline");
      ++rejected;
    };
    // The final required field is a one-byte zero. Omitting only trailing whitespace remains legal.
    const auto complete = original.find_last_not_of(" \t\r\n") + 1;
    for (std::size_t cut = 0; cut < complete; ++cut)
      reject(std::string_view(original).substr(0, cut));
    reject(original + "unexpected trailing bytes", "trailing garbage");
    auto corrupt = original;
    corrupt.replace(0, std::string("NEXORA_EDITOR_SCENE 1").size(), "NEXORA_EDITOR_SCENE 99");
    reject(corrupt, "unsupported version");
    corrupt = original;
    corrupt.insert(corrupt.find("world\n"), "node 11 0 Duplicate\n");
    reject(corrupt, "duplicate node");
    corrupt = original;
    corrupt.replace(corrupt.find("node 12 "), 7, "node 99 ");
    reject(corrupt, "orphan authoring node");
    // Clipboard is checked after the rejected-load sequence without destroying the Redo checks.
    Require(document.Paste() && document.Nodes().size() == 4 && document.Selection().size() == 1 &&
                document.Name(document.Selection().front()) == "Parent µ Copy" && document.Undo() &&
                document.MatchesPreparedSave(expected),
            "Rejected loads changed the retained clipboard");
    runtime::World eof_world;
    editor::SceneDocument eof(eof_world, eof_world.LoadScene("EOF"));
    Require(eof.ReloadBytes(std::string_view(original).substr(0, complete)),
            "Legal final whitespace omission was rejected as corruption");
    Write(source, original);
    Require(Read(fixtures / names[index]) == original && Read(output) == upgrade->Bytes(),
            "Corruption suite changed original fixtures or last-good output");
    std::cout << names[index] << ": " << rejected << " rejected inputs preserve live state\n";
  }
}
} // namespace
int main(int argc, char **argv) {
  try {
    Require(argc == 2, "Expected golden fixture directory");
    Run(std::filesystem::path(argv[1]));
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
