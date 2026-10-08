#include "Nexora/Editor/EditorProduction.h"
#include "Nexora/Editor/EditorWorkspace.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
std::string Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Require(static_cast<bool>(input), "workspace fixture could not be read");
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
void Write(const std::filesystem::path &path, const std::string &text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << text;
  output.close();
  Require(!output.fail(), "workspace fixture could not be written");
}
int Run() {
  namespace fs = std::filesystem;
  using nexora::editor::ProjectAccess;
  using nexora::editor::ProjectWorkspace;
  const auto root = fs::temp_directory_path() /
                    ("nexora-workspace-budget-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup final {
    fs::path root;
    ~Cleanup() {
      std::error_code error;
      fs::remove_all(root, error);
    }
  } cleanup{root};
  ProjectWorkspace writer;
  std::string error;
  Require(writer.Create(root, "Workspace Budget", &error), "fixture project creation failed");
  const auto primary = root / ".nexora/workspace";
  const auto recovery = root / ".nexora/workspace.recovery";
  const std::vector<std::string> original{"Content/Original.scene"};
  Require(writer.SaveWorkspace(original, &error), "fixture workspace save failed");
  const auto committed = Read(primary);
  const std::string journal = "schema=1\ndocument=Content/Recovered.scene\n";
  Write(recovery, journal);
  std::vector<std::string> oversized(ProjectWorkspace::kMaximumDocuments + 1, "Scene.scene");
  Require(!writer.SaveWorkspace(oversized, &error) && error.find("too many") != std::string::npos &&
              Read(primary) == committed && Read(recovery) == journal &&
              writer.OpenDocuments().size() == 1 &&
              writer.OpenDocuments().front() == original.front() &&
              !fs::exists(primary.string() + ".tmp") && !fs::exists(recovery.string() + ".tmp"),
          "over-budget save changed committed state, journal or staging");
  oversized = {std::string(ProjectWorkspace::kMaximumDocumentPathBytes + 1, 'a')};
  Require(!writer.SaveWorkspace(oversized, &error) && Read(primary) == committed &&
              Read(recovery) == journal,
          "overlong path save changed committed data");
  Require(writer.RecoverWorkspace(&error) && writer.OpenDocuments().size() == 1 &&
              writer.OpenDocuments().front() == "Content/Recovered.scene" && !fs::exists(recovery),
          "valid journal was lost after rejecting an over-budget save");

  const std::string longest(ProjectWorkspace::kMaximumDocumentPathBytes, 'a');
  const std::vector<std::string> maximum(ProjectWorkspace::kMaximumDocuments, longest);
  Require(writer.SaveWorkspace(maximum, &error), "maximum supported workspace did not save");
  ProjectWorkspace observer;
  Require(observer.Open(root, ProjectAccess::ReadOnly, &error) &&
              observer.OpenDocuments().size() == maximum.size() &&
              observer.OpenDocuments().back() == longest,
          "maximum supported workspace did not reopen");
  // Exact-size CRLF records and a final record without LF remain compatible.
  Write(primary, "schema=1\r\ndocument=" + longest + "\r\n");
  Require(observer.Open(root, ProjectAccess::ReadOnly, &error) &&
              observer.OpenDocuments().size() == 1 && observer.OpenDocuments().front() == longest,
          "maximum-length CRLF document was rejected");
  Write(primary, "schema=1\ndocument=Content/NoNewline.scene");
  Require(observer.Open(root, ProjectAccess::ReadOnly, &error) &&
              observer.OpenDocuments().front() == "Content/NoNewline.scene",
          "compatible final record without newline was rejected");
  const auto expect_rejected_open = [&](const std::string &text) {
    Write(primary, text);
    Require(!observer.Open(root, ProjectAccess::ReadOnly, &error) && !error.empty() &&
                observer.OpenDocuments().size() == 1 &&
                observer.OpenDocuments().front() == "Content/NoNewline.scene" &&
                Read(primary) == text,
            "malformed workspace changed the previous observer or source");
  };
  expect_rejected_open("schema=1\ndocument=" + std::string(4 * 1024 * 1024, 'a'));
  Require(error.find("overlong") != std::string::npos,
          "overlong record was consumed before the parser budget rejected it");
  expect_rejected_open(std::string(4 * 1024 * 1024, 'a'));
  expect_rejected_open(std::string("schema=1\ndocument=Bad\0Path\n", 27));
  expect_rejected_open("schema=1\ndocument=\n");
  std::string too_many = "schema=1\n";
  for (std::size_t index = 0; index <= ProjectWorkspace::kMaximumDocuments; ++index)
    too_many += "document=Scene.scene\n";
  expect_rejected_open(too_many);

  Require(writer.SaveWorkspace(original, &error), "workspace restore failed");
  const auto before_recovery = Read(primary);
  for (const auto &bad : {std::string("schema=1\ndocument=") + std::string(4 * 1024 * 1024, 'a'),
                          too_many, std::string("schema=999\n"), std::string{}}) {
    Write(recovery, bad);
    Require(!writer.RecoverWorkspace(&error) && !error.empty() &&
                Read(primary) == before_recovery && Read(recovery) == bad &&
                writer.OpenDocuments().size() == 1 &&
                writer.OpenDocuments().front() == original.front(),
            "invalid recovery changed committed data, authoring model or the journal");
  }
  Write(recovery, "schema=1\r\ndocument=" + longest + "\r\n");
  Require(writer.RecoverWorkspace(&error) && writer.OpenDocuments().size() == 1 &&
              writer.OpenDocuments().front() == longest && !fs::exists(recovery),
          "bounded recovery rejected the maximum-length CRLF record");

#if defined(__linux__)
  // Linux supports complete paths longer than the recent-store record budget. Other hosts may
  // reject these project roots at the filesystem layer before the store is reached.
  nexora::editor::RecentProjectStore recents;
  const auto recent_path = root / "recent-projects";
  Require(recents.Open(recent_path, &error) && recents.Record(writer, &error),
          "recent-project fixture could not be recorded");
  const auto recent_bytes = Read(recent_path);
  auto deep_root = root / "deep";
  while (deep_root.string().size() <= ProjectWorkspace::kMaximumDocumentPathBytes)
    deep_root /= std::string(100, 'p');
  ProjectWorkspace deep;
  Require(deep.Create(deep_root, "Deep Project", &error), "long-root project fixture failed");
  Require(!recents.Record(deep, &error) && !error.empty() && Read(recent_path) == recent_bytes &&
              recents.Entries().size() == 1 &&
              recents.Entries().front().id == writer.Project().id &&
              !fs::exists(recent_path.string() + ".tmp"),
          "unsupported recent-project root replaced the last-good store or model");
  nexora::editor::RecentProjectStore reopened;
  Require(reopened.Open(recent_path, &error) && reopened.Entries().size() == 1 &&
              reopened.Entries().front().id == writer.Project().id,
          "rejected recent-project root made the last-good store unreadable");
#endif

  const auto layout_path = root / ".nexora/editor-layout.ini";
  const std::string layout = "[Window][Hierarchy]\nPos=0,0\n";
  Require(writer.SaveEditorLayout(layout, &error) && writer.LoadEditorLayout(&error) == layout,
          "layout fixture failed");
  const auto layout_bytes = Read(layout_path);
  const auto stage = fs::path(layout_path.string() + ".tmp");
  Write(stage, "occupied layout staging");
  for (const auto &invalid_layout :
       {std::string{}, std::string(ProjectWorkspace::kMaximumEditorLayoutBytes + 1, 'a'),
        std::string("bad\0layout", 10)}) {
    Require(!writer.SaveEditorLayout(invalid_layout, &error) && !error.empty() &&
                Read(layout_path) == layout_bytes && Read(stage) == "occupied layout staging",
            "invalid layout save changed last-good bytes or unrelated staging");
  }
  fs::remove(stage);
  Require(!observer.SaveEditorLayout(layout, &error) && Read(layout_path) == layout_bytes &&
              !fs::exists(stage),
          "read-only layout save changed the last-good file");
  const std::string maximum_layout(ProjectWorkspace::kMaximumEditorLayoutBytes, 'a');
  Require(writer.SaveEditorLayout(maximum_layout, &error) &&
              observer.LoadEditorLayout(&error) == maximum_layout && error.empty(),
          "maximum layout did not round-trip");
  Write(layout_path, "schema=0\r\n" + maximum_layout);
  Require(observer.LoadEditorLayout(&error) == maximum_layout,
          "maximum legacy layout with CRLF header was rejected");
  Write(layout_path, "schema=1\r\n[Window][Hierarchy]\r\nPos=0,0\r\n");
  Require(observer.LoadEditorLayout(&error) == layout, "layout CRLF normalization failed");
  std::string many_lines;
  for (std::size_t index = 0; index < ProjectWorkspace::kMaximumEditorLayoutBytes / 2; ++index)
    many_lines += "\r\n";
  Write(layout_path, "schema=1\n" + many_lines);
  Require(observer.LoadEditorLayout(&error) ==
              std::string(ProjectWorkspace::kMaximumEditorLayoutBytes / 2, '\n'),
          "maximum CRLF layout did not normalize");
  for (const auto &invalid_file :
       {std::string{}, std::string("schema=1\n"), std::string("schema=999\n") + layout,
        std::string("schema=1\n") +
            std::string(ProjectWorkspace::kMaximumEditorLayoutBytes + 1, 'a'),
        std::string("schema=1\r\n") +
            std::string(ProjectWorkspace::kMaximumEditorLayoutBytes + 1, 'a'),
        std::string(4 * 1024 * 1024, 'a'), std::string("schema=1\nbad\0layout", 19)}) {
    Write(layout_path, invalid_file);
    Require(!observer.LoadEditorLayout(&error) && !error.empty() &&
                Read(layout_path) == invalid_file,
            "invalid layout was accepted or rewritten");
  }
  fs::remove(layout_path);
  error = "stale";
  Require(!observer.LoadEditorLayout(&error) && error.empty(),
          "missing optional layout reported corruption or retained an error");
  fs::create_directory(layout_path);
  Require(!observer.LoadEditorLayout(&error) && !error.empty() && fs::is_directory(layout_path),
          "directory layout was accepted or changed");
  fs::remove(layout_path);
#if !defined(_WIN32)
  const auto layout_target = root / "aliased-layout";
  Write(layout_target, layout_bytes);
  fs::create_symlink(layout_target, layout_path);
  Require(!observer.LoadEditorLayout(&error) && !error.empty() && fs::is_symlink(layout_path) &&
              Read(layout_target) == layout_bytes,
          "layout reader followed an alias or changed its target");
  fs::remove(layout_path);
  fs::create_symlink(root / "missing-layout-target", layout_path);
  Require(!observer.LoadEditorLayout(&error) && !error.empty() && fs::is_symlink(layout_path),
          "dangling layout alias was treated as missing");
  fs::remove(layout_path);
#endif
  Require(writer.SaveEditorLayout(layout, &error) && observer.LoadEditorLayout(&error) == layout,
          "layout did not recover after rejected inputs");

  const auto recovery_primary = Read(primary);
  const std::array frame_samples{nexora::editor::FrameSample{1, 1, 0, 0}};
  const auto expect_pending = [&] {
    Require(writer.HasRecoveryJournal() && observer.HasRecoveryJournal(),
            "occupied recovery was mistaken for an absent journal");
    Require(!writer.RecoverWorkspace(&error) && !error.empty() && Read(primary) == recovery_primary,
            "unsafe recovery changed the committed workspace");
    Require(!writer.ExportEditorFrameProcessing(frame_samples, 0, &error) &&
                !writer.ExportEditorFrameProcessingJson(frame_samples, 0, &error) &&
                !fs::exists(root / ".nexora/frame-processing.csv") &&
                !fs::exists(root / ".nexora/frame-processing.json"),
            "occupied recovery unlocked frame export");
    Require(!writer.ImportEditorFrameProcessingJson(&error) &&
                !observer.ImportEditorFrameProcessingJson(&error) && writer.HasRecoveryJournal(),
            "occupied recovery unlocked JSON import");
    Require(!observer.DiscardRecovery(&error) && observer.HasRecoveryJournal(),
            "read-only observer discarded occupied recovery metadata");
  };
  Require(!ProjectWorkspace{}.HasRecoveryJournal() && !writer.HasRecoveryJournal(),
          "empty or missing recovery was reported as pending");
  fs::create_directory(recovery);
  Write(recovery / "retained", "pending recovery contents");
  expect_pending();
  Require(!writer.DiscardRecovery(&error) && writer.HasRecoveryJournal() &&
              Read(recovery / "retained") == "pending recovery contents",
          "discard recursively deleted occupied recovery contents");
  fs::remove(recovery / "retained");
  Require(writer.DiscardRecovery(&error) && !writer.HasRecoveryJournal(),
          "explicit discard did not resolve an empty recovery directory");
#if !defined(_WIN32)
  const auto recovery_target = root / "recovery-alias-target";
  Write(recovery_target, "schema=1\ndocument=Aliased.scene\n");
  const auto target_bytes = Read(recovery_target);
  for (const auto &target : {recovery_target, root / "missing-recovery-target"}) {
    fs::create_symlink(target, recovery);
    expect_pending();
    Require(writer.DiscardRecovery(&error) && !writer.HasRecoveryJournal() &&
                Read(recovery_target) == target_bytes,
            "explicit alias discard changed its target or left recovery pending");
  }
#endif

  const auto metadata = root / ".nexora";
  const auto retained_metadata = root / "retained-metadata";
  // Windows prevents renaming directories containing the writer's open lock handle. Inspect
  // parent fixtures through the lease-free observer, then reacquire the writer after restoration.
  writer = ProjectWorkspace{};
  fs::rename(metadata, retained_metadata);
  Write(metadata, "occupied metadata parent");
  Require(observer.HasRecoveryJournal(), "uninspectable recovery path was reported as absent");
  fs::remove(metadata);
  fs::rename(retained_metadata, metadata);
  Require(!observer.HasRecoveryJournal() && Read(primary) == recovery_primary,
          "restoring inspectable metadata changed the workspace or left recovery pending");
  fs::rename(metadata, retained_metadata);
  Require(!observer.HasRecoveryJournal(), "missing legacy metadata was reported as recovery");
#if !defined(_WIN32)
  fs::create_symlink(retained_metadata, metadata);
  Require(!observer.HasRecoveryJournal(),
          "valid metadata directory alias changed missing recovery");
  fs::remove(metadata);
  fs::create_symlink(root / "missing-metadata-target", metadata);
  Require(observer.HasRecoveryJournal(), "dangling metadata parent unlocked recovery gates");
  fs::remove(metadata);
#endif
  fs::rename(retained_metadata, metadata);
  Require(writer.Open(root, &error) && !writer.HasRecoveryJournal() &&
              Read(primary) == recovery_primary,
          "metadata alias fixtures changed committed state");

  fs::remove(primary);
  Require(observer.Open(root, ProjectAccess::ReadOnly, &error) && observer.OpenDocuments().empty(),
          "missing legacy workspace was no longer accepted");
  fs::create_directory(primary);
  Require(!observer.Open(root, ProjectAccess::ReadOnly, &error) && !error.empty(),
          "directory workspace was treated as missing");
  fs::remove(primary);
#if !defined(_WIN32)
  const auto outside = root / "aliased-workspace";
  Write(outside, committed);
  fs::create_symlink(outside, primary);
  Require(!observer.Open(root, ProjectAccess::ReadOnly, &error) && fs::is_symlink(primary) &&
              Read(outside) == committed,
          "workspace reader followed an aliased metadata file");
  fs::remove(primary);
  fs::create_symlink(outside, recovery);
  Require(!writer.RecoverWorkspace(&error) && fs::is_symlink(recovery) &&
              Read(outside) == committed,
          "recovery reader followed or deleted an aliased journal");
#endif
  return 0;
}
} // namespace
int main() {
  try {
    return Run();
  } catch (const std::exception &error) {
    std::cerr << "editor.workspace_budget: " << error.what() << '\n';
    return 1;
  }
}
