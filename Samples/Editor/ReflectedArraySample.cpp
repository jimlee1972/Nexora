#include "Nexora/Editor/ReflectedInspector.h"
#include "Nexora/Editor/SceneFiles.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using namespace nexora;
using Action = editor::ReflectedArrayAction;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
void Put(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint64_t value) {
  for (std::size_t i = 0; i < 8; ++i)
    bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8 * i));
}
struct TemporaryProject final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-editor-sdk-sample-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  TemporaryProject() { std::filesystem::create_directories(root); }
  ~TemporaryProject() {
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
};
void Run() {
  // Only public SDK headers. Project/document/file owners outlive every borrowed session.
  TemporaryProject temporary;
  editor::ProjectWorkspace writer, reader;
  Require(writer.Create(temporary.root, "Counted reflection SDK"),
          "Cannot create temporary project");
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Reflected sample"));
  editor::SceneFileSession files(writer, scene);
  const auto key = *scene.Key(scene.Create("Counted values"));
  const std::array selection{key};
  Require(scene.Select(selection), "Cannot select the authoring target");

  // Version-two portable layout:16 fixed bytes, u64LE count at8, i64LE tail follows at16.
  // The first8 bytes are unknown caller-owned data, not a native container or plugin object.
  const editor::ReflectedComponent schema{
      501, "Sample.Counted", 16, {}, {{"values", editor::ReflectedKind::Integer, 8, 4, {}}}};
  editor::OpaqueComponent component{schema.type, schema.name, std::vector<std::uint8_t>(16)};
  component.data[0] = 0x42;
  component.data[7] = 0xa5;
  const editor::OpaqueComponent unavailable{502, "Unavailable.Provider", {0xff, 0, 0x11}};
  Require(scene.SetOpaqueComponent(key, component) && scene.SetOpaqueComponent(key, unavailable),
          "Cannot seed portable component bytes");
  editor::ReflectedInspector inspector;
  Require(inspector.SetComponents({schema}), "Portable reflection catalog rejected");
  const std::filesystem::path relative = ".nexora/scenes/Reflection.scene";
  Require(files.SaveAs(files.Token(), relative).Applied(), "Initial scoped save failed");
  const auto original = scene.PrepareSave();
  const auto initial = inspector.Inspect(scene, selection, schema.type);
  Require(initial &&
              !inspector.ApplyArray(scene, *initial, {Action::Append, 0, 0, 0, std::int64_t{42}},
                                    false) &&
              scene.MatchesPreparedSave(*original),
          "Denied authoring changed bytes or history");

  // Embedders supply current writer/owned-document/Play/recovery authority at each transaction.
  const auto edit = [&](editor::ReflectedArrayEdit request) {
    const auto current = inspector.Inspect(scene, selection, schema.type); // Owning observation.
    Require(current && inspector.ApplyArray(scene, *current, request, writer.Writable()),
            "Current authorized array operation rejected");
  };
  edit({Action::Append, 0, 0, 0, std::int64_t{42}});
  Require(!inspector.ApplyArray(scene, *initial, {Action::Clear, 0}, writer.Writable()),
          "Old observation retargeted changed component bytes");
  edit({Action::Append, 0, 0, 0, std::int64_t{-7}});
  edit({Action::Move, 0, 0, 1});
  const auto moved = scene.PrepareSave();
  Require(scene.Undo() && scene.Redo() && scene.MatchesPreparedSave(*moved),
          "Reorder did not preserve one Undo/Redo transaction");
  const auto current = inspector.Inspect(scene, selection, schema.type);
  Require(current && current->properties.size() == 2 &&
              std::get<std::int64_t>(current->properties[0].value) == -7 &&
              std::get<std::int64_t>(current->properties[1].value) == 42 &&
              inspector.Apply(scene, *current, 0, std::int64_t{99}, writer.Writable()),
          "Current counted element edit failed");
  auto expected = component;
  expected.data.resize(32);
  Put(expected.data, 8, 2);
  Put(expected.data, 16, 99);
  Put(expected.data, 24, 42);
  const auto exact = [&](const editor::SceneDocument &document,
                         editor::SceneDocument::NodeKey node) {
    const auto all = document.OpaqueComponents(node);
    return all && all->size() == 2 && std::ranges::find(*all, expected) != all->end() &&
           std::ranges::find(*all, unavailable) != all->end();
  };
  Require(exact(scene, key) && files.Save(files.Token()).Applied() && !scene.Dirty(),
          "Save changed unknown prefix/provider bytes or counted values");

  Require(reader.Open(temporary.root, editor::ProjectAccess::ReadOnly),
          "Cannot open read-only project");
  runtime::World fresh_world;
  editor::SceneDocument reopened(fresh_world, fresh_world.LoadScene("Fresh sample"));
  editor::SceneFileSession fresh_files(reader, reopened);
  Require(fresh_files.Open(fresh_files.Token(), relative).Applied(), "Fresh scoped reopen failed");
  const auto reopened_key = *reopened.Key(key.id);
  const std::array reopened_selection{reopened_key};
  Require(reopened.Select(reopened_selection) && exact(reopened, reopened_key),
          "Fresh World lost exact unknown/count/tail data");
  const auto readonly = inspector.Inspect(reopened, reopened_selection, schema.type);
  Require(readonly && !reader.Writable() &&
              !inspector.ApplyArray(reopened, *readonly, {Action::Clear, 0}, reader.Writable()) &&
              !reopened.Dirty() && exact(reopened, reopened_key),
          "Read-only reopen granted authoring authority");
}
} // namespace
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--help") {
    std::cout << "Run without arguments to exercise counted reflection in a temporary project.\n";
    return 0;
  }
  if (argc != 1) {
    std::cerr << "Usage: NexoraEditorReflectedArraySample [--help]\n";
    return 2;
  }
  try {
    Run();
    std::cout
        << "Counted reflection: authority, stale observations, append/edit/reorder, Undo/Redo, "
           "scoped Save and read-only reopen passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
