#include "Nexora/Editor/SceneAuthoring.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <locale>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;
using nexora::editor::AutosaveJournal;

void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

struct Cleanup final {
  fs::path root;
  ~Cleanup() {
    std::error_code error;
    fs::remove_all(root, error);
  }
};

struct GroupedNumbers final : std::numpunct<char> {
  char do_thousands_sep() const override { return '_'; }
  std::string do_grouping() const override { return "\3"; }
};

struct RestoreLocale final {
  std::locale previous;
  ~RestoreLocale() { std::locale::global(previous); }
};

void RequireBaseline(const fs::path &path) {
  std::uint64_t revision{};
  Require(AutosaveJournal::Recover(path, &revision) == std::optional<std::string>{"last good"} &&
              revision == 7,
          "failed write replaced the recoverable baseline");
}

void TestOversized(const fs::path &path) {
  Require(AutosaveJournal::Write(path, 7, "last good"), "baseline write failed");
  std::string payload(64 * 1024 * 1024 + 1, 'x');
  std::string error;
  Require(!AutosaveJournal::Write(path, 8, payload, &error) && !error.empty(),
          "oversized write must fail before replacing a recoverable journal");
  RequireBaseline(path);
  Require(!fs::exists(path.string() + ".tmp"), "oversized write left a temporary file");
  payload.pop_back();
  Require(AutosaveJournal::Write(path, 8, payload, &error), "maximum-sized payload rejected");
  std::uint64_t revision{};
  const auto recovered = AutosaveJournal::Recover(path, &revision, &error);
  Require(recovered && *recovered == payload && revision == 8,
          "maximum-sized payload could not be recovered");
}

void TestOccupiedTemporary(const fs::path &path) {
  Require(AutosaveJournal::Write(path, 7, "last good"), "baseline write failed");
  const fs::path temporary = path.string() + ".tmp";
  {
    std::ofstream output(temporary, std::ios::binary);
    output << "another writer";
  }
  std::string error;
  Require(!AutosaveJournal::Write(path, 8, "replacement", &error) && !error.empty(),
          "write must reject an occupied temporary path");
  RequireBaseline(path);
  std::ifstream input(temporary, std::ios::binary);
  std::string retained((std::istreambuf_iterator<char>(input)), {});
  Require(retained == "another writer", "write modified another writer's temporary file");
  input.close();
  fs::remove(temporary);
  fs::create_directory(temporary);
  Require(!AutosaveJournal::Write(path, 8, "replacement", &error) && fs::is_directory(temporary),
          "write removed an occupied temporary directory");
  RequireBaseline(path);
  fs::remove(temporary);
#if !defined(_WIN32)
  const auto target = path.parent_path() / "missing-symlink-target";
  fs::create_symlink(target, temporary);
  Require(!AutosaveJournal::Write(path, 8, "replacement", &error) && fs::is_symlink(temporary) &&
              !fs::exists(target),
          "write followed or removed a dangling temporary symlink");
  RequireBaseline(path);
  fs::remove(temporary);
#endif
}

void TestReplacementFailure(const fs::path &root) {
  const auto path = root / "occupied-destination";
  fs::create_directory(path);
  std::ofstream(path / "keep") << "keep";
  std::string error;
  Require(!AutosaveJournal::Write(path, 9, "payload", &error) && !error.empty() &&
              fs::is_directory(path) && fs::exists(path / "keep") &&
              !fs::exists(path.string() + ".tmp"),
          "failed replacement must retain destination and clean its own temporary file");
  fs::remove_all(path);
  Require(AutosaveJournal::Write(path, 10, "retry"), "replacement failure blocked a later retry");
}

void TestRoundTripAndCorruption(const fs::path &path) {
  RestoreLocale restore{std::locale()};
  std::locale::global(std::locale(restore.previous, new GroupedNumbers));
  std::string error;
  std::uint64_t revision{};
  Require(AutosaveJournal::Write(path, 0, "", &error) &&
              AutosaveJournal::Recover(path, &revision, &error) == std::optional<std::string>{""} &&
              revision == 0,
          "empty payload round trip failed");
  const std::string binary("a\0b\n\xff", 5);
  Require(AutosaveJournal::Write(path, 12345, binary, &error) &&
              AutosaveJournal::Recover(path, &revision, &error) == std::optional{binary} &&
              revision == 12345,
          "binary payload round trip failed");
  for (const auto header : {"NEXORA_AUTOSAVE 1 12 67108865\n", "NEXORA_AUTOSAVE 1 12 3\nab",
                            "NEXORA_AUTOSAVE 1 12 2\nabc"}) {
    std::ofstream(path, std::ios::binary | std::ios::trunc) << header;
    revision = 99;
    Require(!AutosaveJournal::Recover(path, &revision, &error) && !error.empty() && revision == 99,
            "invalid recovery mutated the caller's revision or accepted corrupt content");
  }
}
} // namespace

int main() {
  try {
    const auto root = fs::temp_directory_path() /
                      ("nexora-autosave-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Cleanup cleanup{root};
    fs::create_directories(root);
    TestOversized(root / "limit.autosave");
    TestOccupiedTemporary(root / "occupied.autosave");
    TestReplacementFailure(root);
    TestRoundTripAndCorruption(root / "binary.autosave");
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "editor.autosave_journal: " << error.what() << '\n';
    return 1;
  }
}
