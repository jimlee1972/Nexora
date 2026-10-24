#include "GameEntry.h"
#include <iostream>

int main() try {
  const auto root = std::filesystem::temp_directory_path() / "game with spaces";
  const auto exe = root / "ExampleGame";
  const auto path = (root / "game.nx2game").u8string();
  const auto result =
      nexora::player::GameArguments(exe, {"ExampleGame", "--frames", "60"}, "game.nx2game");
  if (result != std::vector<std::string>{"ExampleGame", "--run-2d-project",
                                         std::string(path.begin(), path.end()), "--frames", "60"})
    throw std::runtime_error("implicit project must be executable-relative and preserve options");
  for (const auto option : {"--project", "--run-2d-project"}) {
    const std::vector<std::string> explicitArgs{"ExampleGame", option, "another.nx2game"};
    if (nexora::player::GameArguments(exe, explicitArgs, "game.nx2game") != explicitArgs)
      throw std::runtime_error("explicit project selection changed");
  }
  if (nexora::player::GameArguments(exe, {"ExampleGame"}, "game.nx2game").size() != 3)
    throw std::runtime_error("zero-option startup did not select the game");
  std::cout << "named game entry arguments passed\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
