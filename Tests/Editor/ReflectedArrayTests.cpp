#include "../../Engine/Editor/src/ReflectedArrayTestAccess.h"
#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/ReflectedInspector.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using Kind = editor::ReflectedKind;
using Action = editor::ReflectedArrayAction;
using Edit = editor::ReflectedArrayEdit;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Put(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint64_t value) {
  for (std::size_t i = 0; i < 8; ++i)
    bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (8 * i));
}
std::uint64_t Get(std::span<const std::uint8_t> bytes, std::size_t offset) {
  std::uint64_t value{};
  for (std::size_t i = 0; i < 8; ++i)
    value |= std::uint64_t{bytes[offset + i]} << (8 * i);
  return value;
}
std::size_t Property(const editor::ReflectedObservation &view, std::string_view path) {
  const auto found = std::ranges::find(view.properties, path, &editor::ReflectedProperty::path);
  Require(found != view.properties.end(), "Expected reflected property missing");
  return static_cast<std::size_t>(found - view.properties.begin());
}
struct Fixture final {
  editor::ReflectedComponent schema{
      301,
      "Plugin.陣列",
      40,
      {{"enabled", Kind::Boolean, 0}, {"amount", Kind::Number, 8}},
      {{"samples", Kind::Number, 16, 4}, {"ids", Kind::Unsigned, 24, 3}}};
  runtime::World world;
  editor::SceneDocument scene{world, world.LoadScene("Dynamic reflected arrays")};
  std::array<editor::SceneDocument::NodeKey, 2> keys;
  editor::ReflectedInspector inspector;
  Fixture() {
    Require(inspector.SetComponents({schema}), "Valid dynamic catalog rejected");
    keys = {*scene.Key(scene.Create("A")), *scene.Key(scene.Create("B"))};
    for (std::size_t target = 0; target < 2; ++target) {
      const auto count = 2 - target;
      editor::OpaqueComponent source{schema.type, schema.name,
                                     std::vector<std::uint8_t>(40 + count * 8 + 16)};
      source.data[7] = 0xff;
      source.data[39] = 0x7f;
      Put(source.data, 8, std::bit_cast<std::uint64_t>(3.5));
      Put(source.data, 16, count);
      Put(source.data, 24, 2);
      for (std::size_t i = 0; i < count; ++i)
        Put(source.data, 40 + i * 8, std::bit_cast<std::uint64_t>(double(i + 1)));
      Put(source.data, 40 + count * 8, 7);
      Put(source.data, 48 + count * 8, 9);
      Require(
          scene.SetOpaqueComponent(keys[target], source) &&
              scene.SetOpaqueComponent(keys[target], {999, "Missing.Provider", {0, 255, 127, 42}}),
          "Fixture source failed");
    }
    Require(scene.Select(keys), "Fixture selection failed");
  }
  editor::ReflectedObservation Observe() {
    const auto result = inspector.Inspect(scene, keys, schema.type);
    Require(result.has_value(), "Dynamic observation unavailable");
    return *result;
  }
  editor::OpaqueComponent Bytes(std::size_t target) {
    const auto sources = scene.OpaqueComponents(keys[target]);
    return *std::ranges::find(*sources, schema.type, &editor::OpaqueComponent::type);
  }
  void Conserve() {
    for (std::size_t t = 0; t < 2; ++t) {
      const auto data = Bytes(t).data;
      Require(data[7] == 255 && data[39] == 127, "Unknown prefix bytes changed");
      const auto components = scene.OpaqueComponents(keys[t]);
      const auto missing = std::ranges::find(*components, 999, &editor::OpaqueComponent::type);
      Require(missing != components->end() && missing->type_name == "Missing.Provider" &&
                  missing->data == std::vector<std::uint8_t>({0, 255, 127, 42}),
              "Unavailable component bytes changed");
    }
  }
};
void Transactions() {
  Fixture f;
  auto view = f.Observe();
  Require(view.arrays.size() == 2 && view.arrays[0].counts == std::vector<std::size_t>({2, 1}) &&
              view.arrays[0].mixed && view.arrays[0].common_elements == 1 &&
              view.properties.size() == 5,
          "Mixed lengths/common properties incorrect");
  const auto original = std::array{f.Bytes(0), f.Bytes(1)};
  const auto ids = Property(view, "ids[1]");
  Require(f.inspector.Apply(f.scene, view, ids, std::uint64_t{99}, true),
          "Variable-offset edit rejected");
  auto updated = f.Observe();
  Require(std::get<std::uint64_t>(updated.properties[Property(updated, "ids[1]")].value) == 99 &&
              !updated.properties[Property(updated, "ids[1]")].mixed &&
              Get(f.Bytes(0).data, 64) == 99 && Get(f.Bytes(1).data, 56) == 99 && f.scene.Undo() &&
              f.Bytes(0) == original[0] && f.Bytes(1) == original[1],
          "Property edit addressed first target offsets or lost one-step Undo");
  const std::array edits{Edit{Action::Append, 0, 0, 0, editor::ReflectedValue{8.5}},
                         Edit{Action::Insert, 0, 0, 0, editor::ReflectedValue{-2.5}},
                         Edit{Action::Remove, 0, 0},
                         Edit{Action::Move, 1, 0, 1},
                         Edit{Action::Move, 1, 1, 0},
                         Edit{Action::Clear, 0}};
  for (const auto &edit : edits) {
    view = f.Observe();
    Require(f.inspector.ApplyArray(f.scene, view, edit, true), "Dynamic transaction rejected");
    f.Conserve();
    updated = f.Observe();
    if (edit.action == Action::Append)
      Require(updated.arrays[0].counts == std::vector<std::size_t>({3, 2}) &&
                  Get(f.Bytes(0).data, 56) == std::bit_cast<std::uint64_t>(8.5) &&
                  Get(f.Bytes(1).data, 48) == std::bit_cast<std::uint64_t>(8.5),
              "Append lost each target's own length");
    if (edit.action == Action::Insert)
      Require(Get(f.Bytes(0).data, 40) == std::bit_cast<std::uint64_t>(-2.5) &&
                  Get(f.Bytes(1).data, 40) == std::bit_cast<std::uint64_t>(-2.5),
              "Insert value/order incorrect");
    if (edit.action == Action::Remove)
      Require(updated.arrays[0].counts == std::vector<std::size_t>({1, 0}) &&
                  Get(f.Bytes(0).data, 40) == std::bit_cast<std::uint64_t>(2.0),
              "Remove count/order incorrect");
    if (edit.action == Action::Move)
      Require(Get(f.Bytes(0).data, 56) == 9 && Get(f.Bytes(0).data, 64) == 7 &&
                  Get(f.Bytes(1).data, 48) == 9 && Get(f.Bytes(1).data, 56) == 7,
              "Move groups mixed bytes/tail offsets");
    if (edit.action == Action::Clear)
      Require(updated.arrays[0].counts == std::vector<std::size_t>({0, 0}) &&
                  !updated.arrays[0].mixed && updated.arrays[0].common_elements == 0 &&
                  f.Bytes(0).data.size() == 56 && f.Bytes(1).data.size() == 56,
              "Clear lost following tail/empty observation");
    Require(updated.arrays[1].counts == std::vector<std::size_t>({2, 2}),
            "Other array count changed");
    const auto after = std::array{f.Bytes(0), f.Bytes(1)};
    Require(f.scene.Undo() && f.Bytes(0) == original[0] && f.Bytes(1) == original[1] &&
                f.scene.Redo() && f.Bytes(0) == after[0] && f.Bytes(1) == after[1] &&
                f.scene.Undo(),
            "Array mutation did not publish exactly one owning Undo/Redo transaction");
  }
  view = f.Observe();
  const Edit append{Action::Append, 0, 0, 0, editor::ReflectedValue{6.25}};
  Require(!f.inspector.ApplyArray(f.scene, view, append, false), "Read-only authority ignored");
  for (const auto &invalid : std::array{
           Edit{Action::Append, 0}, Edit{Action::Append, 0, 0, 0, editor::ReflectedValue{true}},
           Edit{Action::Insert, 0, 2, 0, editor::ReflectedValue{1.0}}, Edit{Action::Remove, 0, 1},
           Edit{Action::Move, 0, 0, 1}, Edit{Action::Clear, 0, 1}, Edit{Action::Clear, 2},
           Edit{static_cast<Action>(255), 0}, Edit{Action::Move, 1, 0, 2}})
    Require(!f.inspector.ApplyArray(f.scene, view, invalid, true) && f.Bytes(0) == original[0] &&
                f.Bytes(1) == original[1],
            "Invalid late target partially changed source");
  auto forged = view;
  forged.arrays[0].counts[0] = 0;
  Require(!f.inspector.ApplyArray(f.scene, forged, append, true), "Forged observation accepted");
  forged = view;
  forged.properties[ids].array = 0;
  Require(!f.inspector.Apply(f.scene, forged, ids, std::uint64_t{2}, true),
          "Forged element identity accepted");
  Require(f.inspector.ApplyArray(f.scene, view, append, true) &&
              !f.inspector.ApplyArray(f.scene, view, append, true) && f.scene.Undo(),
          "Stale array source accepted");
  Require(f.scene.Select(std::array{f.keys[0]}) &&
              !f.inspector.ApplyArray(f.scene, view, append, true) && f.scene.Select(f.keys),
          "Changed selection accepted");
  for (std::size_t fault = 0; fault < 3; ++fault) {
    view = f.Observe();
    Require(f.inspector.Apply(f.scene, view, Property(view, "amount"), 5.0, true) && f.scene.Undo(),
            "Pending Redo fixture failed");
    if (fault < 2)
      editor::ReflectedArrayTestAccess::FailAfterReplacements(fault);
    else
      editor::ReflectedArrayTestAccess::FailHistoryAdmission();
    Require(!f.inspector.ApplyArray(f.scene, view, append, true) && f.Bytes(0) == original[0] &&
                f.Bytes(1) == original[1] && f.scene.Redo() && f.scene.Undo(),
            "Allocation/history admission failure changed source or erased pending Redo");
  }
  view = f.Observe();
  Require(f.inspector.ApplyArray(f.scene, view, {Action::Move, 1, 0, 0}, true) && f.scene.Redo() &&
              f.scene.Undo(),
          "No-op Move erased pending Redo");
  Require(f.inspector.SetComponents({f.schema}) &&
              !f.inspector.ApplyArray(f.scene, view, append, true),
          "Replaced catalog accepted old observation");
  view = f.Observe();
  Require(f.inspector.ApplyArray(f.scene, view, append, true), "Recovery after failures rejected");
  const auto saved = std::array{f.Bytes(0), f.Bytes(1)};
  const auto saved_view = f.Observe();
  const auto root = std::filesystem::temp_directory_path() /
                    std::filesystem::path(u8"nexora-陣列-µ") /
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  std::filesystem::create_directories(root);
  Require(f.scene.Save(root / "array.scene"), "Array scene Save failed");
  runtime::World reopened_world;
  editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Fresh"));
  Require(reopened.Reload(root / "array.scene"), "Fresh World reopen failed");
  for (std::size_t t = 0; t < 2; ++t) {
    const auto components = reopened.OpaqueComponents(*reopened.Key(f.keys[t].id));
    Require(*std::ranges::find(*components, f.schema.type, &editor::OpaqueComponent::type) ==
                saved[t],
            "Fresh reopen lost variable lengths/payload/padding");
  }
  Require(f.scene.Reload(root / "array.scene"), "Original owner reload failed");
  const std::array fresh_keys{*f.scene.Key(f.keys[0].id), *f.scene.Key(f.keys[1].id)};
  Require(f.scene.Select(fresh_keys) && fresh_keys[0] != saved_view.sources[0].first &&
              f.scene.OpaqueComponents(fresh_keys[0])->front() == saved[0] &&
              !f.inspector.ApplyArray(f.scene, saved_view, append, true),
          "Identical-byte reload revived stale entity/document generation");
}
void CorruptionAndLimits() {
  Fixture f;
  const auto good = f.Observe();
  const auto source = f.Bytes(0);
  for (std::size_t corruption = 0; corruption < 5; ++corruption) {
    auto bad = source;
    if (corruption == 0)
      Put(bad.data, 16, std::numeric_limits<std::uint64_t>::max());
    if (corruption == 1)
      bad.data.pop_back();
    if (corruption == 2)
      bad.data.push_back(255);
    if (corruption == 3)
      Put(bad.data, 48, std::bit_cast<std::uint64_t>(std::numeric_limits<double>::infinity()));
    if (corruption == 4)
      bad.data.resize(16);
    Require(f.scene.SetOpaqueComponent(f.keys[0], bad) &&
                !f.inspector.Inspect(f.scene, f.keys, f.schema.type) &&
                !f.inspector.ApplyArray(f.scene, good, {Action::Clear, 0}, true) &&
                f.Bytes(0) == bad && f.scene.Undo(),
            "Corrupt/outside-common element was interpreted, truncated or edited");
  }
  for (std::size_t defect = 0; defect < 7; ++defect) {
    auto bad = f.schema;
    if (defect == 0)
      bad.arrays[0].count_offset = 8;
    if (defect == 1)
      bad.arrays[1].count_offset = 16;
    if (defect == 2)
      bad.arrays[0].maximum_elements = 0;
    if (defect == 3)
      bad.arrays[0].maximum_elements = 33;
    if (defect == 4)
      bad.arrays[0].path = "enabled";
    if (defect == 5)
      bad.fields.push_back({"samples[3]", Kind::Boolean, 1});
    if (defect == 6)
      bad.arrays[0].kind = static_cast<Kind>(255);
    const auto revision = f.inspector.Revision();
    Require(!f.inspector.SetComponents({bad}) && f.inspector.Revision() == revision &&
                f.Bytes(0) == source,
            "Invalid schema changed catalog/source");
  }
  auto view = f.Observe();
  for (int i = 0; i < 2; ++i) {
    Require(f.inspector.ApplyArray(f.scene, view,
                                   {Action::Append, 0, 0, 0, editor::ReflectedValue{1.0}}, true),
            "Exact array bound rejected");
    view = f.Observe();
  }
  const auto full = f.Bytes(0);
  Require(!f.inspector.ApplyArray(f.scene, view,
                                  {Action::Append, 0, 0, 0, editor::ReflectedValue{1.0}}, true) &&
              f.Bytes(0) == full,
          "Array over-capacity partially published");
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Observation budget"));
  editor::ReflectedInspector inspector;
  editor::ReflectedComponent schema{902, "Budget", 4096, {}, {{"values", Kind::Unsigned, 0, 1}}};
  Require(inspector.SetComponents({schema}), "Array-only schema rejected");
  std::vector<editor::SceneDocument::NodeKey> keys;
  for (std::size_t i = 0; i < editor::ReflectedInspector::kMaximumTargets; ++i) {
    const auto key = *scene.Key(scene.Create("Budget target"));
    keys.push_back(key);
    Require(
        scene.SetOpaqueComponent(key, {schema.type, schema.name, std::vector<std::uint8_t>(4096)}),
        "Observation budget fixture failed");
  }
  Require(scene.Select(keys), "Budget selection failed");
  const auto at_bound = inspector.Inspect(scene, keys, schema.type);
  Require(at_bound.has_value() &&
              !inspector.ApplyArray(
                  scene, *at_bound,
                  {Action::Append, 0, 0, 0, editor::ReflectedValue{std::uint64_t{1}}}, true),
          "Exact source bound or candidate-over-budget admission incorrect");
  for (const auto key : keys)
    Require(scene.OpaqueComponents(key)->front().data.size() == 4096,
            "Late output budget rejection partially resized targets");
}
void KindsAndBounds() {
  const std::array kinds{Kind::Boolean, Kind::Integer, Kind::Unsigned,        Kind::Number,
                         Kind::Enum,    Kind::Flags,   Kind::Vector2,         Kind::Vector3,
                         Kind::Vector4, Kind::Color,   Kind::EntityReference, Kind::AssetReference};
  for (const auto kind : kinds) {
    runtime::World world;
    editor::SceneDocument scene(world, world.LoadScene("Typed array"));
    const auto key = *scene.Key(scene.Create("Typed"));
    editor::ReflectedComponent schema{
        1000 + static_cast<unsigned>(kind), "Typed.Array", 16, {}, {{"values", kind, 0, 32}}};
    editor::ReflectedValue first, second;
    switch (kind) {
    case Kind::Boolean:
      first = false;
      second = true;
      break;
    case Kind::Integer:
      first = std::numeric_limits<std::int64_t>::min();
      second = std::int64_t{42};
      break;
    case Kind::Number:
      first = -1.25;
      second = 9.5;
      break;
    case Kind::Enum:
      schema.arrays[0].choices = {{7, "Seven"}, {9, "Nine"}};
      first = std::uint64_t{7};
      second = std::uint64_t{9};
      break;
    case Kind::Flags:
      schema.arrays[0].choices = {{1, "Read"}, {8, "Write"}};
      first = std::uint64_t{1};
      second = std::uint64_t{8};
      break;
    case Kind::Vector2:
    case Kind::Vector3:
    case Kind::Vector4:
    case Kind::Color:
      first = std::array<double, 4>{.1, .2, .3, .4};
      second = std::array<double, 4>{.4, .3, .2, .1};
      if (kind == Kind::Vector2) {
        std::get<std::array<double, 4>>(first)[2] = 0;
        std::get<std::array<double, 4>>(second)[2] = 0;
      }
      if (kind == Kind::Vector2 || kind == Kind::Vector3) {
        std::get<std::array<double, 4>>(first)[3] = 0;
        std::get<std::array<double, 4>>(second)[3] = 0;
      }
      break;
    case Kind::AssetReference:
      first = foundation::Uuid{std::numeric_limits<std::uint64_t>::max(), 1};
      second = foundation::Uuid{2, 3};
      break;
    default:
      first = std::numeric_limits<std::uint64_t>::max();
      second = std::uint64_t{42};
      break;
    }
    editor::ReflectedInspector inspector;
    Require(inspector.SetComponents({schema}), "Typed array schema rejected");
    editor::OpaqueComponent payload{schema.type, schema.name, std::vector<std::uint8_t>(16)};
    payload.data[15] = 255;
    Require(scene.SetOpaqueComponent(key, payload) && scene.Select(std::array{key}),
            "Typed fixture rejected");
    const auto observe = [&] {
      auto v = inspector.Inspect(scene, std::array{key}, schema.type);
      Require(v.has_value(), "Typed observation missing");
      return *v;
    };
    Require(inspector.ApplyArray(scene, observe(), {Action::Append, 0, 0, 0, first}, true) &&
                inspector.ApplyArray(scene, observe(), {Action::Append, 0, 0, 0, second}, true),
            "Typed append failed");
    auto view = observe();
    Require(view.properties[0].value == first && view.properties[1].value == second &&
                inspector.ApplyArray(scene, view, {Action::Move, 0, 0, 1}, true),
            "Typed values/reorder failed");
    view = observe();
    Require(view.properties[0].value == second && view.properties[1].value == first &&
                inspector.Apply(scene, view, 0, first, true) && scene.Undo(),
            "Typed reorder moved partial lanes or edit/Undo failed");
    if (kind == Kind::Flags) {
      view = observe();
      Require(inspector.ApplyFlag(scene, view, 1, 8, true, true) &&
                  std::get<std::uint64_t>(observe().properties[1].value) == 9 && scene.Undo(),
              "Dynamic flag edit lost unrelated bits");
    }
    if (kind == Kind::Enum || kind == Kind::Flags || kind == Kind::Color || kind == Kind::Number) {
      view = observe();
      const editor::ReflectedValue invalid =
          kind == Kind::Color    ? editor::ReflectedValue{std::array<double, 4>{2, 0, 0, 0}}
          : kind == Kind::Number ? editor::ReflectedValue{std::numeric_limits<double>::quiet_NaN()}
                                 : editor::ReflectedValue{std::uint64_t{2}};
      Require(!inspector.ApplyArray(scene, view, {Action::Append, 0, 0, 0, invalid}, true),
              "Invalid typed array value accepted");
    }
    Require(inspector.ApplyArray(scene, observe(), {Action::Clear, 0}, true), "Typed clear failed");
    for (std::size_t i = 0; i < 32; ++i)
      Require(inspector.ApplyArray(scene, observe(), {Action::Append, 0, 0, 0, first}, true),
              "Exact32-element admission rejected");
    view = observe();
    Require(view.properties.size() == 32 && view.arrays[0].counts[0] == 32 &&
                !inspector.ApplyArray(scene, view, {Action::Append, 0, 0, 0, first}, true) &&
                scene.OpaqueComponents(key)->front().data[15] == 255,
            "Over32 admission or typed prefix preservation failed");
  }
  Fixture f;
  Require(f.inspector.ApplyArray(f.scene, f.Observe(), {Action::Clear, 0}, true),
          "No-op Clear fixture failed");
  auto view = f.Observe();
  Require(f.inspector.Apply(f.scene, view, Property(view, "amount"), 5.0, true) && f.scene.Undo() &&
              f.inspector.ApplyArray(f.scene, view, {Action::Clear, 0}, true) && f.scene.Redo() &&
              f.scene.Undo(),
          "Empty Clear erased pending Redo");
}
void Formats() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-array-schema-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  std::filesystem::create_directories(root / ".nexora");
  const auto path = root / ".nexora/inspector.reflection";
  const auto load = [&](std::string_view text) {
    {
      std::ofstream out(path, std::ios::binary);
      out << text;
      Require(bool(out), "Metadata write failed");
    }
    return editor::ReflectedInspector::LoadProject(root);
  };
  const std::string version2 = "NXEDITORREFLECTION 2\ntype 301 \"Plugin.陣列\" 40 1 2\n"
                               "field \"enabled\" bool 0 1 0\narray \"samples\" double 16 4 0\n"
                               "array \"ids\" uint64 24 3 0\n";
  const auto parsed = load(version2);
  Require(parsed && parsed->Components().size() == 1 &&
              parsed->Components()[0].arrays.size() == 2 &&
              parsed->Components()[0].arrays[0].maximum_elements == 4,
          "Version2 count/tail metadata failed");
  Require(load("NXEDITORREFLECTION 1\ntype 1 \"Legacy\" 8 1\nfield \"number\" double 0 1 0\n")
              .has_value(),
          "Legacy fixed reflection compatibility regressed");
  for (const auto &bad : std::array{
           std::string("NXEDITORREFLECTION 3\n"), version2 + "junk\n",
           version2.substr(0, version2.size() - 8),
           std::string("NXEDITORREFLECTION 2\ntype 1 \"X\" 8 0 1\narray \"x\" double -1 2 0\n"),
           std::string("NXEDITORREFLECTION 2\ntype 1 \"X\" 8 0 1\narray \"x\" double 0 33 0\n"),
           std::string("NXEDITORREFLECTION 1\ntype 1 \"X\" 8 0 1\narray \"x\" double 0 2 0\n")})
    Require(!load(bad), "Malformed/truncated/unknown-version metadata accepted");
}
} // namespace
int main() {
  try {
    Transactions();
    CorruptionAndLimits();
    KindsAndBounds();
    Formats();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
