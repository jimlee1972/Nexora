#include "Nexora/Editor/EditorWorkspace.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {
int Run(int argc, char **argv) {
  std::filesystem::path project;
  std::filesystem::path report;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument.starts_with("--project="))
      project = argument.substr(10);
    else if (argument.starts_with("--report="))
      report = argument.substr(9);
    else if (argument == "--help") {
      std::cout << "NexoraEditor --project=PATH [--report=PATH]\n";
      return 0;
    } else {
      std::cerr << "unknown argument: " << argument << '\n';
      return 2;
    }
  }
  if (project.empty()) {
    std::cerr << "--project is required\n";
    return 2;
  }
  nexora::editor::ProjectWorkspace workspace;
  std::string error;
  if (!workspace.Open(project, &error)) {
    std::cerr << error << '\n';
    return 1;
  }
  nexora::editor::AssetWorkspace assets;
  if (!assets.ImportTree(project / "Content")) {
    std::cerr << "content indexing failed\n";
    return 1;
  }
  const std::string json =
      "{\n  \"application\": \"NexoraEditor\",\n  \"project\": \"" + workspace.Project().name +
      "\",\n  \"panels\": " + std::to_string(nexora::editor::ProductShell::Panels().size()) +
      ",\n  \"assets\": " + std::to_string(assets.Entries().size()) + "\n}\n";
  if (report.empty())
    std::cout << json;
  else {
    std::ofstream output(report);
    if (!output || !(output << json))
      return 1;
  }
  return 0;
}
} // namespace

int main(int argc, char **argv) { return Run(argc, argv); }
