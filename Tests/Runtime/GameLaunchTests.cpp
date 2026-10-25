#include "Nexora/Runtime/GameLaunch.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora::runtime;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Write(const std::filesystem::path &path, GameLaunch launch) {
  const auto bytes = EncodeGameLaunch(launch);
  Require(bytes.has_value(), "launch fixture encoding failed");
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char *>(bytes->data()),
             static_cast<std::streamsize>(bytes->size()));
  file.close();
  Require(static_cast<bool>(file), "launch fixture writing failed");
}
int Run(const std::filesystem::path &destination = {}) try {
  if (!destination.empty()) {
    Write(destination, {true, 100});
    return 0;
  }
  for (const auto launch :
       {GameLaunch{false, 0}, GameLaunch{true, 100}, GameLaunch{true, 1000000}}) {
    const auto bytes = EncodeGameLaunch(launch);
    Require(bytes && bytes->size() == 24 && DecodeGameLaunch(*bytes) == launch, "launch roundtrip");
  }
  Require(!EncodeGameLaunch({false, 1000001}), "unbounded launch budget accepted");
  const auto valid = *EncodeGameLaunch({true, 100});
  for (std::size_t length = 0; length < valid.size(); ++length)
    Require(!DecodeGameLaunch(std::span(valid).first(length)), "truncated launch admitted");
  for (const auto offset : {0U, 7U, 8U, 15U, 16U, 20U}) {
    auto invalid = valid;
    invalid[offset] = std::byte{255};
    Require(!DecodeGameLaunch(invalid),
            "invalid launch version/flags/budget/ABI/reserved admitted");
  }
  auto extended = valid;
  extended.push_back(std::byte{});
  Require(!DecodeGameLaunch(extended), "trailing launch bytes admitted");
  const auto root = std::filesystem::canonical(std::filesystem::temp_directory_path()) /
                    ("nexora-launch-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  struct Cleanup {
    std::filesystem::path root;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(root, error);
    }
  } cleanup{root};
  const auto path = root / kGameLaunchFilename;
  Write(path, {true, 100});
  Require(ReadGameLaunch(path) == GameLaunch{true, 100}, "physical launch read");
  std::error_code error;
  std::filesystem::create_symlink(path, root / "alias.launch", error);
  if (!error)
    Require(!ReadGameLaunch(root / "alias.launch"), "launch alias admitted");
  else
    std::cout << "Launch alias fixture unavailable on this host\n";
  std::ofstream oversized(path, std::ios::binary | std::ios::trunc);
  oversized << std::string(1024, 'x');
  oversized.close();
  Require(!ReadGameLaunch(path) && !ReadGameLaunch(root / "missing"),
          "invalid launch file admitted");
  std::cout << "Fixed-name launch metadata, exact wire, budgets and file admission passed\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
} // namespace
#if defined(_WIN32)
int wmain(int argc, wchar_t **argv) {
  if (argc == 1)
    return Run();
  return argc == 3 && std::wstring_view(argv[1]) == L"--write-fixture" ? Run(argv[2]) : 2;
}
#else
int main(int argc, char **argv) {
  if (argc == 1)
    return Run();
  return argc == 3 && std::string_view(argv[1]) == "--write-fixture"
             ? Run(std::filesystem::path(
                   std::u8string(argv[2], argv[2] + std::char_traits<char>::length(argv[2]))))
             : 2;
}
#endif
