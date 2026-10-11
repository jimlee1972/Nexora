#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/PrefabAssets.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Assets = editor::PrefabAssets;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Write(const std::filesystem::path &path, const editor::PrefabAsset &asset) {
  const auto bytes = Assets::Encode(asset);
  Require(bytes.has_value(), "Fixture encode failed");
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char *>(bytes->data()),
               static_cast<std::streamsize>(bytes->size()));
  Require(static_cast<bool>(output), "Fixture write failed");
}
void Run() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-prefab-revisions-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace;
  std::string error;
  Require(workspace.Create(root / "Project", "Revisions", &error), "Project creation failed");
  runtime::World world;
  editor::SceneDocument document(world, world.LoadScene("Revision source"));
  const auto parent = document.Create("Revision one"), child = document.Create("Child", parent);
  const auto key = *document.Key(parent);
  Require(document.SetOpaqueComponent(*document.Key(child), {99, "Missing.Provider", {0, 255, 27}}),
          "Opaque fixture failed");
  std::uint64_t identity = 100;
  const auto factory = [&] { return foundation::Uuid{602, ++identity}; };
  const foundation::Uuid id{601, 1};
  const auto first = Assets::Capture(id, document, factory);
  Require(first && Assets::Publish(workspace, *first, nullptr, &error) &&
              Assets::LoadRevision(workspace, {id, 1}) == first &&
              !Assets::LoadRevision(workspace, {id, 2}) &&
              !Assets::LoadRevision(workspace, {{}, 1}) &&
              !Assets::LoadRevision(workspace, {id, 0}),
          "Initial exact current revision lookup failed");
  Require(document.Rename(key, "Revision two"), "Revision edit failed");
  const auto second = Assets::Capture(id, document, factory, &*first);
  Require(second && second->revision == 2 && second->nodes == first->nodes, "Capture failed");
  const auto directory = workspace.Root() / ".nexora/prefabs/revisions" / id.ToString();
  std::filesystem::create_directories(directory);
  const auto history = directory / "1.nxprefab";
  auto history_stage = history;
  history_stage += ".tmp";
  std::filesystem::create_directory(history_stage);
  Require(!Assets::Publish(workspace, *second, &*first, &error) && !error.empty() &&
              std::filesystem::is_directory(history_stage) && !std::filesystem::exists(history) &&
              Assets::Load(workspace, id) == first,
          "Foreign archive staging was consumed or current source advanced");
  std::filesystem::remove(history_stage);
  const auto current = workspace.Root() / ".nexora/prefabs" / (id.ToString() + ".nxprefab");
  auto current_stage = current;
  current_stage += ".tmp";
  std::filesystem::create_directory(current_stage);
  Require(!Assets::Publish(workspace, *second, &*first, &error) &&
              Assets::Load(workspace, id) == first &&
              Assets::LoadRevision(workspace, {id, 1}) == first &&
              !std::filesystem::exists(directory / "2.nxprefab") &&
              std::filesystem::is_directory(current_stage),
          "Failed current publication retained an uncommitted future revision");
  std::filesystem::remove(current_stage);
  // Re-edit before retry: failed publication must not occupy revision two with its first draft.
  Require(document.Rename(key, "Retried revision two"), "Retry edit failed");
  const auto retried = Assets::Capture(id, document, factory, &*first);
  Require(retried && retried->scene_bytes != second->scene_bytes &&
              Assets::Publish(workspace, *retried, &*first, &error) &&
              Assets::Load(workspace, id) == retried &&
              Assets::LoadRevision(workspace, {id, 1}) == first &&
              Assets::LoadRevision(workspace, {id, 2}) == retried &&
              !Assets::Publish(workspace, *second, &*first, &error),
          "Retry was poisoned or stale expected revision overwrote current source");
  const auto container = Assets::Capture({601, 9}, document, factory);
  Require(container.has_value(), "Container fixture failed");
  auto nested = *container;
  nested.nested = {{{603, 1}, nested.nodes[0].id, {id, 1}},
                   {{603, 2}, nested.nodes[0].id, {id, 2}}};
  const std::array sources{nested, *first, *retried};
  const auto resolved = Assets::Resolve({nested.id, 1}, sources);
  Require(resolved && resolved->assets.size() == 3 && resolved->instances.size() == 3 &&
              resolved->expanded_nodes == 6 &&
              resolved->instances[1].scope != resolved->instances[2].scope &&
              Assets::Publish(workspace, nested, nullptr, &error) &&
              Assets::ResolveProject(workspace, {nested.id, 1}).has_value(),
          "Different exact revisions of the same asset failed actual nested reopen");
  const std::array duplicate{nested, *first, *first, *retried};
  Require(!Assets::Resolve({nested.id, 1}, duplicate), "Duplicate exact source revision resolved");
  {
    editor::ProjectWorkspace observer;
    Require(observer.Open(workspace.Root(), editor::ProjectAccess::ReadOnly, &error) &&
                Assets::LoadRevision(observer, {id, 1}) == first &&
                Assets::ResolveProject(observer, {nested.id, 1}).has_value() &&
                !Assets::Publish(observer, *second, &*first, &error),
            "Read-only historical access changed source");
  }
  auto third = *retried;
  third.revision = 3;
  const auto history_two = directory / "2.nxprefab";
  Write(history_two, *first);
  Require(!Assets::Publish(workspace, third, &*retried, &error) &&
              Assets::Load(workspace, id) == retried && !Assets::LoadRevision(workspace, {id, 2}) &&
              !Assets::ResolveProject(workspace, {nested.id, 1}),
          "Foreign immutable revision was replaced or hidden by current-file fallback");
  std::filesystem::remove(history_two);
  Require(Assets::Publish(workspace, third, &*retried, &error) &&
              Assets::LoadRevision(workspace, {id, 1}) == first &&
              Assets::LoadRevision(workspace, {id, 2}) == retried &&
              Assets::LoadRevision(workspace, {id, 3}) == third &&
              Assets::ResolveProject(workspace, {nested.id, 1}).has_value(),
          "Multiple committed historical sources did not survive another advance");
  {
    std::ofstream output(history, std::ios::binary | std::ios::trunc);
    output << "corrupt history";
  }
  Require(!Assets::LoadRevision(workspace, {id, 1}) &&
              !Assets::ResolveProject(workspace, {nested.id, 1}) &&
              Assets::Load(workspace, id) == third,
          "Corrupt historical closure accepted or modified current source");
  Write(history, *first);
  {
    std::ofstream output(history, std::ios::binary | std::ios::trunc);
    output.seekp(static_cast<std::streamoff>(Assets::kMaximumAssetBytes));
    output.put('\0');
  }
  Require(!Assets::LoadRevision(workspace, {id, 1}) && Assets::Load(workspace, id) == third,
          "Oversized historical source accepted or current source changed");
  Write(history, *first);
  const auto alias = directory / "foreign-link";
  std::error_code ec;
  std::filesystem::create_hard_link(history, alias, ec);
  if (!ec) {
    Require(!Assets::LoadRevision(workspace, {id, 1}) && std::filesystem::exists(alias),
            "Multiply linked history accepted");
    std::filesystem::remove(alias);
  }
