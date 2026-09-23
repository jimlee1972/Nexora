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

  const auto other_scene = world.LoadScene("OtherScene");
  Require(!editor.DestroyEntity(other_scene, entity_id) && editor.UndoDepth() == 2 &&
              world.FindEntity(entity_id) != nullptr,
          "scene editor accepted an entity from a different scene");

  Require(editor.DestroyEntity(scene, entity_id) && editor.UndoDepth() == 3,
          "scene editor did not destroy the entity");
  Require(world.FindEntity(entity_id) == nullptr, "destroyed entity is still findable");

  Require(editor.Undo() && editor.UndoDepth() == 2, "undoing the destroy failed");
  Require(world.FindScene(scene)->entities.size() == 1,
          "undoing a destroy did not restore an entity");
  Require(world.FindEntity(entity_id) != nullptr && world.FindEntity(entity_id)->transform.x == 7.0,
          "undoing a destroy did not restore the entity's identity and component data");

  Require(editor.Undo() && editor.UndoDepth() == 1,
          "undoing a transform after restoring an entity failed");
  Require(world.FindEntity(entity_id) != nullptr && world.FindEntity(entity_id)->transform.x == 0.0,
          "transform undo did not target the restored stable entity");
  Require(editor.Undo() && editor.UndoDepth() == 0, "undoing entity creation failed");
  Require(world.FindEntity(entity_id) == nullptr,
          "undoing entity creation did not remove the restored stable entity");
  Require(!editor.Undo(), "undo succeeded past the bottom of the stack");

  // ---- Play-in-Editor: isolated world / pause / step / focus / apply-back ----
  auto &pie_entity = editor.CreateEntity(scene);
  const auto pie_entity_id = pie_entity.id;
  Require(editor.SetTransform(pie_entity_id, {1.0, 0.0, 0.0}), "PIE source transform setup failed");
  PlaySession play(world);
  const auto simulate = [pie_entity_id](World &play_world, double fixed_delta) {
    const auto *current = play_world.FindEntity(pie_entity_id);
    if (current == nullptr)
      return false;
    WorldCommandBuffer commands;
    commands.SetTransform(pie_entity_id, {current->transform.x + fixed_delta, current->transform.y,
                                          current->transform.z});
    return commands.Apply(play_world);
  };
  Require(!play.Start(0.0, simulate) && play.Start(0.5, simulate) &&
              play.State() == PlayState::Playing && play.PlayWorld() != nullptr &&
              play.PlayWorld()->Kind() == WorldKind::Play && !play.AcceptsInput(),
          "PIE did not create an isolated Play World with safe input focus");
  play.SetInputFocus(true);
  Require(play.AcceptsInput(), "PIE Game View could not acquire input focus");
  Require(!play.Start(0.5, simulate) && play.Tick() &&
              play.PlayWorld()->FindEntity(pie_entity_id)->transform.x == 1.5 &&
              world.FindEntity(pie_entity_id)->transform.x == 1.0,
          "PIE tick leaked into the Editor World");
  Require(play.Pause() && !play.Tick() && play.Step() && play.Stats().fixed_ticks == 2 &&
              play.Stats().manual_steps == 1 &&
              play.PlayWorld()->FindEntity(pie_entity_id)->transform.x == 2.0,
          "PIE pause/step did not execute exactly one fixed tick");
  play.SetInputFocus(false);
  Require(!play.AcceptsInput() && play.Resume() && !play.Step() && play.Tick(),
          "PIE focus or resume policy failed");
  Require(play.Stop() && play.State() == PlayState::Stopped && play.PlayWorld() == nullptr &&
              world.FindEntity(pie_entity_id)->transform.x == 1.0 && !play.AcceptsInput(),
          "default PIE stop did not discard Play World changes");

  Require(play.Start(0.25, simulate) && play.Pause() && play.Step() &&
              play.Stop(ApplyBackPolicy::Transforms) && play.Stats().applied_transforms == 1 &&
              world.FindEntity(pie_entity_id)->transform.x == 1.25,
          "explicit PIE transform apply-back failed");
  Require(!play.Stop(ApplyBackPolicy::Transforms), "stopped PIE session accepted another stop");

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
  Require(instance.Overrides().size() == 1 && instance.Overrides().front().path == "Weapon" &&
              instance.RevertOverride("Weapon", "damage") && instance.OverrideCount() == 0 &&
              instance.Resolve("Weapon", "damage") == "10" &&
              !instance.RevertOverride("Weapon", "damage"),
          "prefab override diff or revert failed");
  Require(instance.SetOverride("Weapon", "damage", "30"),
          "prefab override could not be restored for apply/rebase coverage");

  PrefabNode new_child{"Weapon", {{"damage", "10"}, {"range", "5"}}, {}};
  PrefabNode new_root{"Hero", {{"health", "150"}}, {new_child}};
  auto updated = std::make_shared<Prefab>(std::move(new_root));
  Require(instance.Rebase(updated), "prefab rebase failed");
  Require(instance.Resolve("", "health") == "150" && instance.Resolve("Weapon", "damage") == "30" &&
              instance.Resolve("Weapon", "range") == "5",
          "rebase did not combine the new template with the surviving override");
  const auto applied_prefab = instance.ApplyOverrides();
  Require(applied_prefab && instance.OverrideCount() == 0 &&
              instance.Resolve("Weapon", "damage") == "30" &&
              applied_prefab->Find("Weapon") != nullptr,
          "prefab override apply did not create a clean source revision");

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
