#include "GameBuildSDKFiles.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <unordered_set>

int main(int argc, char **argv) {
  try {
    if (argc != 2)
      throw std::runtime_error("Expected the canonical SDK source root.");
    std::unordered_set<std::string_view> paths;
    std::size_t total{};
    for (const auto &file : nexora::editor::detail::kGameSDKFiles) {
      if (!paths.insert(file.path).second)
        throw std::runtime_error("Duplicate embedded SDK path.");
      const auto path = std::filesystem::path(argv[1]) / file.path;
      std::ifstream source(path, std::ios::binary);
      if (!source)
        throw std::runtime_error("Canonical SDK source is unavailable: " + path.string());
      const std::string expected{std::istreambuf_iterator<char>(source), {}};
      const auto actual = file.Bytes();
      if (source.bad() || actual != expected)
        throw std::runtime_error("Embedded SDK bytes differ: " + std::string(file.path));
      total += actual.size();
    }
    if (paths.empty() || total == 0)
      throw std::runtime_error("Embedded SDK is empty.");
    std::cout << "Canonical SDK bytes verified: " << paths.size() << " files, " << total
              << " bytes\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
