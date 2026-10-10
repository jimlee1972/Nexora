#include "Nexora/Editor/EditorWorkspace.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace {
using namespace nexora::editor;
namespace fs = std::filesystem;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Read(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  Require(static_cast<bool>(input), "Actual upgrade fixture is unreadable");
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}
void Write(const fs::path &path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.close();
  Require(!output.fail(), "Actual upgrade fixture write failed");
}
void Run() {
  const auto root = fs::temp_directory_path() /
                    ("nexora-project-upgrade-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root / "Content");
  const std::string original = "schema=1\r\nname=Upgrade 空 白 µ\r\n";
  const auto descriptor = root / "project.nexora";
  Write(descriptor, original);
  std::string error = "old error";
  const auto preview = ProjectWorkspace::PreviewUpgrade(root, &error);
  Require(preview && error.empty() && preview->state == ProjectUpgradeState::Required &&
              preview->project.schema_version == 1 && !preview->project.id.IsNil() &&
              preview->documents.empty() && preview->original_descriptor == original &&
              preview->proposed_descriptor.starts_with("schema=2\nuuid=") &&
              Read(descriptor) == original && !fs::exists(root / ".nexora"),
          "Dry-run mutated a legacy root or lost exact original bytes/identity");
  const auto id = preview->project.id;
  const auto proposed = preview->proposed_descriptor;
  {
    ProjectWorkspace observer;
    Require(observer.Open(root, ProjectAccess::ReadOnly, &error) &&
                observer.UpgradeState() == ProjectUpgradeState::Required &&
                observer.Project().id == id && !fs::exists(root / ".nexora") &&
                Read(descriptor) == original,
            "Read-only legacy open wrote upgrade evidence");
  }
  fs::create_directory(root / ".nexora");
  const auto workspace = root / ".nexora/workspace";
  const std::string workspace_bytes = "schema=1\ndocument=Content/Named.scene\n";
  Write(workspace, workspace_bytes);
  const auto backup = root / ".nexora/project-upgrade.schema1.backup";
  const auto report = root / ".nexora/project-upgrade.schema1.report";
  Write(backup, "foreign backup");
  {
    ProjectWorkspace writer;
    Require(!writer.Open(root, &error) && !error.empty() && writer.Root().empty() &&
                Read(descriptor) == original && Read(backup) == "foreign backup" &&
                !fs::exists(report) && Read(workspace) == workspace_bytes,
            "Foreign backup was overwritten or source/workspace changed");
  }
  fs::remove(backup);
  const auto backup_stage = fs::path(backup.string() + ".tmp");
  fs::create_directory(backup_stage);
  {
    ProjectWorkspace writer;
    Require(!writer.Open(root, &error) && fs::is_directory(backup_stage) && !fs::exists(backup) &&
                Read(descriptor) == original,
            "Foreign backup staging was consumed");
  }
  fs::remove(backup_stage);
  Write(report, "foreign report");
  {
    ProjectWorkspace writer;
    Require(!writer.Open(root, &error) && Read(descriptor) == original &&
                Read(backup) == original && Read(report) == "foreign report",
            "Report conflict did not preserve every version");
  }
  fs::remove(report);
  const auto descriptor_stage = fs::path(descriptor.string() + ".tmp");
  Write(descriptor_stage, "foreign descriptor staging");
  {
    ProjectWorkspace writer;
    Require(!writer.Open(root, &error) && Read(descriptor) == original &&
                Read(backup) == original &&
                Read(descriptor_stage) == "foreign descriptor staging" &&
                Read(report).find("publication=plan-only\n") != std::string::npos,
            "Failed publication lost its last-good source or mislabeled its prepared report");
  }
  const auto prepared_report = Read(report);
  fs::remove(descriptor_stage);
  // A byte-identical hard-link alias is not a dedicated retained original.
  fs::remove(backup);
  fs::create_hard_link(descriptor, backup);
  {
    ProjectWorkspace writer;
    Require(!writer.Open(root, &error) && Read(descriptor) == original &&
                fs::equivalent(descriptor, backup) && Read(report) == prepared_report,
            "Byte-identical aliased backup granted publication");
  }
  fs::remove(backup);
  // After an external source edit, old evidence is retained and cannot grant a new upgrade.
  Write(backup, original);
  const std::string external = "schema=1\nname=External revision\n";
  Write(descriptor, external);
  {
    ProjectWorkspace writer;
    Require(!writer.Open(root, &error) && Read(descriptor) == external &&
                Read(backup) == original && Read(report) == prepared_report,
            "External source revision erased retained evidence");
  }
  Write(descriptor, original);
  {
    ProjectWorkspace writer;
    Require(writer.Open(root, &error) && error.empty() &&
                writer.UpgradeState() == ProjectUpgradeState::Applied &&
                writer.Project().id == id &&
                writer.Project().schema_version == ProjectDescriptor::kSchemaVersion &&
                writer.OpenDocuments().size() == 1 && Read(descriptor) == proposed &&
                Read(backup) == original && Read(report) == prepared_report &&
                Read(workspace) == workspace_bytes,
            "Exact backup/report retry failed to publish stable upgrade");
  }
  {
    ProjectWorkspace reopened;
    Require(reopened.Open(root, &error) &&
                reopened.UpgradeState() == ProjectUpgradeState::Current &&
                reopened.Project().id == id && Read(backup) == original &&
                Read(report) == prepared_report,
            "Successful restart lost stable identity or upgrade evidence");
    const auto current = ProjectWorkspace::PreviewUpgrade(root, &error);
    Require(current && current->state == ProjectUpgradeState::Current &&
                current->original_descriptor == proposed &&
                current->proposed_descriptor == proposed && current->documents.size() == 1 &&
                current->project.id == id,
            "Current schema preview proposed a spurious rewrite");
  }
  {
    ProjectWorkspace retained;
    const auto retained_root = root / "RetainedCurrent";
    Require(retained.Create(retained_root, "Retained authoring", &error),
            "Existing owner fixture failed");
    const std::vector<std::string> retained_documents{"Content/Retained.scene"};
    Require(retained.SaveWorkspace(retained_documents, &error),
            "Existing workspace fixture failed");
    const auto retained_id = retained.Project().id;
    const auto retained_workspace = Read(retained_root / ".nexora/workspace");
    for (const auto &invalid :
         {std::string("schema=99\nname=Future\n"), std::string("schema=1\nname=\n"),
          std::string("schema=2\nuuid=broken\nname=Corrupt\n"),
          std::string("schema=1\nname=Legacy\nextra=record\n"),
          std::string(ProjectWorkspace::kMaximumProjectDescriptorBytes + 1, 'x')}) {
      Write(descriptor, invalid);
      Require(!retained.Open(root, &error) && retained.Root() == fs::canonical(retained_root) &&
                  retained.Project().id == retained_id && retained.Writable() &&
                  retained.OpenDocuments().size() == 1 &&
                  Read(retained_root / ".nexora/workspace") == retained_workspace,
              "Failed candidate open damaged the prior live workspace/lease");
      ProjectWorkspace competing;
      Require(!competing.Open(retained_root, &error),
              "Failed candidate released the prior writer lease");
      Require(!ProjectWorkspace::PreviewUpgrade(root, &error) && !error.empty() &&
                  Read(descriptor) == invalid && Read(backup) == original &&
                  Read(report) == prepared_report && Read(workspace) == workspace_bytes,
              "Unsupported/corrupt/oversized preview changed last-good evidence");
    }
  }
  Write(descriptor, proposed);
  Write(workspace, "schema=99\n");
  Require(!ProjectWorkspace::PreviewUpgrade(root, &error) && Read(descriptor) == proposed &&
              Read(workspace) == "schema=99\n" && Read(backup) == original,
          "Corrupt workspace preview changed source/backup");
  const auto absent = root / "Missing";
  Require(!ProjectWorkspace::PreviewUpgrade(absent, &error) && !fs::exists(absent),
          "Missing-root preview created files");
  fs::remove_all(root);
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Owning project upgrade previews and retained evidence passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
