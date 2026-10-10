#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "Nexora/Editor/SignedExtensionHost.h"
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
#if defined(__linux__) && !defined(__ANDROID__)
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
using namespace nexora;
using Error = editor::ExtensionAdmissionError;
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
#if defined(__linux__) && !defined(__ANDROID__) && defined(NEXORA_TEST_OPENSSL)
std::vector<std::byte> Read(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  Require(static_cast<bool>(input), "Fixture read failed");
  const std::string text{std::istreambuf_iterator<char>{input}, {}};
  std::vector<std::byte> bytes;
  for (const unsigned char c : text)
    bytes.push_back(static_cast<std::byte>(c));
  return bytes;
}
std::string Events(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>{input}, {}};
}
#endif
std::string Target() {
#if defined(__linux__) && defined(__aarch64__)
  return "linux-aarch64";
#else
  return "linux-x86_64";
#endif
}
editor::ExtensionAdmissionPolicy Policy() {
  return {foundation::kEngineAbiVersion, 3, Target(), {"core", "reflection"}};
}
editor::ExtensionManifest Manifest() {
  editor::ExtensionManifest m{
      "sample.plugin", "1.0.0", "known.vendor", Target(), foundation::kEngineAbiVersion, 3,
      {"core"},        {}};
  m.artifact_digest[0] = std::byte{1};
  return m;
}
#if defined(NEXORA_TEST_OPENSSL)
std::array<std::byte, 64> Sign(std::span<const std::byte> message) {
  // RFC 8032's publicly published TEST 1 seed; never a user's signing credential.
  const auto seed = Hex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
  auto *key = EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, nullptr, reinterpret_cast<const unsigned char *>(seed.data()), seed.size());
  auto *context = EVP_MD_CTX_new();
  std::array<std::byte, 64> signature{};
  std::size_t length = signature.size();
  const bool ok = key && context &&
                  EVP_DigestSignInit(context, nullptr, nullptr, nullptr, key) == 1 &&
                  EVP_DigestSign(context, reinterpret_cast<unsigned char *>(signature.data()),
                                 &length, reinterpret_cast<const unsigned char *>(message.data()),
                                 message.size()) == 1 &&
                  length == signature.size();
  EVP_MD_CTX_free(context);
  EVP_PKEY_free(key);
  Require(ok, "Actual fixture signature failed");
  return signature;
}
editor::SignedExtensionPackage Package(editor::ExtensionManifest manifest,
                                       std::vector<std::byte> artifact) {
  const auto hash = cryptography::Sha256(artifact);
  Require(hash.has_value(), "Actual artifact digest unavailable");
  manifest.artifact_digest = *hash;
  const auto encoded = editor::SignedExtensionHost::EncodeManifest(manifest);
  Require(encoded.has_value(), "Fixture encoding failed");
  return {*encoded, Sign(*encoded), std::move(artifact)};
}
#endif
void Codec() {
  auto m = Manifest();
  const auto encoded = editor::SignedExtensionHost::EncodeManifest(m);
  Require(encoded && editor::SignedExtensionHost::DecodeManifest(*encoded) == m,
          "Canonical manifest round trip failed");
  for (std::size_t count = 0; count < encoded->size(); ++count)
    Require(!editor::SignedExtensionHost::DecodeManifest(std::span(*encoded).first(count)),
            "Truncated manifest accepted");
  auto bytes = *encoded;
  bytes.push_back(std::byte{});
  Require(!editor::SignedExtensionHost::DecodeManifest(bytes), "Trailing bytes accepted");
  bytes = *encoded;
  bytes[6] = std::byte{'9'};
  Require(!editor::SignedExtensionHost::DecodeManifest(bytes), "Unknown version accepted");
  bytes = *encoded;
  std::fill(bytes.begin() + 16, bytes.begin() + 20, std::byte{255});
  Require(!editor::SignedExtensionHost::DecodeManifest(bytes), "Overflow length accepted");
  bytes.assign(editor::SignedExtensionHost::kMaximumManifestBytes + 1, std::byte{});
  Require(!editor::SignedExtensionHost::DecodeManifest(bytes), "Manifest budget ignored");
  for (const auto *id : {"", "../plugin", "bad/name", "bad\nname"}) {
    auto invalid = m;
    invalid.id = id;
    Require(!editor::SignedExtensionHost::EncodeManifest(invalid), "Unsafe identity accepted");
  }
  m.dependencies = {"core", "core"};
  Require(!editor::SignedExtensionHost::EncodeManifest(m), "Duplicate dependency accepted");
  m.dependencies = {"reflection", "core"};
  Require(!editor::SignedExtensionHost::EncodeManifest(m), "Unsorted dependency accepted");
  m = Manifest();
  m.permissions = 0x80000000U;
  Require(!editor::SignedExtensionHost::EncodeManifest(m), "Unknown permissions accepted");
}
void Run(int argc, char **argv) {
  Codec();
#if defined(__linux__) && !defined(__ANDROID__) && defined(NEXORA_TEST_OPENSSL)
  const auto scratch =
      std::filesystem::temp_directory_path() /
      ("nexora-signed-admission-" + std::to_string(getpid()) + "-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Require(std::filesystem::create_directory(scratch), "Fixture scratch creation failed");
  editor::test::TemporaryDirectoryCleanup cleanup(scratch);
  const auto events = scratch / "events";
  struct Environment final {
    std::optional<std::string> previous;
    explicit Environment(const std::filesystem::path &path) {
      if (const auto *value = std::getenv("NEXORA_SIGNED_FIXTURE_EVENTS"))
        previous = value;
      Require(setenv("NEXORA_SIGNED_FIXTURE_EVENTS", path.c_str(), 1) == 0, "Fixture env failed");
    }
    ~Environment() {
      if (previous)
        static_cast<void>(setenv("NEXORA_SIGNED_FIXTURE_EVENTS", previous->c_str(), 1));
      else
        static_cast<void>(unsetenv("NEXORA_SIGNED_FIXTURE_EVENTS"));
    }
  } environment(events);
#endif
  const auto key = Hex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
  editor::ExtensionTrust trust;
  Require(trust.SetPublisher("known.vendor", key), "Trusted public fixture key rejected");
#if defined(__linux__) && !defined(__ANDROID__) && defined(NEXORA_TEST_OPENSSL)
  runtime::ServiceRegistry services;
#endif
  editor::SignedExtensionHost host(trust, foundation::kEngineAbiVersion);
  Require(host.SetPolicy(Policy()), "Host policy rejected");
  auto invalid_policy = Policy();
  ++invalid_policy.engine_abi;
  Require(!host.SetPolicy(invalid_policy), "Mismatched host ABI policy accepted");
  Require(host.Load(editor::PreparedExtension{}).error == Error::StaleAdmission,
          "Default prepared observation authorized native loading");
#if !defined(NEXORA_TEST_OPENSSL)
  static_cast<void>(argc);
  static_cast<void>(argv);
  editor::SignedExtensionPackage unavailable{
      *editor::SignedExtensionHost::EncodeManifest(Manifest()), {}, {std::byte{1}}};
  Error error{};
  Require(!host.Prepare(std::move(unavailable), &error) && error == Error::BackendUnavailable &&
              host.Snapshot().empty(),
          "Unavailable provider did not fail closed");
#else
  std::vector<std::byte> artifact{std::byte{1}, std::byte{2}};
#if defined(__linux__) && !defined(__ANDROID__)
  Require(argc == 4, "Actual native fixtures missing");
  artifact = Read(argv[1]);
#else
  static_cast<void>(argc);
  static_cast<void>(argv);
#endif
  const auto original = Package(Manifest(), artifact);
  Error error{};
  const auto reject = [&](editor::SignedExtensionPackage p, Error expected) {
    Require(!host.Prepare(std::move(p), &error) && error == expected && host.Snapshot().empty(),
            "Invalid package reached native loader/history");
  };
  auto changed = original;
  changed.signature[0] ^= std::byte{1};
  reject(std::move(changed), Error::UntrustedOrInvalidSignature);
  changed = original;
  changed.artifact.back() ^= std::byte{1};
  reject(std::move(changed), Error::ArtifactMismatch);
  auto m = Manifest();
  m.publisher = "unknown.vendor";
  reject(Package(m, artifact), Error::UntrustedOrInvalidSignature);
  m = Manifest();
  ++m.engine_abi;
  reject(Package(m, artifact), Error::PolicyRejected);
  m = Manifest();
  m.permissions = 4;
  reject(Package(m, artifact), Error::PolicyRejected);
  m = Manifest();
  m.target = "other-platform";
  reject(Package(m, artifact), Error::PolicyRejected);
  m = Manifest();
  m.dependencies = {"missing"};
  reject(Package(m, artifact), Error::PolicyRejected);
  changed = original;
  changed.artifact.clear();
  reject(std::move(changed), Error::BudgetExceeded);
  changed = original;
  changed.artifact.resize(cryptography::kMaximumMessageBytes + 1);
  reject(std::move(changed), Error::BudgetExceeded);
  auto prepared = host.Prepare(original, &error);
  Require(prepared && error == Error::None && prepared->Manifest().id == "sample.plugin",
          "Real signed package not prepared");
  Require(host.SetPolicy(Policy()), "Identical policy rejected");
  auto rotated = key;
  rotated[3] ^= std::byte{1};
  Require(trust.SetPublisher("known.vendor", rotated) &&
              host.Load(*prepared).error == Error::StaleAdmission && host.Snapshot().empty(),
          "Rotated key retained old prepared authorization");
  Require(trust.SetPublisher("known.vendor", key), "Fixture key restore failed");
  prepared = host.Prepare(original);
  auto narrowed = Policy();
  narrowed.allowed_permissions = 1;
  Require(host.SetPolicy(narrowed) && host.Load(*prepared).error == Error::StaleAdmission,
          "Changed policy retained old prepared authorization");
  Require(host.SetPolicy(Policy()), "Policy restore failed");
  prepared = host.Prepare(original);
#if defined(__linux__) && !defined(__ANDROID__)
  changed = original;
  changed.signature[8] ^= std::byte{1};
  reject(std::move(changed), Error::UntrustedOrInvalidSignature);
  Require(Events(events).empty(), "Rejected package executed a native constructor");
  auto copy = original;
  auto owned = host.Prepare(copy);
  copy.artifact.assign(1, std::byte{});
  copy.manifest.assign(1, std::byte{});
  const auto loaded = host.Load(*owned, &services);
  Require(loaded.error == Error::None && loaded.native.loaded && loaded.native.cooperative &&
              services.Find("signed.A") &&
              std::string(static_cast<char *>(services.Find("signed.A"))) == "A" &&
              Events(events).find("1 initialize") != std::string::npos,
          "Actual verified owning native image did not initialize/register");
  Require(host.Load(*owned).error == Error::DuplicateIdentity, "Duplicate live identity loaded");
  const auto snapshot = host.Snapshot();
  Require(snapshot.size() == 1 && snapshot[0].library_path.starts_with("/proc/self/fd/"),
          "Mutable project path reached native loader");
  const int fd = std::stoi(snapshot[0].library_path.substr(14));
  constexpr int seals = F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
  const auto actual_seals = fcntl(fd, F_GET_SEALS);
  const char byte = 'x';
  Require(actual_seals >= 0 && (actual_seals & seals) == seals && pwrite(fd, &byte, 1, 0) == -1 &&
              errno == EPERM && ftruncate(fd, 0) == -1 && errno == EPERM,
          "Admitted native image was mutable");
  m = Manifest();
  m.id = "second.plugin";
  const auto second = host.Prepare(Package(m, Read(argv[2])));
  const auto other = host.Load(*second, &services);
  Require(other.native.loaded && services.Find("signed.B") &&
              std::string(static_cast<char *>(services.Find("signed.B"))) == "B" &&
              host.Snapshot()[1].library_path != snapshot[0].library_path,
          "Live descriptor reuse loaded the wrong native image");
  Require(host.RequestUnload(loaded.native.id) == runtime::PluginState::Unloaded &&
              !services.Find("signed.A") && Events(events).find("1 unload") != std::string::npos &&
              fcntl(fd, F_GETFD) == -1 && errno == EBADF,
          "Cooperative unload retained visibility/sealed descriptor");
  Require(trust.RemovePublisher("known.vendor"), "Actual trust revocation failed");
  host.PollShutdown();
  Require(!services.Find("signed.B") && host.Snapshot()[1].state == runtime::PluginState::Unloaded,
          "Revocation poll retained live services");
  Require(trust.SetPublisher("known.vendor", key), "Restore trust failed");
  prepared = host.Prepare(original);
  const auto reloaded = host.Load(*prepared, &services);
  Require(reloaded.native.loaded && services.Find("signed.A"),
          "Unloaded identity could not reload");
  Require(host.SetPolicy(narrowed) && !services.Find("signed.A") &&
              host.Snapshot().back().state == runtime::PluginState::Unloaded,
          "Changed policy retained active native services");
  Require(host.SetPolicy(Policy()), "Final policy restore failed");
  m = Manifest();
  m.id = "dishonest.abi";
  const auto dishonest = host.Prepare(Package(m, Read(argv[3])));
  const auto mismatch = host.Load(*dishonest, &services);
  Require(mismatch.error == Error::NativeLoadFailed &&
              mismatch.native.error == runtime::PluginLoadError::AbiMismatch &&
              Events(events).find("3 initialize") != std::string::npos &&
              !services.Find("signed.A"),
          "Post-admission native ABI diagnostic was falsely treated as pre-execution inspection");
  editor::SignedExtensionHost bounded(trust, foundation::kEngineAbiVersion);
  Require(bounded.SetPolicy(Policy()), "Budget host policy failed");
  const auto reusable = bounded.Prepare(original);
  for (std::size_t i = 0; i < runtime::PluginHost::kMaximumPlugins; ++i) {
    const auto result = bounded.Load(*reusable);
    Require(result.native.loaded &&
                bounded.RequestUnload(result.native.id) == runtime::PluginState::Unloaded,
            "Lifetime budget fixture failed to unload");
  }
  const auto before = Events(events);
  Require(bounded.Load(*reusable).error == Error::BudgetExceeded && Events(events) == before,
          "Lifetime admissions exceeded budget or initialized rejected code");
#else
  Require(host.Load(*prepared).error == Error::BackendUnavailable && host.Snapshot().empty(),
          "Unsupported native staging backend did not reject");
#endif
#endif
}
} // namespace
int main(int argc, char **argv) {
  try {
    Run(argc, argv);
    std::cout
        << "Canonical signed manifest, fail-closed admission and native image lifecycle passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
