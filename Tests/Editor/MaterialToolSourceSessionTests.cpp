#include "Nexora/Editor/MaterialToolSourceSession.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora;
using namespace nexora::editor;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
void Write(const std::filesystem::path &path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary);
  output << bytes;
  Require(static_cast<bool>(output), "Fixture write failed");
}
MaterialToolSnapshot Snapshot(const MaterialToolSourceSession &session) {
  const auto value = session.Snapshot();
  Require(value.has_value(), "Material owner disappeared");
  return *value;
}
std::vector<std::byte> Changed(const MaterialToolSnapshot &snapshot, float value) {
  auto material = snapshot.material;
  material.roughness = value;
  material.schema.parameters[2].value = value;
  const auto exported = ExportMaterial(material);
  Require(exported.source.has_value(), "Fixture export failed");
  const auto *start = reinterpret_cast<const std::byte *>(exported.source->data());
  return {start, start + exported.source->size()};
}
void Conserved(const MaterialToolSourceSession &session, const MaterialToolSnapshot &expected) {
  const auto current = Snapshot(session);
  Require(current.scope == expected.scope && current.serial == expected.serial &&
              current.source == expected.source && current.saved_source == expected.saved_source &&
              current.dirty == expected.dirty && current.can_undo == expected.can_undo &&
              current.can_redo == expected.can_redo,
          "Rejected operation changed document/history/baseline");
}
void Test(const std::filesystem::path &root) {
  ProjectWorkspace workspace;
  Require(workspace.Create(root, "Material source session") && workspace.SaveWorkspace({}),
          "Project creation failed");
  const auto source = root / std::filesystem::path(u8"Content/材質-Source.nmaterial");
  const std::string initial = "NEXORA_MATERIAL 1\nbase_color 0.2 0.4 0.6\nmetallic 0.25\nroughness "
                              "0.5\nocclusion 1\nemission 0 0 2\n  \n";
  Write(source, initial);
  AssetWorkspace assets;
  Require(assets.ImportTree(root / "Content", {}, {}, AssetIdentityMode::PersistentReadWrite),
          "Asset indexing failed");
  Require(assets.Entries().size() == 1, "Unexpected fixture assets");
  const auto asset = assets.Entries().front().id;
  const auto sidecar = AssetWorkspace::IdentitySidecar(source);
  const auto identity = Read(sidecar);
  ProjectContentSession content;
  Require(content.Open(workspace, assets, 41), "Content open failed");
  MaterialToolSourceSession session(workspace, content);
  Require(session.Open(asset), "Ordinary source did not open");
  const auto original = Snapshot(session);
  Require(original.saved_source == initial && !original.dirty, "Exact raw baseline lost");
  MaterialToolSourceSession other_owner(workspace, content);
  Require(other_owner.Open(asset), "Second material owner failed open");
  const auto foreign = Snapshot(other_owner);
  Require(foreign.scope.generation != original.scope.generation &&
              foreign.serial == original.serial &&
              !session.Apply(foreign.scope, foreign.serial, Changed(foreign, .1F), true) &&
              !other_owner.Apply(original.scope, original.serial, Changed(original, .1F), true),
          "Equal project/asset/serial observations crossed material owners");
  Conserved(session, original);
  Conserved(other_owner, foreign);
  auto bytes = Changed(original, .75F);
  Require(!session.Apply(original.scope, original.serial, bytes, false) &&
              !session.Save(original.scope, original.serial, false),
          "Disabled authoring accepted");
  Conserved(session, original);
  Require(session.Apply(original.scope, original.serial, bytes, true),
          "Material candidate failed apply");
  const auto edited = Snapshot(session);
  Require(edited.dirty && edited.material.roughness == .75F && Read(source) == initial,
          "Authoring wrote source before Save");
  Require(!session.Open(asset) && !session.Close(edited.scope, edited.serial),
          "Dirty discard was implicit");
  auto invalid = edited.scope;
  ++invalid.generation;
  Require(!session.Save(invalid, edited.serial, true) &&
              !session.Save(edited.scope, original.serial, true),
          "Stale observation published");
  bool wrong_thread{};
  std::thread worker([&] {
    wrong_thread = !session.Snapshot() && !session.Open(asset, true) &&
                   !session.Apply(edited.scope, edited.serial, bytes, true) &&
                   !session.Undo(edited.scope, edited.serial, true) &&
                   !session.Redo(edited.scope, edited.serial, true) &&
                   !session.Save(edited.scope, edited.serial, true) &&
                   !session.Close(edited.scope, edited.serial, true);
  });
  worker.join();
  Require(wrong_thread, "Wrong thread reached mutable owner/IO");
  Conserved(session, edited);
  Write(source, "NEXORA_MATERIAL 2\n");
  Require(!session.Save(edited.scope, edited.serial, true) &&
              !session.Apply(edited.scope, edited.serial, bytes, true) &&
              !session.Undo(edited.scope, edited.serial, true) && !session.Open(asset, true),
          "Externally replaced unsupported source was overwritten");
  Conserved(session, edited);
  Require(Read(source) == "NEXORA_MATERIAL 2\n", "Unsupported source lost");
  Write(source, initial);
  Write(root / ".nexora/workspace.recovery", "unresolved evidence");
  Require(!session.Save(edited.scope, edited.serial, true), "Recovery-pending Save accepted");
  Conserved(session, edited);
  std::filesystem::remove(root / ".nexora/workspace.recovery");
  const auto workspace_file = root / ".nexora/workspace";
  const auto stamp = std::filesystem::last_write_time(workspace_file);
  std::filesystem::last_write_time(workspace_file, stamp + std::chrono::seconds(3));
  Require(!session.Save(edited.scope, edited.serial, true), "External workspace change accepted");
  Conserved(session, edited);
  std::filesystem::last_write_time(workspace_file, stamp);
  auto staging = source;
  staging += ".tmp";
  Write(staging, "foreign staging");
  Require(!session.Save(edited.scope, edited.serial, true) && Read(staging) == "foreign staging" &&
              Read(source) == initial,
          "Occupied staging/source was modified");
  Conserved(session, edited);
  std::filesystem::remove(staging);
  const auto alias = root / "alias-copy";
  std::filesystem::create_hard_link(source, alias);
  Require(!session.Save(edited.scope, edited.serial, true) && !session.Open(asset, true),
          "Hardlink source admitted");
  Conserved(session, edited);
  std::filesystem::remove(alias);
#if !defined(_WIN32)
  const auto retained = root / "retained-source";
  std::filesystem::rename(source, retained);
  std::filesystem::create_symlink(retained, source);
  Require(!session.Save(edited.scope, edited.serial, true) && !session.Open(asset, true),
          "Symlink source admitted");
  Conserved(session, edited);
  std::filesystem::remove(source);
  std::filesystem::rename(retained, source);
  const auto content_directory = root / "Content";
  const auto retained_directory = root / "retained-content";
  std::filesystem::rename(content_directory, retained_directory);
  std::filesystem::create_directory_symlink(retained_directory, content_directory);
  Require(!session.Save(edited.scope, edited.serial, true) && !session.Open(asset, true),
          "Aliased parent admitted");
  Conserved(session, edited);
  std::filesystem::remove(content_directory);
  std::filesystem::rename(retained_directory, content_directory);
#endif
  Write(source, std::string(kMaximumMaterialSourceBytes + 1, ' '));
  Require(!session.Save(edited.scope, edited.serial, true) && !session.Open(asset, true),
          "Oversized source admitted");
  Conserved(session, edited);
  Write(source, initial);
  Require(session.Save(edited.scope, edited.serial, true), "Confirmed source publication failed");
  const auto saved = Snapshot(session);
  Require(!saved.dirty && saved.can_undo && saved.saved_source == Read(source) &&
              saved.material.roughness == .75F && content.Browser().Find(asset)->material &&
              content.Browser().Find(asset)->material->roughness == .75F &&
              Read(sidecar) == identity,
          "Confirmed Save lost history/identity or failed to refresh Content material");
  Require(session.Undo(saved.scope, saved.serial, true), "Saved history Undo failed");
  const auto undone = Snapshot(session);
  Require(undone.dirty && undone.material.roughness == .5F && Read(source) == saved.source,
          "Undo changed saved bytes or dirty state");
  Require(session.Redo(undone.scope, undone.serial, true), "Saved history Redo failed");
  const auto redone = Snapshot(session);
  Require(!redone.dirty, "Redo did not restore saved baseline");
  for (unsigned cycle = 0; cycle < 20; ++cycle) {
    auto current = Snapshot(session);
    Require(session.Undo(current.scope, current.serial, true), "Repeated saved Undo failed");
    current = Snapshot(session);
    Require(session.Save(current.scope, current.serial, true), "Repeated undone Save failed");
    current = Snapshot(session);
    Require(session.Redo(current.scope, current.serial, true), "Repeated saved Redo failed");
    current = Snapshot(session);
    Require(session.Save(current.scope, current.serial, true) && Read(source) == saved.source,
            "Repeated Save lost history or exact source");
  }
  const auto current_redone = Snapshot(session);
  Require(session.Close(current_redone.scope, current_redone.serial) && session.Open(asset),
          "Actual save/reopen failed");
  const auto reopened = Snapshot(session);
  Require(!reopened.dirty && !reopened.can_undo && reopened.material.roughness == .75F &&
              reopened.saved_source == Read(source) &&
              reopened.material.base_color == original.material.base_color,
          "Actual reopen lost saved material or retained old history");
  ProjectWorkspace observer;
  Require(observer.Open(root, ProjectAccess::ReadOnly), "Read-only observer failed");
  ProjectContentSession readonly_content;
  Require(readonly_content.Open(observer, assets, 41, false), "Read-only Content failed");
  MaterialToolSourceSession readonly(observer, readonly_content);
  Require(readonly.Open(asset), "Read-only source inspection failed");
  const auto observed = Snapshot(readonly);
  Require(!readonly.Apply(observed.scope, observed.serial, Changed(observed, .25F), true) &&
              !readonly.Save(observed.scope, observed.serial, true),
          "Read-only observer authored/published");
  Conserved(readonly, observed);
  Require(Read(source) == reopened.source, "Read-only source changed");
  auto binding_items =
      std::vector<ContentItem>(content.Browser().Items().begin(), content.Browser().Items().end());
  auto wrong_path = binding_items;
  wrong_path.front().path = "Content/Other.nmaterial";
  Require(content.Browser().Reset(wrong_path, 41) &&
              !session.Save(reopened.scope, reopened.serial, true),
          "Stale asset path accepted");
  Conserved(session, reopened);
  Require(content.Browser().Reset({}, 41) && !session.Save(reopened.scope, reopened.serial, true),
          "Missing asset identity accepted");
  Require(content.Browser().Reset(binding_items, 41), "Asset binding restore failed");
  auto missing_material = binding_items;
  missing_material.front().material.reset();
  const auto cached_material = binding_items.front().material;
  for (std::size_t index = 0; index < kMaximumWorkspaceMaterials; ++index)
    missing_material.push_back({{9, index + 1},
                                "Content/Dummy" + std::to_string(index) + ".nmaterial",
                                ".nmaterial",
                                "budget",
                                ThumbnailState::Ready,
                                {},
                                cached_material});
  Require(content.Browser().Reset(missing_material, 41), "Actual material budget fixture failed");
  Require(session.Apply(reopened.scope, reopened.serial, Changed(reopened, .9F), true),
          "Late-publication candidate failed apply");
  const auto before_late_save = Snapshot(session);
  std::string late_error;
  Require(!session.Save(before_late_save.scope, before_late_save.serial, true, &late_error) &&
              late_error.find("published but Content refresh failed") != late_error.npos &&
              Read(source) == before_late_save.source,
          "Actual late Content-budget failure reported false success or lost published evidence");
  Conserved(session, before_late_save);
  Require(!session.Save(before_late_save.scope, before_late_save.serial, true),
          "Unacknowledged publication retried an old baseline");
  Require(content.Browser().Reset(binding_items, 41) && session.Open(asset, true) &&
              content.Reimport(asset),
          "Explicit reopen after publication failure failed");
  const auto after_late_reopen = Snapshot(session);
  Require(!after_late_reopen.dirty && after_late_reopen.material.roughness == .9F,
          "Explicit reopen did not retain actually published values");
  ProjectWorkspace rebound_workspace;
  Require(rebound_workspace.Create(root / "OtherProject", "Other material source") &&
              rebound_workspace.SaveWorkspace({}),
          "Rebound project fixture failed");
  workspace = std::move(rebound_workspace);
  Require(!session.Save(after_late_reopen.scope, after_late_reopen.serial, true) &&
              !session.Apply(after_late_reopen.scope, after_late_reopen.serial,
                             Changed(after_late_reopen, .1F), true) &&
              Read(source) == after_late_reopen.source,
          "Rebound workspace published into the old source");
  Conserved(session, after_late_reopen);
  Require(workspace.Open(root), "Original project restore failed");
  const auto items =
      std::vector<ContentItem>(content.Browser().Items().begin(), content.Browser().Items().end());
  Require(content.Browser().Reset(items, 42) &&
              !session.Save(after_late_reopen.scope, after_late_reopen.serial, true),
          "Stale Content generation accepted");
  Conserved(session, after_late_reopen);
  Require(session.Close(after_late_reopen.scope, after_late_reopen.serial),
          "Stale owner could not close without IO");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-material-source-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    Test(root);
    std::filesystem::remove_all(root);
    std::cout << "Scoped material source publication/reopen passed\n";
    return 0;
  } catch (const std::exception &failure) {
    std::filesystem::remove_all(root);
    std::cerr << failure.what() << '\n';
    return 1;
  }
}
