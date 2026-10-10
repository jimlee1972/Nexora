#include "Nexora/Editor/ProjectPrefabPlacement.h"
#include "Nexora/Editor/SceneFiles.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
namespace {
using namespace nexora;
using Assets = editor::PrefabAssets;
using Owner = editor::ProjectPrefabPlacement;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::map<std::filesystem::path, std::string> Files(const std::filesystem::path &root) {
  std::map<std::filesystem::path, std::string> result;
  for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    if (entry.is_regular_file()) {
      std::ifstream file(entry.path(), std::ios::binary);
      result.emplace(entry.path().lexically_relative(root),
                     std::string{std::istreambuf_iterator<char>(file), {}});
    }
  return result;
}
void Write(const std::filesystem::path &path, const editor::PrefabAsset &asset) {
  const auto encoded = Assets::Encode(asset);
  Require(encoded.has_value(), "Replacement fixture was not a valid prefab asset");
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char *>(encoded->data()),
             static_cast<std::streamsize>(encoded->size()));
  Require(static_cast<bool>(file), "Replacement fixture write failed");
}
void Run() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-project-placement-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(path, error);
    }
  } cleanup{root};
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root / "Project", "Placement"), "Project fixture failed");
  runtime::World source_world, nested_world, target_world;
  editor::SceneDocument source(source_world, source_world.LoadScene("Source"));
  editor::SceneDocument nested(nested_world, nested_world.LoadScene("Nested"));
  editor::SceneDocument target(target_world, target_world.LoadScene("Target"));
  const auto attachment = source.Create("Attachment"), child = nested.Create("Old nested");
  Require(nested.SetOpaqueComponent(*nested.Key(child), {99, "Absent.Provider", {0, 255, 27}}),
          "Unknown nested fixture failed");
  std::uint64_t serial = 100;
  const auto factory = [&] { return foundation::Uuid{1900, ++serial}; };
  auto parent = Assets::Capture({1901, 1}, source, factory);
  const auto old = Assets::Capture({1901, 2}, nested, factory);
  Require(parent && old && Assets::Publish(workspace, *old), "First nested publication failed");
  Require(nested.Rename(*nested.Key(child), "New nested"), "Nested advance failed");
  const auto newer = Assets::Capture(old->id, nested, factory, &*old);
  Require(newer && Assets::Publish(workspace, *newer, &*old), "Nested archive publication failed");
  const auto attach_id =
      std::ranges::find(parent->nodes, attachment, &editor::PrefabNodeIdentity::serialized_node)
          ->id;
  parent->nested = {{{1902, 1}, attach_id, {old->id, 1}}, {{1902, 2}, attach_id, {old->id, 2}}};
  Require(Assets::Publish(workspace, *parent), "Parent publication failed");
  const auto seed = target.Create("Seed");
  const auto seed_key = *target.Key(seed);
  Require(target.Select(std::array{seed}) && target.CopySelection() &&
              target.Rename(seed_key, "Pending redo") && target.Undo(),
          "History fixture failed");
  const auto expected = *target.PrepareSave();
  auto review = Owner::Prepare(workspace, parent->id, target);
  Require(review && review->Source() == editor::PrefabRevisionReference{parent->id, 1} &&
              review->NodeCount() == 3,
          "Exact retained project closure preparation failed");
  const auto original_files = Files(workspace.Root());
  Require(!Owner::Prepare(workspace, {}, target) &&
              !Owner::Instantiate(workspace, target, *review, {1903, 1}, false) &&
              !Owner::Instantiate(workspace, target, *review, {}, true) &&
              target.MatchesPreparedSave(expected) && target.Redo() && target.Undo(),
          "Rejected placement changed content or consumed Redo");
  runtime::World foreign_world;
  editor::SceneDocument foreign(foreign_world, foreign_world.LoadScene("Foreign"));
  Require(!Owner::Instantiate(workspace, foreign, *review, {1903, 1}, true),
          "Foreign document consumed a prepared placement");
  editor::ProjectWorkspace observer;
  Require(observer.Open(workspace.Root(), editor::ProjectAccess::ReadOnly), "Observer open failed");
  const auto readonly = Owner::Prepare(observer, parent->id, target);
  Require(readonly && !Owner::Instantiate(observer, target, *readonly, {1903, 1}, true) &&
              target.MatchesPreparedSave(expected),
          "Read-only observation acquired write authority");
  const auto recovery = workspace.Root() / ".nexora/workspace.recovery";
  {
    std::ofstream file(recovery);
    file << "pending";
  }
  Require(!Owner::Prepare(workspace, parent->id, target) &&
              !Owner::Instantiate(workspace, target, *review, {1903, 1}, true),
          "Pending recovery allowed placement");
  std::filesystem::remove(recovery);
  const auto state = workspace.Root() / ".nexora/workspace";
  const auto timestamp = std::filesystem::last_write_time(state);
  std::filesystem::last_write_time(state, timestamp + std::chrono::seconds(10));
  Require(!Owner::Prepare(workspace, parent->id, target) &&
              !Owner::Instantiate(workspace, target, *review, {1903, 1}, true),
          "Externally changed project allowed placement");
  std::filesystem::last_write_time(state, timestamp);
  const auto archive =
      workspace.Root() / ".nexora/prefabs/revisions" / old->id.ToString() / "1.nxprefab";
  auto replaced = *old;
  runtime::World tamper_world;
  editor::SceneDocument tamper(tamper_world, tamper_world.LoadScene("Tamper"));
  Require(tamper.ReloadBytes(old->scene_bytes) &&
              tamper.Rename(*tamper.Key(child), "Valid replacement"),
          "Replacement fixture failed");
  replaced.scene_bytes = tamper.PrepareSave()->Bytes();
  Write(archive, replaced);
  Require(!Owner::Instantiate(workspace, target, *review, {1903, 1}, true) &&
              target.MatchesPreparedSave(expected),
          "Valid same-revision transitive replacement retained stale placement authority");
  Write(archive, *old);
  Require(target.Rename(seed_key, "Stale local") &&
              !Owner::Instantiate(workspace, target, *review, {1903, 1}, true) && target.Undo(),
          "Stale target accepted placement");
  const auto placed = Owner::Instantiate(workspace, target, *review, {1903, 1}, true);
  Require(placed && placed->size() == 3 && target.PrefabPlacements().size() == 1 &&
              target.PrefabPlacements().front().nodes.size() == 3 &&
              target.PrefabPlacements().front().revision == 1 && target.Key(seed) == seed_key &&
              target.Dirty() && Files(workspace.Root()) == original_files,
          "Placement lost scoped bindings, existing keys or wrote before Scene Save");
  const auto published = *target.PrepareSave();
  Require(target.Undo() && target.MatchesPreparedSave(expected) &&
              target.PrefabPlacements().empty() && target.Selection().front() == seed &&
              target.Redo() && target.MatchesPreparedSave(published) && target.Paste() &&
              target.Name(target.Selection().front()) == "Seed Copy" && target.Undo() &&
              target.MatchesPreparedSave(published),
          "Placement was not one complete Undo/Redo or erased clipboard");
  auto stale_source = Owner::Prepare(workspace, parent->id, target);
  auto next_parent = *parent;
  next_parent.revision = 2;
  Require(stale_source && Assets::Publish(workspace, next_parent, &*parent) &&
              !Owner::Instantiate(workspace, target, *stale_source, {1903, 2}, true) &&
              target.MatchesPreparedSave(published),
          "Advancing published root retained stale authority");
  auto fresh = Owner::Prepare(workspace, parent->id, target);
  Require(fresh && fresh->Source().revision == 2 &&
              Owner::Instantiate(workspace, target, *fresh, {1903, 2}, true) &&
              target.PrefabPlacements().size() == 2 &&
              target.PrefabPlacements().back().revision == 2,
          "Fresh repeated source placement failed");
  editor::SceneFileSession files(workspace, target);
  Require(files.SaveAs(files.Token(), "Content/Placed.scene").Applied() && !target.Dirty(),
          "Explicit Scene Save failed bound publication");
  runtime::World reopen_world;
  editor::SceneDocument reopen(reopen_world, reopen_world.LoadScene("Reopen"));
  Require(reopen.Reload(workspace.Root() / "Content/Placed.scene") && !reopen.Dirty() &&
              std::ranges::equal(reopen.PrefabPlacements(), target.PrefabPlacements()),
          "Saved project placement did not reopen exact scoped source revisions");
}
} // namespace
int main() {
  try {
    Run();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
