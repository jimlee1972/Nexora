#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/SceneAuthoring.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace {
using namespace nexora;

void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

void Run() {
  runtime::ReflectionRegistry reflection;
  const auto type = runtime::HashTypeName("Transform");
  const auto scalar = runtime::HashTypeName("double");
  Require(reflection.Register({"Transform", type, {{"x", scalar, 0, sizeof(double)}}}),
          "reflection fixture failed");
  editor::InspectorPropertyAdapter adapter(reflection);
  runtime::World world;
  const auto scene_id = world.LoadScene("Inspector batch");
  editor::SceneDocument scene(world, scene_id);
  const auto first = scene.Create("First");
  const auto second = scene.Create("Second");
  const std::array ids{first, second};
  const std::array keys{*scene.Key(first), *scene.Key(second)};
  const std::array initial{runtime::Transform{1, 2, 3}, runtime::Transform{4, 5, 6}};
  Require(scene.SetTransforms(keys, initial) && scene.Select(keys) &&
              scene.SetOpaqueComponent(keys[0], {71, "Missing component", {0, 127, 255}}),
          "scene fixture failed");
  const auto original = world.SaveScene(scene_id);
  const auto opaque = scene.OpaqueComponents(keys[0]);
  const auto properties = adapter.Inspect(
      ids, std::array{type}, [&](runtime::Id entity, runtime::TypeId, std::string_view) {
        const auto pose = scene.Transform(entity);
        return pose ? std::optional<editor::InspectorValue>{pose->x} : std::nullopt;
      });
  Require(properties.size() == 1 && properties[0].mixed && !properties[0].value,
          "mixed reflection snapshot failed");
  const auto &property = properties[0];

  // The old per-target callback could commit First before rejecting Second. It must never be
  // called for multi-selection now, so neither Runtime state nor Undo history is changed.
  std::size_t legacy_calls = 0;
  Require(!adapter.Apply(ids, property, 9.0,
                         [&](runtime::Id entity, runtime::TypeId, std::string_view,
                             const editor::InspectorValue &value) {
                           ++legacy_calls;
                           if (entity == second)
                             return false;
                           auto pose = *scene.Transform(entity);
                           pose.x = std::get<double>(value);
                           return scene.SetTransform(entity, pose);
                         }) &&
              legacy_calls == 0 && world.SaveScene(scene_id) == original,
          "legacy multi-selection admitted partial mutation");

  std::size_t transaction_calls = 0;
  std::optional<editor::InspectorEditBatch> retained;
  const auto transaction = [&](const editor::InspectorEditBatch &request) {
    ++transaction_calls;
    retained = request;
    const auto *value = std::get_if<double>(&request.value);
    if (!value || request.property.component_type != type || request.property.property_name != "x")
      return false;
    std::vector<editor::SceneDocument::NodeKey> targets;
    std::vector<runtime::Transform> poses;
    for (const auto entity : request.entities) {
      // The authoring owner, not the reflection adapter, validates its live generations.
      const auto key = scene.Key(entity);
      const auto expected = std::ranges::find(keys, entity, &editor::SceneDocument::NodeKey::id);
      const auto pose = scene.Transform(entity);
      if (!key || expected == keys.end() || *key != *expected || !pose)
        return false;
      targets.push_back(*key);
      poses.push_back(*pose);
      poses.back().x = *value;
    }
    return scene.SetTransforms(targets, poses);
  };
  Require(adapter.ApplyBatch(ids, property, 9.0, transaction) && transaction_calls == 1 &&
              scene.Transform(first)->x == 9 && scene.Transform(second)->x == 9 &&
              scene.OpaqueComponents(keys[0]) == opaque,
          "one reflected request did not commit the complete selection");
  const auto edited = world.SaveScene(scene_id);
  Require(scene.Undo() && world.SaveScene(scene_id) == original && scene.Redo() &&
              world.SaveScene(scene_id) == edited && scene.OpaqueComponents(keys[0]) == opaque,
          "reflected selection was not one complete Undo/Redo unit");

  // A missing later target must reject the staged request while retaining the first target and
  // the Redo branch. Replaying Redo afterward verifies that the rejection added no transaction.
  Require(scene.Undo() && world.SaveScene(scene_id) == original, "Redo fixture failed");
  const std::array missing{first, std::numeric_limits<runtime::Id>::max()};
  Require(!adapter.ApplyBatch(missing, property, 12.0, transaction) && transaction_calls == 2 &&
              world.SaveScene(scene_id) == original && scene.Redo() &&
              world.SaveScene(scene_id) == edited,
          "rejected batch mutated an earlier target or discarded Redo");
  Require(!adapter.ApplyBatch(ids, property, std::int64_t{3}, transaction) &&
              world.SaveScene(scene_id) == edited,
          "the owner could not reject an incompatible property value atomically");

  std::size_t writes = 0;
  const editor::InspectorPropertyAdapter::WriteBatch capture =
      [&](const editor::InspectorEditBatch &request) {
        ++writes;
        retained = request;
        return true;
      };
  const auto rejected = [&](std::span<const runtime::Id> targets,
                            const editor::InspectorProperty &descriptor,
                            const editor::InspectorValue &value = 1.0) {
    const auto before = writes;
    Require(!adapter.ApplyBatch(targets, descriptor, value, capture) && writes == before,
            "invalid reflected request reached the transaction writer");
  };
  rejected({}, property);
  rejected(std::array{first, first}, property);
  rejected(std::array{first, runtime::Id{0}}, property);
  rejected(ids, property, std::numeric_limits<double>::infinity());
  rejected(ids, property, std::numeric_limits<double>::quiet_NaN());
  auto stale = property;
  stale.read_only = true;
  rejected(ids, stale);
  stale = property;
  stale.component_type = type + 1;
  rejected(ids, stale);
  stale = property;
  stale.component_name = "Another type";
  rejected(ids, stale);
  stale = property;
  stale.property_name = "missing";
  rejected(ids, stale);
  stale = property;
  ++stale.value_type;
  rejected(ids, stale);
  Require(!adapter.ApplyBatch(ids, property, 1.0, {}) && !adapter.Apply({}, property, 1.0, {}),
          "missing writers were accepted");

  const auto ambiguous = runtime::HashTypeName("Ambiguous");
  Require(reflection.Register({"Ambiguous", ambiguous, {{"x", scalar, 0, 8}, {"x", scalar, 8, 8}}}),
          "ambiguous metadata fixture failed");
  stale = property;
  stale.component_type = ambiguous;
  stale.component_name = "Ambiguous";
  rejected(ids, stale);

  std::vector<runtime::Id> large(editor::InspectorPropertyAdapter::kMaximumBatchEntities + 1);
  std::iota(large.begin(), large.end(), runtime::Id{1});
  rejected(large, property);
  large.pop_back();
  Require(adapter.ApplyBatch(large, property, 4.0, capture) && writes == 1 &&
              retained->entities == large,
          "maximum-sized selection could not produce one owning request");
  large.clear();
  Require(retained->entities.size() == editor::InspectorPropertyAdapter::kMaximumBatchEntities &&
              retained->property.property_name == "x" &&
              retained->value == editor::InspectorValue{4.0},
          "retained batch borrowed the caller's selection or property storage");
  Require(adapter.Apply(std::array{first}, property, 2.0,
                        [&](runtime::Id entity, runtime::TypeId component, std::string_view name,
                            const editor::InspectorValue &value) {
                          ++legacy_calls;
                          return entity == first && component == type && name == "x" &&
                                 value == editor::InspectorValue{2.0};
                        }) &&
              legacy_calls == 1,
          "single-target legacy compatibility failed");
}
} // namespace

int main() {
  try {
    Run();
    std::cout << "Inspector atomic batch contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
