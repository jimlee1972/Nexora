#include "Nexora/Editor/EditorWorkspace.h"

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
