#include "Nexora/Cryptography/Signature.h"
#include "Nexora/Editor/EditorProduction.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
void Require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}
std::vector<std::byte> Read(const std::filesystem::path &path) {
  std::ifstream stream(path, std::ios::binary);
  Require(stream.is_open(), "Actual artifact reader failed");
  const std::string bytes{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
  Require(!stream.bad(), "Actual artifact read failed");
  std::vector<std::byte> result;
  for (const unsigned char c : bytes)
    result.push_back(static_cast<std::byte>(c));
  return result;
}
constexpr auto kAbc = "sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
constexpr auto kEmpty = "sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
} // namespace

int main() {
  using namespace nexora::editor;
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-artifact-proof-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  {
    std::ofstream first(root / "game.bin", std::ios::binary);
    first << "abc";
    Require(static_cast<bool>(first), "Actual artifact producer failed");
    std::ofstream empty(root / "empty.bin", std::ios::binary);
    Require(static_cast<bool>(empty), "Actual empty artifact producer failed");
  }
  BuildManifest manifest{
      1,
      {"native-dev", "linux-x64", "Development", "cmake --build --preset linux-development"},
      {{"bin/game.bin", kAbc, 3}, {"bin/empty.bin", kEmpty, 0}}};
  std::vector<BuildArtifactInput> inputs{{"bin/empty.bin", Read(root / "empty.bin")},
                                         {"bin/game.bin", Read(root / "game.bin")}};
  const auto original_inputs = inputs;
  std::string error{"stale"};
  const bool available =
      nexora::cryptography::ActiveProvider() != nexora::cryptography::Provider::Unavailable;
  Require(BuildFrontend::VerifyArtifacts(manifest, inputs, &error) == available &&
              (available ? error.empty() : error.find("unavailable") != std::string::npos),
          "Independent known SHA-256 vectors or unavailable-provider contract failed");
  Require(BuildFrontend::Validate(manifest) &&
              BuildFrontend::Write(manifest, root / "manifest.json"),
          "Legacy manifest writer compatibility failed");
  const auto committed = Read(root / "manifest.json");
  auto exact_text = manifest;
  auto exact_inputs = inputs;
  const std::u8string unicode = u8"bin/產物-😀.bin";
  exact_text.artifacts[0].path.assign(unicode.begin(), unicode.end());
  exact_inputs[1].path = exact_text.artifacts[0].path;
  exact_text.profile.command.assign(BuildFrontend::kMaximumVerifiedCommandBytes, 'x');
  Require(BuildFrontend::VerifyArtifacts(exact_text, exact_inputs) == available,
          "Unicode captured identity or exact command limit failed");
  exact_text.artifacts[0].path.assign(BuildFrontend::kMaximumVerifiedPathBytes, 'x');
  exact_inputs[1].path = exact_text.artifacts[0].path;
  Require(BuildFrontend::VerifyArtifacts(exact_text, exact_inputs) == available,
          "Exact captured path limit failed");
  const auto reject = [&](const BuildManifest &candidate,
                          const std::vector<BuildArtifactInput> &captured) {
    error = "stale";
    Require(!BuildFrontend::VerifyArtifacts(candidate, captured, &error) && !error.empty() &&
                error != "stale" && Read(root / "manifest.json") == committed &&
                Read(root / "game.bin") == original_inputs[1].bytes &&
                Read(root / "empty.bin") == original_inputs[0].bytes &&
                !std::filesystem::exists(root / "manifest.json.tmp"),
            "Rejected verification accepted or changed actual source/manifest bytes");
  };
  auto changed = inputs;
  changed[1].bytes[0] = std::byte{'z'};
  reject(manifest, changed);
  changed = inputs;
  changed.pop_back();
  reject(manifest, changed);
  changed = inputs;
  changed.push_back({"bin/extra", {}});
  reject(manifest, changed);
  changed = inputs;
  changed[0] = changed[1];
  reject(manifest, changed);
  changed = inputs;
  changed[0].path = "bin/unlisted";
  reject(manifest, changed);
  changed = inputs;
  changed[1].bytes.pop_back();
  reject(manifest, changed);
  auto invalid = manifest;
  invalid.schema_version = 2;
  reject(invalid, inputs);
  invalid = manifest;
  invalid.artifacts[0].bytes = 4;
  reject(invalid, inputs);
  invalid = manifest;
  invalid.artifacts[1] = invalid.artifacts[0];
  reject(invalid, inputs);
  for (const auto &path : {"../game", "bin/../game", "/bin/game", "bin//game", "bin/game:stream"}) {
    invalid = manifest;
    changed = inputs;
    invalid.artifacts[0].path = path;
    changed[1].path = path;
    reject(invalid, changed);
  }
  for (const auto &checksum :
       {"sha256:bad", "SHA256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "sha256:Ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"}) {
    invalid = manifest;
    invalid.artifacts[0].checksum = checksum;
    reject(invalid, inputs);
  }
  invalid = manifest;
  invalid.profile.command.assign(BuildFrontend::kMaximumVerifiedCommandBytes + 1, 'x');
  reject(invalid, inputs);
  invalid = manifest;
  invalid.profile.name = std::string("name\0hidden", 11);
  reject(invalid, inputs);
  invalid = manifest;
  invalid.profile.target = "\xff";
  reject(invalid, inputs);
  invalid = manifest;
  invalid.profile.configuration = "Development\n";
  reject(invalid, inputs);
  invalid = exact_text;
  changed = exact_inputs;
  invalid.artifacts[0].path += "x";
  changed[1].path = invalid.artifacts[0].path;
  reject(invalid, changed);
  invalid = manifest;
  invalid.artifacts.clear();
  reject(invalid, {});

  // Exact count/byte bounds exercise actual hashes, not a shape-only success shortcut.
  auto boundary = manifest;
  boundary.artifacts.clear();
  std::vector<BuildArtifactInput> captured;
  for (std::size_t i = 0; i < BuildFrontend::kMaximumVerifiedArtifacts; ++i) {
    const auto name = "bin/empty-" + std::to_string(i);
    boundary.artifacts.push_back({name, kEmpty, 0});
    captured.push_back({name, {}});
  }
  Require(BuildFrontend::VerifyArtifacts(boundary, captured) == available,
          "Exact artifact-count boundary failed");
  boundary.artifacts.push_back({"bin/extra", kEmpty, 0});
  captured.push_back({"bin/extra", {}});
  reject(boundary, captured);
  boundary.artifacts.clear();
  captured.clear();
  constexpr auto zero_digest =
      "sha256:080acf35a507ac9849cfcba47dc2ad83e01b75663a516279c8b9d243b719643e";
  for (int i = 0; i < 4; ++i) {
    const auto name = "bin/zero-" + std::to_string(i);
    boundary.artifacts.push_back({name, zero_digest, BuildFrontend::kMaximumVerifiedArtifactBytes});
    captured.push_back(
        {name, std::vector<std::byte>(BuildFrontend::kMaximumVerifiedArtifactBytes)});
  }
  Require(BuildFrontend::VerifyArtifacts(boundary, captured) == available,
          "Exact per-artifact/total bytes failed independent Python hashlib SHA-256 vector");
  boundary.artifacts.push_back({"bin/one-more", kAbc, 1});
  captured.push_back({"bin/one-more", {std::byte{0}}});
  reject(boundary, captured);
  boundary.artifacts.pop_back();
  captured.pop_back();
  ++boundary.artifacts[0].bytes;
  captured[0].bytes.push_back(std::byte{0});
  reject(boundary, captured);
  Require(inputs[0].bytes == original_inputs[0].bytes &&
              inputs[1].bytes == original_inputs[1].bytes,
          "Verification mutated caller-owned capture");
  std::filesystem::remove_all(root);
  return 0;
}
