#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace nexora::player {
// Explicit diagnostic project selection retains the normal Player CLI contract.
inline std::vector<std::string> GameArguments(const std::filesystem::path &executable,
                                              std::vector<std::string> arguments,
                                              const std::string &descriptor) {
  if (arguments.empty())
    throw std::invalid_argument("missing executable argument");
  for (std::size_t i = 1; i < arguments.size(); ++i)
    if (arguments[i] == "--project" || arguments[i] == "--run-2d-project")
      return arguments;
  const auto path = (executable.parent_path() / descriptor).u8string();
  arguments.insert(arguments.begin() + 1,
                   {"--run-2d-project", std::string(path.begin(), path.end())});
  return arguments;
}
} // namespace nexora::player
