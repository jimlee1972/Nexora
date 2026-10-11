#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/PluginManager.h"
#include "Nexora/Foundation/BuildInfo.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#if defined(NEXORA_TEST_OPENSSL)
#include <openssl/evp.h>
#endif

namespace {
using namespace nexora;
namespace fs = std::filesystem;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::vector<std::byte> Hex(std::string_view text) {
  const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  std::vector<std::byte> bytes;
  for (std::size_t i = 0; i < text.size(); i += 2)
    bytes.push_back(static_cast<std::byte>((digit(text[i]) << 4) | digit(text[i + 1])));
  return bytes;
}
std::vector<std::byte> Read(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  Require(static_cast<bool>(input), "Fixture source unavailable");
  const std::string text{std::istreambuf_iterator<char>{input}, {}};
  const auto *first = reinterpret_cast<const std::byte *>(text.data());
  return {first, first + text.size()};
}
void Write(const fs::path &path, std::span<const std::byte> bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  output.close();
  Require(!output.fail(), "Fixture write failed");
}
std::string Target() {
#if defined(__ANDROID__)
  return "android-native";
#elif defined(__linux__) && defined(__aarch64__)
  return "linux-aarch64";
#elif defined(__linux__)
  return "linux-x86_64";
#elif defined(_WIN32)
  return "windows-x86_64";
#elif defined(__APPLE__)
  return "macos-native";
#else
  return "unsupported-native";
#endif
}
editor::ExtensionManifest Manifest(std::string id = "sample.plugin") {
  editor::ExtensionManifest m{
      std::move(id), "1.0.0", "known.vendor", Target(), foundation::kEngineAbiVersion, 3,
      {"core"},      {}};
  m.artifact_digest[0] = std::byte{1};
  return m;
}
#if defined(NEXORA_TEST_OPENSSL)
editor::SignedExtensionPackage Package(editor::ExtensionManifest m,
                                       std::vector<std::byte> artifact) {
  const auto hash = cryptography::Sha256(artifact);
  Require(hash.has_value(), "Real digest unavailable");
  m.artifact_digest = *hash;
  const auto bytes = editor::SignedExtensionHost::EncodeManifest(m);
  Require(bytes.has_value(), "Manifest fixture invalid");
  // RFC 8032 TEST 1's public seed, never a user's credential.
  const auto seed = Hex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
  auto *key = EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, nullptr, reinterpret_cast<const unsigned char *>(seed.data()), seed.size());
  auto *ctx = EVP_MD_CTX_new();
  std::array<std::byte, 64> signature{};
  std::size_t count = signature.size();
  const bool ok =
      key && ctx && EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, key) == 1 &&
      EVP_DigestSign(ctx, reinterpret_cast<unsigned char *>(signature.data()), &count,
                     reinterpret_cast<const unsigned char *>(bytes->data()), bytes->size()) == 1 &&
      count == signature.size();
  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(key);
  Require(ok, "Actual signature failed");
  return {*bytes, signature, std::move(artifact)};
}
void NativeFixture(const fs::path &root, const fs::path &module) {
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root, "Signed manager fixture"), "Project fixture failed");
  runtime::World world;
  editor::SceneDocument scene(world, world.LoadScene("Main"));
  fs::create_directories(root / ".nexora/scenes");
  Require(scene.Create("Plugin subject") != 0 && scene.Save(root / ".nexora/scenes/Main.scene") &&
              workspace.SaveWorkspace(std::array<std::string, 1>{".nexora/scenes/Main.scene"}),
          "Scene fixture failed");
  const auto bytes = editor::PluginManager::EncodePackage(Package(Manifest(), Read(module)));
  Require(bytes.has_value(), "Container fixture invalid");
  Write(root / "input.nxpkg", *bytes);
}
#endif
void Run(int argc, char **argv) {
  using Manager = editor::PluginManager;
  const auto root = fs::temp_directory_path() /
                    ("nexora-plugin-manager-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Require(fs::create_directory(root), "Scratch failed");
  editor::test::TemporaryDirectoryCleanup cleanup(root);
  editor::ProjectWorkspace workspace;
  Require(workspace.Create(root / "project", "Plugin manager"), "Workspace failed");
  const auto descriptor = Read(workspace.Root() / "project.nexora");
  Manager manager;
  Require(manager.BindProject(workspace) && manager.Snapshot().empty() &&
              !fs::exists(workspace.Root() / ".nexora/extensions"),
          "Discovery wrote/autoenabled");
  const auto key = Hex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
  Require(manager.SetPublisher("known.vendor", key) && manager.SetAllowedPermissions(3),
          "Trust failed");
  editor::SignedExtensionPackage package{
      *editor::SignedExtensionHost::EncodeManifest(Manifest()), {}, {std::byte{1}}};
  const auto encoded = Manager::EncodePackage(package);
  Require(encoded && Manager::DecodePackage(*encoded), "Container round trip failed");
  for (std::size_t i = 0; i < encoded->size(); ++i)
    Require(!Manager::DecodePackage(std::span(*encoded).first(i)), "Truncated container accepted");
  auto malformed = *encoded;
  malformed.push_back(std::byte{});
  Require(!Manager::DecodePackage(malformed), "Trailing bytes accepted");
  malformed = *encoded;
  std::fill(malformed.begin() + 12, malformed.begin() + 20, std::byte{255});
  Require(!Manager::DecodePackage(malformed), "Artifact length overflow accepted");
  const auto input = root / "input.nxpkg";
  Write(input, *encoded);
  Require(!manager.Review(workspace, input) && !manager.ReviewSnapshot() &&
              !fs::exists(workspace.Root() / ".nexora/extensions"),
          "Invalid signature granted install");
  Require(!Manager::ReadPackageFile("relative.nxpkg"), "Relative input accepted");
  std::error_code ec;
  fs::create_symlink(input, root / "alias.nxpkg", ec);
  if (!ec)
    Require(!Manager::ReadPackageFile(root / "alias.nxpkg"), "Symlink accepted");
  ec.clear();
  fs::create_hard_link(input, root / "hard.nxpkg", ec);
  if (!ec) {
    Require(!Manager::ReadPackageFile(input), "Hard-linked input accepted");
    fs::remove(root / "hard.nxpkg");
  }
#if defined(NEXORA_TEST_OPENSSL)
  std::vector<std::byte> artifact{std::byte{1}, std::byte{2}};
#if defined(__linux__) && !defined(__ANDROID__)
  Require(argc == 4, "Native fixtures missing");
  artifact = Read(argv[1]);
#else
  static_cast<void>(argc);
  static_cast<void>(argv);
#endif
  const auto good = *Manager::EncodePackage(Package(Manifest(), artifact));
  Write(input, good);
  Require(manager.Review(workspace, input), "Actual verified review failed");
  const auto review = *manager.ReviewSnapshot();
  auto copy = review;
  copy.manifest.id.clear();
  Require(manager.ReviewSnapshot()->manifest.id == "sample.plugin", "Review aliases caller");
  Write(input, malformed);
  Require(manager.Install(workspace, review.scope, review.configuration) &&
              manager.Snapshot().size() == 1,
          "Owned review reread mutable input");
  const auto installed = workspace.Root() / manager.Snapshot()[0].relative_path;
  Require(Read(installed) == good &&
              manager.Snapshot()[0].state == editor::ManagedExtensionState::Disabled,
          "Install changed verified bytes or autoenabled native code");
  const auto foreign = workspace.Root() / ".nexora/extensions/foreign.nxpkg";
  Write(foreign, malformed);
  Require(manager.Refresh(workspace) && manager.RejectedFiles() == 1 &&
              Read(foreign) == malformed && manager.Snapshot().size() == 1,
          "Corrupt discovery consumed foreign bytes or replaced valid observations");
  fs::remove(foreign);
  auto second_manifest = Manifest();
  second_manifest.version = "1.0.1";
  const auto second = *Manager::EncodePackage(Package(second_manifest, artifact));
  Write(input, second);
  Require(manager.Review(workspace, input), "Second version review failed");
  const auto second_review = *manager.ReviewSnapshot();
  const auto second_path = workspace.Root() / ".nexora/extensions/13-sample.plugin-1.0.1.nxpkg";
  const auto staging = fs::path(second_path.string() + ".tmp");
  Write(staging, malformed);
  Require(!manager.Install(workspace, second_review.scope, second_review.configuration) &&
              !fs::exists(second_path) && Read(staging) == malformed && Read(installed) == good,
          "Occupied staging was consumed or existing version overwritten");
  fs::remove(staging);
  Require(manager.Install(workspace, second_review.scope, second_review.configuration) &&
              manager.Remove(workspace, "sample.plugin", "1.0.1") && !fs::exists(second_path),
          "Verified second version install/remove failed");
  Write(input, good);
  Require(manager.Review(workspace, input) && manager.SetAllowedPermissions(1) &&
              !manager.Install(workspace, review.scope, review.configuration) &&
              !manager.ReviewSnapshot(),
          "Stale policy review remained authorized");
  Require(manager.SetAllowedPermissions(3), "Policy restoration failed");
  editor::ProjectWorkspace observer;
  Require(observer.Open(workspace.Root(), editor::ProjectAccess::ReadOnly),
          "Read-only observer failed");
  Manager readonly;
  Require(readonly.BindProject(observer) && readonly.SetPublisher("known.vendor", key) &&
              readonly.SetAllowedPermissions(3) && readonly.Snapshot().size() == 1 &&
              readonly.Review(observer, input),
          "Read-only inspection unavailable");
  const auto roreview = *readonly.ReviewSnapshot();
  Require(!readonly.Install(observer, roreview.scope, roreview.configuration) &&
              !readonly.Enable(observer, "sample.plugin", "1.0.0") &&
              !readonly.Remove(observer, "sample.plugin", "1.0.0") && Read(installed) == good,
          "Read-only action wrote/loaded/deleted");
  auto tampered = good;
  tampered.back() ^= std::byte{1};
  Write(installed, tampered);
  Require(!manager.Enable(workspace, "sample.plugin", "1.0.0") &&
              !manager.FindService("signed.A") &&
              !manager.Remove(workspace, "sample.plugin", "1.0.0") && Read(installed) == tampered,
          "Tampered artifact executed or was deleted");
  Write(installed, good);
  Require(manager.Refresh(workspace), "Fresh package discovery failed");
#if defined(__linux__) && !defined(__ANDROID__)
  Require(manager.Enable(workspace, "sample.plugin", "1.0.0") && manager.FindService("signed.A") &&
              manager.Snapshot()[0].reported_abi == foundation::kEngineAbiVersion &&
              manager.Snapshot()[0].cooperative,
          "Actual native registration/diagnostics failed");
  Require(!manager.Remove(workspace, "sample.plugin", "1.0.0"), "Loaded package removed");
  Require(manager.Review(workspace, input), "Current native package review failed");
  const auto recovery = workspace.Root() / ".nexora/workspace.recovery";
  Require(fs::create_directory(recovery) && manager.BindProject(workspace) &&
              !manager.FindService("signed.A") && !manager.ReviewSnapshot() &&
              !manager.Enable(workspace, "sample.plugin", "1.0.0") && fs::is_directory(recovery),
          "Recovery did not revoke native services/review or consumed occupied evidence");
  fs::remove(recovery);
  Require(workspace.SaveWorkspace(std::span<const std::string>{}) &&
              manager.Enable(workspace, "sample.plugin", "1.0.0"),
          "Resolved recovery did not permit explicit enable");
  const auto workspace_path = workspace.Root() / ".nexora/workspace";
  const auto workspace_bytes = Read(workspace_path);
  const auto workspace_time = fs::last_write_time(workspace_path);
  Write(workspace_path, malformed);
  fs::last_write_time(workspace_path, workspace_time + std::chrono::seconds(2));
  Require(workspace.HasExternalChange() && manager.BindProject(workspace) &&
              !manager.FindService("signed.A") && !manager.Review(workspace, input) &&
              !manager.Enable(workspace, "sample.plugin", "1.0.0") &&
              !manager.Remove(workspace, "sample.plugin", "1.0.0") &&
              Read(workspace_path) == malformed,
          "External workspace change retained native authority or replaced foreign bytes");
  Write(workspace_path, workspace_bytes);
  fs::last_write_time(workspace_path, workspace_time);
  Require(!workspace.HasExternalChange() && manager.Enable(workspace, "sample.plugin", "1.0.0"),
          "Explicit re-enable after exact workspace restoration failed");
  Require(manager.RemovePublisher("known.vendor") && !manager.FindService("signed.A") &&
              manager.Snapshot()[0].state == editor::ManagedExtensionState::Disabled &&
              !manager.Enable(workspace, "sample.plugin", "1.0.0"),
          "Revoke retained services/enable authority");
  Require(manager.SetPublisher("known.vendor", key) &&
              manager.Enable(workspace, "sample.plugin", "1.0.0") &&
              manager.Disable("sample.plugin", "1.0.0") && !manager.FindService("signed.A") &&
              manager.Remove(workspace, "sample.plugin", "1.0.0") && !fs::exists(installed),
          "Actual disable/remove failed");
  const auto incompatible =
      *Manager::EncodePackage(Package(Manifest("dishonest.plugin"), Read(argv[3])));
  Write(input, incompatible);
  Require(manager.Review(workspace, input), "Signed incompatible package review failed");
  const auto incompatible_review = *manager.ReviewSnapshot();
  Require(
      manager.Install(workspace, incompatible_review.scope, incompatible_review.configuration) &&
          !manager.Enable(workspace, "dishonest.plugin", "1.0.0") &&
          manager.Snapshot()[0].state == editor::ManagedExtensionState::Rejected &&
          manager.Snapshot()[0].native_error == runtime::PluginLoadError::AbiMismatch &&
          manager.Snapshot()[0].reported_abi == foundation::kEngineAbiVersion + 1 &&
          !manager.FindService("signed.A") &&
          manager.Remove(workspace, "dishonest.plugin", "1.0.0"),
      "Actual native ABI failure lost diagnostics/state or retained services");
  const auto legacy = *Manager::EncodePackage(Package(Manifest("legacy.plugin"), Read(argv[2])));
  Write(input, legacy);
  Require(manager.Review(workspace, input), "Legacy signed review failed");
  const auto legacyreview = *manager.ReviewSnapshot();
  Require(manager.Install(workspace, legacyreview.scope, legacyreview.configuration) &&
              manager.Enable(workspace, "legacy.plugin", "1.0.0") &&
              manager.Disable("legacy.plugin", "1.0.0") && manager.RestartRequired() &&
              !manager.FindService("signed.A") &&
              manager.Snapshot()[0].lifecycle_error ==
                  runtime::PluginLifecycleError::LegacyNeedsRestart &&
              !manager.Remove(workspace, "legacy.plugin", "1.0.0"),
          "Legacy mapping was force unloaded/deleted");
#else
  Require(!manager.Enable(workspace, "sample.plugin", "1.0.0"),
          "Unavailable native backend accepted");
#endif
#else
  static_cast<void>(argc);
  static_cast<void>(argv);
  Require(!manager.Review(workspace, input) && manager.Snapshot().empty(),
          "Missing provider did not fail closed");
#endif
  const auto scope = manager.Scope();
  manager.Detach();
  Require(manager.Scope() != scope && manager.Root().empty() && !manager.ReviewSnapshot() &&
              !manager.FindService("signed.A") &&
              Read(workspace.Root() / "project.nexora") == descriptor,
          "Detach retained scope/service or changed source descriptor");
}
} // namespace
int main(int argc, char **argv) {
  try {
#if defined(NEXORA_TEST_OPENSSL)
    if (argc == 4 && std::string_view(argv[1]) == "--write-native-fixture") {
      NativeFixture(
          fs::path{std::u8string(argv[2], argv[2] + std::char_traits<char>::length(argv[2]))},
          fs::path{std::u8string(argv[3], argv[3] + std::char_traits<char>::length(argv[3]))});
      return 0;
    }
#endif
    Run(argc, argv);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
