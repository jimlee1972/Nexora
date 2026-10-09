#include "Nexora/Editor/SceneAuthoring.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
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

void TestRecoveryAdmission(const fs::path &root) {
  const auto path = root / "recovery-admission";
  const auto read = [&] {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  };
  const auto reject = [&](const std::string &bytes) {
    {
      std::ofstream output(path, std::ios::binary | std::ios::trunc);
      output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    std::uint64_t revision = 99;
    std::string error;
    Require(!AutosaveJournal::Recover(path, &revision, &error) && !error.empty() &&
                revision == 99 && read() == bytes && !fs::exists(path.string() + ".tmp"),
            "rejected recovery changed caller revision, source bytes or staging");
  };
  for (const auto *header :
       {"NEXORA_AUTOSAVE 1 -1 0\n", "NEXORA_AUTOSAVE 1 +1 0\n", "NEXORA_AUTOSAVE 1 1 -0\n",
        "NEXORA_AUTOSAVE 1 1 +0\n", "NEXORA_AUTOSAVE 1 18446744073709551616 0\n",
        "NEXORA_AUTOSAVE 1 1 18446744073709551616\n", "NEXORA_AUTOSAVE 2 1 0\n",
        "NEXORA_AUTOSAVE 1 1 0 extra\n", "NEXORA_AUTOSAVE 1 1 67108864\n",
        "NEXORA_AUTOSAVE 1 1 0\ntrailing"})
    reject(header);
  reject(std::string(2 * 1024 * 1024, 'x') + " 1 1 0\n");
  reject("NEXORA_AUTOSAVE 1 " + std::string(128, '0') + " 0\n");
  const std::string prefix = "NEXORA_AUTOSAVE 1 ";
  const std::string maximum_header = prefix + std::string(127 - prefix.size() - 2, '0') + " 0\n";
  reject(prefix + std::string(128 - prefix.size() - 2, '0') + " 0\n");
  std::ofstream(path, std::ios::binary | std::ios::trunc) << maximum_header;
  std::uint64_t boundary_revision = 99;
  Require(AutosaveJournal::Recover(path, &boundary_revision) == std::optional<std::string>{""} &&
              boundary_revision == 0 && read() == maximum_header,
          "exact header budget did not recover without modifying source");
  constexpr char nul_header[] = "NEXORA_AUTOSAVE 1 1 0\0\n";
  reject(std::string(nul_header, sizeof(nul_header) - 1));
  const std::string valid = "NEXORA_AUTOSAVE 1 18446744073709551615 3\n" + std::string("a\0b", 3);
  for (std::size_t count = 0; count < valid.size(); ++count)
    reject(valid.substr(0, count));
  std::string error = "previous error";
  std::uint64_t revision{};
  const std::string binary("a\0b", 3);
  Require(AutosaveJournal::Write(path, std::numeric_limits<std::uint64_t>::max(), binary) &&
              AutosaveJournal::Recover(path, &revision, &error) == std::optional{binary} &&
              revision == std::numeric_limits<std::uint64_t>::max() && error.empty(),
          "maximum revision or stale error clearing failed");

  fs::resize_file(path, 64 * 1024 * 1024 + 129);
  revision = 99;
  Require(!AutosaveJournal::Recover(path, &revision, &error) && !error.empty() && revision == 99 &&
              fs::file_size(path) == 64 * 1024 * 1024 + 129,
          "oversized source was admitted or modified");
  fs::remove(path);
  fs::create_directory(path);
  std::ofstream(path / "keep") << "keep";
  Require(!AutosaveJournal::Recover(path, &revision, &error) && revision == 99 &&
              fs::exists(path / "keep"),
          "non-regular recovery changed caller revision or directory");
  fs::remove_all(path);
#if !defined(_WIN32)
  const auto target = root / "alias-target";
  Require(AutosaveJournal::Write(target, 7, "last good"), "alias target setup failed");
  fs::create_symlink(target, path);
  Require(!AutosaveJournal::Recover(path, &revision, &error) && !error.empty() && revision == 99 &&
              fs::is_symlink(path),
          "recovery followed an aliased journal");
  RequireBaseline(target);
  fs::remove(target);
  Require(!AutosaveJournal::Recover(path, &revision, &error) && revision == 99 &&
              fs::is_symlink(path) && !fs::exists(target),
          "recovery modified a dangling alias");
#endif
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
    TestRecoveryAdmission(root);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "editor.autosave_journal: " << error.what() << '\n';
    return 1;
  }
}
