#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/ReflectedInspector.h"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  using K = editor::ReflectedKind;
  editor::ReflectedComponent schema{91,
                                    "Plugin.Properties",
                                    160,
                                    {{"enabled", K::Boolean, 0},
                                     {"integer", K::Integer, 8},
                                     {"number", K::Number, 16},
                                     {"mode", K::Enum, 24, 1, {{0, "Off"}, {1, "On"}}},
                                     {"permissions", K::Flags, 32, 1, {{1, "Read"}, {2, "Write"}}},
                                     {"vector", K::Vector3, 40},
                                     {"color", K::Color, 64},
                                     {"entity", K::EntityReference, 96},
                                     {"asset", K::AssetReference, 104},
                                     {"samples", K::Number, 128, 2},
                                     {"nested.amount", K::Number, 144}}};
  editor::ReflectedInspector inspector;
  Require(inspector.SetComponents({schema}), "schema rejected");
  const auto revision = inspector.Revision();
  auto bad = schema;
  bad.fields[1].offset = 0;
  Require(!inspector.SetComponents({bad}) && inspector.Revision() == revision,
          "overlap changed catalog");
  bad = schema;
  bad.fields[0].elements = 0;
  Require(!inspector.SetComponents({bad}), "zero array accepted");
  bad = schema;
  bad.fields[3].choices[1].value = 0;
  Require(!inspector.SetComponents({bad}), "duplicate enum accepted");
  bad = schema;
  bad.fields[4].choices[1].value = 3;
  Require(!inspector.SetComponents({bad}), "non-bit flags accepted");
  bad = schema;
  bad.fields[1].offset = std::numeric_limits<std::size_t>::max();
  Require(!inspector.SetComponents({bad}), "overflow offset accepted");
  Require(!inspector.SetComponents({schema, schema}), "duplicate type accepted");
  runtime::World world;
  const auto sid = world.LoadScene("Reflected properties");
  editor::SceneDocument scene(world, sid);
  const auto a = scene.Create("A"), b = scene.Create("B");
  std::array keys{*scene.Key(a), *scene.Key(b)};
  editor::OpaqueComponent payload{schema.type, schema.name,
                                  std::vector<std::uint8_t>(schema.bytes)};
  payload.data[1] = 255;
  payload.data[159] = 127;
  Require(scene.SetOpaqueComponent(keys[0], payload) &&
              scene.SetOpaqueComponent(keys[1], payload) &&
              scene.SetOpaqueComponent(keys[0], {999, "Missing.Plugin", {0, 255, 127}}) &&
              scene.Select(keys),
          "fixture failed");
  const auto missing = scene.OpaqueComponents(keys[0])->back();
  auto second = payload;
  second.data[0] = 1;
  Require(scene.SetOpaqueComponent(keys[1], second), "mixed fixture failed");
  auto observation = inspector.Inspect(scene, keys, schema.type);
  Require(observation && observation->properties.size() == 12 && observation->properties[0].mixed &&
              observation->properties[10].path == "samples[1]" &&
              observation->properties[11].path == "nested.amount",
          "owning mixed/array/nested snapshot failed");
  const auto original = scene.OpaqueComponents(keys[0]);
  const std::array<editor::ReflectedValue, 12> values{true,
                                                      std::int64_t{-42},
                                                      2.5,
                                                      std::uint64_t{1},
                                                      std::uint64_t{3},
                                                      std::array<double, 4>{1, 2, 3, 0},
                                                      std::array<double, 4>{.2, .3, .4, 1},
                                                      std::uint64_t{0xffffffffffffffffULL},
                                                      foundation::Uuid{7, 9},
                                                      8.5,
                                                      9.5,
                                                      10.5};
  for (std::size_t field = 0; field < values.size(); ++field) {
    observation = inspector.Inspect(scene, keys, schema.type);
    Require(observation && inspector.Apply(scene, *observation, field, values[field], true),
            "atomic property edit failed");
    auto after = inspector.Inspect(scene, keys, schema.type);
    Require(after && !after->properties[field].mixed &&
                after->properties[field].value == values[field] &&
                scene.OpaqueComponents(keys[0])->front().data[1] == 255 &&
                scene.OpaqueComponents(keys[0])->front().data[159] == 127 &&
                scene.OpaqueComponents(keys[0])->back() == missing,
            "edit changed padding/unknown bytes or lost full-width value");
    Require(scene.Undo() && scene.OpaqueComponents(keys[0]) == original && scene.Redo() &&
                inspector.Inspect(scene, keys, schema.type)->properties[field].value ==
                    values[field] &&
                scene.Undo(),
            "single-step batch Undo/Redo failed");
  }
  observation = inspector.Inspect(scene, keys, schema.type);
  Require(inspector.Apply(scene, *observation, 2, 0.0, true) && scene.Redo() &&
              inspector.Inspect(scene, keys, schema.type)->properties[11].value ==
                  editor::ReflectedValue{10.5} &&
              scene.Undo(),
          "no-op reflected edit erased Redo history");
  observation = inspector.Inspect(scene, keys, schema.type);
  Require(
      !inspector.Apply(scene, *observation, 0, false, false) &&
          !inspector.Apply(scene, *observation, 0, 2.0, true) &&
          !inspector.Apply(scene, *observation, 2, std::numeric_limits<double>::infinity(), true) &&
          !inspector.Apply(scene, *observation, 3, std::uint64_t{3}, true) &&
          !inspector.Apply(scene, *observation, 4, std::uint64_t{4}, true) &&
          !inspector.Apply(scene, *observation, 6, std::array<double, 4>{2, 0, 0, 0}, true) &&
          scene.OpaqueComponents(keys[0]) == original,
      "invalid/unauthorized edit mutated source");
  auto flags_a = scene.OpaqueComponents(keys[0])->front();
  auto flags_b = scene.OpaqueComponents(keys[1])->front();
  flags_a.data[32] = 1;
  flags_b.data[32] = 2;
  Require(scene.SetOpaqueComponent(keys[0], flags_a) && scene.SetOpaqueComponent(keys[1], flags_b),
          "Mixed flag fixture failed");
  const auto flags = inspector.Inspect(scene, keys, schema.type);
  Require(flags && flags->properties[4].mixed &&
              inspector.ApplyFlag(scene, *flags, 4, 2, true, true) &&
              scene.OpaqueComponents(keys[0])->front().data[32] == 3 &&
              scene.OpaqueComponents(keys[1])->front().data[32] == 2 && scene.Undo() &&
              scene.OpaqueComponents(keys[0])->front() == flags_a &&
              scene.OpaqueComponents(keys[1])->front() == flags_b && scene.Redo() && scene.Undo(),
          "Mixed flags copied other targets' bits or lost atomic replay");
  Require(inspector.ApplyFlag(scene, *flags, 4, 1, false, true) &&
              scene.OpaqueComponents(keys[0])->front().data[32] == 0 &&
              scene.OpaqueComponents(keys[1])->front() == flags_b && scene.Undo() &&
              !inspector.ApplyFlag(scene, *flags, 4, 0, true, true) &&
              !inspector.ApplyFlag(scene, *flags, 4, 4, true, true) &&
              !inspector.ApplyFlag(scene, *flags, 4, 3, true, true) &&
              !inspector.ApplyFlag(scene, *flags, 0, 1, true, true) &&
              !inspector.ApplyFlag(scene, *flags, 4, 1, true, false) && scene.Undo() &&
              scene.Undo() && scene.OpaqueComponents(keys[0]) == original,
          "Flag clear or invalid bit changed unrelated source/history");
  observation = inspector.Inspect(scene, keys, schema.type);
  auto stale = *observation;
  stale.sources[0].second.data[0] ^= 1;
  Require(scene.OpaqueComponents(keys[0]) == original &&
              !inspector.Apply(scene, stale, 0, true, true),
          "owning snapshot mutation reached live bytes or granted write authority");
  stale = *observation;
  stale.properties[0].offset = 8;
  Require(!inspector.Apply(scene, stale, 0, true, true), "forged property address accepted");
  auto changed = second;
  changed.data[159] = 99;
  Require(scene.SetOpaqueComponent(keys[1], changed) &&
              !inspector.Apply(scene, *observation, 0, true, true) &&
              scene.OpaqueComponents(keys[0]) == original,
          "stale source allowed partial publication");
  Require(scene.Undo(), "restore failed");
  Require(scene.Select(std::array{a}) && !inspector.Apply(scene, *observation, 0, true, true) &&
              scene.Select(keys),
          "selection mismatch accepted");
  Require(inspector.SetComponents({schema}) && !inspector.Apply(scene, *observation, 0, true, true),
          "replaced metadata revived old observation");
  std::array<editor::SceneDocument::OpaqueComponentEdit, 2> edits{
      editor::SceneDocument::OpaqueComponentEdit{keys[0], payload, second},
      editor::SceneDocument::OpaqueComponentEdit{keys[1], changed, payload}};
  Require(!scene.ApplyOpaqueComponents(edits) && scene.OpaqueComponents(keys[0]) == original,
          "late invalid target allowed partial batch");
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-reflected-inspector-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  std::filesystem::create_directories(root / ".nexora");
  Require(scene.Save(root / "test.scene"), "save failed");
  const auto persisted = scene.OpaqueComponents(keys[0]);
  Require(scene.Reload(root / "test.scene") && scene.OpaqueComponents(*scene.Key(a)) == persisted &&
              !scene.ApplyOpaqueComponents(std::span(edits).first(1)),
          "reload lost bytes or revived key");
  Require(editor::ReflectedInspector::LoadProject(root)->Components().empty(),
          "missing metadata failed");
  const auto schema_path = root / ".nexora/inspector.reflection";
  const std::string valid =
      "NXEDITORREFLECTION 1\ntype 91 \"Plugin.Properties\" 160 2\n"
      "field \"enabled\" bool 0 1 0\nfield \"nested.samples\" double 128 2 0\n";
  std::ofstream(schema_path, std::ios::binary) << valid;
  const auto loaded = editor::ReflectedInspector::LoadProject(root);
  Require(loaded && loaded->Components().size() == 1 && loaded->Components()[0].fields.size() == 2,
          "real bounded schema load failed");
  std::error_code alias_error;
  std::filesystem::create_hard_link(schema_path, root / "schema-alias", alias_error);
  Require(!alias_error && !editor::ReflectedInspector::LoadProject(root),
          "aliased reflection metadata accepted");
  std::filesystem::remove(root / "schema-alias");
  for (const auto &invalid :
       {std::string("NXEDITORREFLECTION 2\n"), valid + "junk\n",
        std::string("NXEDITORREFLECTION 1\ntype -1 \"Bad\" 160 1\nfield \"x\" bool 0 1 0\n"),
        std::string(editor::ReflectedInspector::kMaximumSchemaBytes + 1, 'x')}) {
    std::ofstream(schema_path, std::ios::binary) << invalid;
    Require(!editor::ReflectedInspector::LoadProject(root) &&
                scene.OpaqueComponents(*scene.Key(a)) == persisted,
            "corrupt metadata changed component bytes");
  }
}
} // namespace
void WriteNativeFixture(const std::filesystem::path &root) {
  editor::ProjectWorkspace workspace;
  std::string error;
  Require(workspace.Create(root, "Native Reflection", &error), "native workspace fixture failed");
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Native Reflection"));
  const auto entity = scene.Create("Reflected entity");
  editor::OpaqueComponent payload{91, "Plugin.Properties", std::vector<std::uint8_t>(24)};
  payload.data[23] = 255;
  Require(scene.SetOpaqueComponent(*scene.Key(entity), payload), "native payload fixture failed");
  const auto directory = root / ".nexora/scenes";
  std::filesystem::create_directories(directory);
  Require(scene.Save(directory / "Main.scene"), "native scene fixture save failed");
  std::ofstream(root / ".nexora/inspector.reflection", std::ios::binary)
      << "NXEDITORREFLECTION 1\ntype 91 \"Plugin.Properties\" 24 2\n"
         "field \"enabled\" bool 0 1 0\nfield \"nested.amount\" double 8 1 0\n";
}
int main(int argc, char **argv) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--write-native-fixture") {
      WriteNativeFixture(
          std::filesystem::path{std::u8string(argv[2], argv[2] + std::strlen(argv[2]))});
      return 0;
    }
    Run();
    std::cout << "Reflected metadata/atomic editing/undo/persistence contracts passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
