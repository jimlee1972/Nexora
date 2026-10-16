#pragma once
#include "nexora/nexora.h"
#include <filesystem>
#include <iostream>
#include <vector>
namespace nexora::player {
int RunTwoDPlayer(int argc, char **argv, NexoraGameModuleLoadV3Fn builtin = nullptr);
template <typename Char>
int DispatchTwoDPlayer(int argc, Char **argv, NexoraGameModuleLoadV3Fn builtin = nullptr) try {
  std::vector<std::string> strings;
  std::vector<char *> arguments;
  strings.reserve(static_cast<std::size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    const auto value = std::filesystem::path(argv[i]).u8string();
    strings.emplace_back(value.begin(), value.end());
  }
  for (auto &value : strings)
    arguments.push_back(value.data());
  return RunTwoDPlayer(argc, arguments.data(), builtin);
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 2;
}
} // namespace nexora::player
