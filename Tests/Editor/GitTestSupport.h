#pragma once
#include "Nexora/Editor/BuildProcess.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace nexora::editor::test {
inline std::string RunGit(core::JobSystem &jobs, const std::filesystem::path &tool,
                          const std::filesystem::path &root, std::vector<std::string> arguments,
                          std::uint32_t expected = 0) {
  BuildProcess process(jobs);
  std::vector<std::string> command{"-c", "user.name=Nexora Tests",
                                   "-c", "user.email=tests@example.invalid",
                                   "-c", "commit.gpgsign=false"};
  command.insert(command.end(), arguments.begin(), arguments.end());
  if (!process.Start({9000, tool, root, std::move(command), 64 * 1024}))
    throw std::runtime_error("Git fixture launch failed");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!process.Poll(9000)) {
    if (std::chrono::steady_clock::now() >= deadline)
      throw std::runtime_error("Git fixture setup timed out");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const auto result = process.Snapshot();
  if (result.exit_code != expected)
    std::cerr << "Fixture Git output: " << result.output << '\n';
  if (result.exit_code != expected || result.dropped_output_bytes)
    throw std::runtime_error("Git fixture command failed");
  return result.output;
}
} // namespace nexora::editor::test