#if defined(__linux__) || defined(__APPLE__)
  const auto parked = directory.parent_path() / "parked";
  std::filesystem::rename(directory, parked);
  std::filesystem::create_directory_symlink(parked, directory);
  auto fourth = third;
  fourth.revision = 4;
  Require(!Assets::LoadRevision(workspace, {id, 1}) && !Assets::LoadRevision(workspace, {id, 3}) &&
              !Assets::Publish(workspace, fourth, &third, &error) &&
              Assets::Load(workspace, id) == third,
          "Aliased history directory accepted or used current fallback");
  std::filesystem::remove(directory);
  std::filesystem::rename(parked, directory);
#endif
  const auto recovery = workspace.Root() / ".nexora/workspace.recovery";
  std::filesystem::create_directory(recovery);
  Require(!Assets::LoadRevision(workspace, {id, 1}) &&
              !Assets::ResolveProject(workspace, {nested.id, 1}) &&
              std::filesystem::is_directory(recovery),
          "Recovery did not freeze historical access");
  std::filesystem::remove(recovery);
  Require(Assets::LoadRevision(workspace, {id, 1}) == first &&
              Assets::ResolveProject(workspace, {nested.id, 1}).has_value(),
          "Historical source recovery failed");
  auto cyclic = *first;
  cyclic.nested = {{{604, 1}, cyclic.nodes[0].id, {nested.id, 1}}};
  Write(history, cyclic);
  Require(Assets::LoadRevision(workspace, {id, 1}) == cyclic &&
              !Assets::ResolveProject(workspace, {nested.id, 1}),
          "Actual historical dependency cycle resolved");
  Write(history, *first);
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
