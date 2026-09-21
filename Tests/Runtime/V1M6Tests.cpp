#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Runtime/EditorSdk.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using namespace nexora::runtime;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

int Run() {
  Require(EditorSdkEnabled(), "editor SDK unexpectedly stripped");

  // ---- Reflection metadata ----
  ReflectionRegistry reflection;
  TypeDescriptor transform_type{"Transform",
                                0,
                                {{"x", HashTypeName("double"), 0, sizeof(double)},
                                 {"y", HashTypeName("double"), sizeof(double), sizeof(double)}}};
  Require(reflection.Register(transform_type), "type registration failed");
  Require(!reflection.Register(transform_type), "duplicate type name accepted");
  const auto *found = reflection.Find("Transform");
  Require(found != nullptr && found->fields.size() == 2 && found->id == HashTypeName("Transform"),
          "reflection lookup by name failed");
  Require(reflection.FindById(found->id) == found, "reflection lookup by id failed");
  Require(reflection.Find("Missing") == nullptr, "missing type incorrectly found");

  // ---- Service registry ----
  ServiceRegistry services;
  int dummy_service = 42;
  Require(services.Register("logging", &dummy_service), "service registration failed");
  Require(!services.Register("logging", &dummy_service), "duplicate service accepted");
  Require(services.Find("logging") == &dummy_service, "service lookup failed");
  Require(services.Unregister("logging") && services.Find("logging") == nullptr,
          "service unregister failed");

  // ---- Plugin host: real dynamic loading, stable ABI gate ----
  PluginHost matching_host(nexora::foundation::kEngineAbiVersion);
  const auto missing = matching_host.Load("Nexora_Nonexistent_Plugin_File.so");
  Require(!missing.loaded && missing.error == PluginLoadError::OpenFailed,
          "loading a nonexistent plugin file was not rejected");

  if (const auto *plugin_path = std::getenv("NEXORA_EXAMPLE_PLUGIN_PATH");
      plugin_path != nullptr && *plugin_path != '\0') {
    PluginHost mismatched_host(nexora::foundation::kEngineAbiVersion + 1);
    const auto mismatch = mismatched_host.Load(plugin_path, &services);
    Require(!mismatch.loaded && mismatch.error == PluginLoadError::AbiMismatch &&
                mismatch.reported_abi == nexora::foundation::kEngineAbiVersion &&
                !mismatch.registered && mismatched_host.LoadedCount() == 0 &&
                services.Find("example.marker") == nullptr,
            "ABI-mismatched plugin was loaded or registered instead of being rejected before use");

    const auto matched = matching_host.Load(plugin_path, &services);
    Require(matched.loaded && matched.error == PluginLoadError::None &&
                matched.reported_abi == nexora::foundation::kEngineAbiVersion &&
                matched.registered && matching_host.LoadedCount() == 1,
            "an ABI-matched plugin built from only public headers failed to load or register");
    const auto *marker = static_cast<const char *>(services.Find("example.marker"));
    Require(marker != nullptr && std::string_view(marker) == "Nexora example plugin",
            "plugin registration did not reach the host's service registry");
  } else {
    std::cout << "V1-M6: NEXORA_EXAMPLE_PLUGIN_PATH not set, skipping the real plugin load\n";
  }

  // ---- Scene editor: create / modify / undo over World ----
  World world;
  const auto scene = world.LoadScene("EditorScene");
  SceneEditor editor(world);
  auto &entity = editor.CreateEntity(scene);
  const auto entity_id = entity.id;
  Require(editor.UndoDepth() == 1 && world.FindEntity(entity_id) != nullptr,
          "scene editor did not create the entity");

  Require(editor.SetTransform(entity_id, {1.0, 2.0, 3.0}) && editor.UndoDepth() == 2,
          "scene editor did not apply the transform");
  Require(world.FindEntity(entity_id)->transform.x == 1.0, "transform was not applied");
  Require(!editor.SetTransform(999999, {}), "transform on a missing entity was accepted");

  Require(editor.Undo() && editor.UndoDepth() == 1, "undoing the transform failed");
  Require(world.FindEntity(entity_id)->transform.x == 0.0, "transform undo did not restore state");

  // Give the entity a distinctive, non-default marker before destroying it,
  // so restoring it back is verified against real data rather than a value
  // a freshly-created blank entity would also happen to have.
  Require(editor.SetTransform(entity_id, {7.0, 8.0, 9.0}) && editor.UndoDepth() == 2,
          "scene editor did not apply the pre-destroy marker transform");

  Require(editor.DestroyEntity(scene, entity_id) && editor.UndoDepth() == 3,
          "scene editor did not destroy the entity");
  Require(world.FindEntity(entity_id) == nullptr, "destroyed entity is still findable");

  Require(editor.Undo() && editor.UndoDepth() == 2, "undoing the destroy failed");
  Require(world.FindScene(scene)->entities.size() == 1,
          "undoing a destroy did not restore an entity");
  Require(world.FindScene(scene)->entities.front().transform.x == 7.0,
          "undoing a destroy did not restore the entity's component data");

  // World has no public API to recreate an entity under a caller-chosen ID
  // (documented in EditorSdk.h), so the restored entity above has a new,
  // different ID than the one SceneEditor::CreateEntity produced first. The
  // two remaining undo cards below it (the marker SetTransform and the
  // original CreateEntity) still target that original, now-gone ID:
  // WorldCommandBuffer::Apply rejects a command whose entity does not exist,
  // so popping them is a safe, observable no-op rather than a silent
  // corruption -- they pop the stack but leave the restored entity in place.
  Require(editor.Undo() && editor.UndoDepth() == 1, "the stale transform-undo card was not popped");
  Require(
      world.FindScene(scene)->entities.size() == 1,
      "a stale transform-undo below a destroy+restore unexpectedly changed the restored entity");
  Require(editor.Undo() && editor.UndoDepth() == 0, "the stale create-undo card was not popped");
  Require(world.FindScene(scene)->entities.size() == 1,
          "a stale create-undo below a destroy+restore unexpectedly removed the restored entity");
  Require(!editor.Undo(), "undo succeeded past the bottom of the stack");

  // ---- Prefab / nested prefab / override / rebase / variant ----
  PrefabNode child{"Weapon", {{"damage", "10"}}, {}};
  PrefabNode root{"Hero", {{"health", "100"}}, {child}};
  auto base = std::make_shared<Prefab>(std::move(root));
  Require(base->Find("Weapon") != nullptr && base->Find("Missing") == nullptr,
          "nested prefab lookup failed");

  PrefabInstance instance(base);
  Require(instance.Resolve("", "health") == "100", "prefab base property was not resolved");
  Require(!instance.SetOverride("Missing", "key", "value"), "override on a missing path accepted");
  Require(instance.SetOverride("Weapon", "damage", "25"), "prefab override was rejected");
  Require(instance.Resolve("Weapon", "damage") == "25", "prefab override was not resolved");
  Require(instance.OverrideCount() == 1, "override count is wrong");
  Require(instance.SetOverride("Weapon", "damage", "30") && instance.OverrideCount() == 1,
          "re-setting the same override key created a duplicate");

  PrefabNode new_child{"Weapon", {{"damage", "10"}, {"range", "5"}}, {}};
  PrefabNode new_root{"Hero", {{"health", "150"}}, {new_child}};
  auto updated = std::make_shared<Prefab>(std::move(new_root));
  Require(instance.Rebase(updated), "prefab rebase failed");
  Require(instance.Resolve("", "health") == "150" && instance.Resolve("Weapon", "damage") == "30" &&
              instance.Resolve("Weapon", "range") == "5",
          "rebase did not combine the new template with the surviving override");

  PrefabNode stale_child{"Renamed", {}, {}};
  PrefabNode stale_root{"Hero", {}, {stale_child}};
  Require(instance.Rebase(std::make_shared<Prefab>(std::move(stale_root))) &&
              instance.OverrideCount() == 0,
          "rebase did not drop an override whose path no longer exists");

  PrefabNode variant_child{"Weapon", {{"damage", "10"}}, {}};
  PrefabNode variant_root{"Hero", {{"health", "100"}}, {variant_child}};
  auto variant_base = std::make_shared<Prefab>(std::move(variant_root));
  PrefabVariant variant(variant_base, {{"Weapon", "damage", "999"}, {"", "health", "1"}});
  const auto baked = variant.Bake();
  Require(baked && baked->Find("")->properties.front().value == "1" &&
              baked->Find("Weapon")->properties.front().value == "999",
          "prefab variant did not bake its overrides");

  // ---- Project settings ----
  ProjectSettings settings;
  Require(!settings.GetString("missing").has_value(), "missing setting returned a value");
  Require(settings.GetStringOr("missing", "default") == "default", "fallback setting failed");
  settings.SetString("renderer.backend", "vulkan");
  Require(settings.GetString("renderer.backend") == "vulkan", "setting round trip failed");
  settings.SetString("renderer.backend", "metal");
  Require(settings.GetString("renderer.backend") == "metal" && settings.Size() == 1,
          "overwriting a setting created a duplicate");

  return 0;
}
} // namespace

int main() {
  try {
    return Run();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
