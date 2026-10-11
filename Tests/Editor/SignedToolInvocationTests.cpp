#include "NativeToolFixtureBridge.h"
#include "Nexora/Editor/MaterialImport.h"
#include "Nexora/Editor/ScalarMaterialTool.h"
#include "Nexora/Editor/SignedExtensionHost.h"
#include "Nexora/Foundation/BuildInfo.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#if defined(__linux__) && !defined(__ANDROID__) && defined(NEXORA_TEST_OPENSSL)
#include <openssl/evp.h>
#endif

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}
std::vector<std::byte> Bytes(std::string_view text) {
  const auto view = std::as_bytes(std::span(text.data(), text.size()));
  return {view.begin(), view.end()};
}
#if defined(__linux__) && !defined(__ANDROID__) && defined(NEXORA_TEST_OPENSSL)
std::vector<std::byte> Hex(std::string_view text) {
  const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  std::vector<std::byte> result;
  for (std::size_t i = 0; i < text.size(); i += 2)
    result.push_back(static_cast<std::byte>((digit(text[i]) << 4) | digit(text[i + 1])));
  return result;
}
std::string Target() {
#if defined(__aarch64__)
  return "linux-aarch64";
#else
  return "linux-x86_64";
#endif
}
editor::SignedExtensionPackage Package(const std::filesystem::path &path, std::string id,
                                       std::string dependency) {
  const auto size = std::filesystem::file_size(path);
  Require(size && size <= cryptography::kMaximumMessageBytes,
          "Actual tool artifact budget invalid");
  std::vector<std::byte> artifact(size);
  std::ifstream file(path, std::ios::binary);
  file.read(reinterpret_cast<char *>(artifact.data()), artifact.size());
  Require(file.good() && file.peek() == std::char_traits<char>::eof(),
          "Actual tool artifact read failed");
  file.close();
  const auto hash = cryptography::Sha256(artifact);
  Require(hash.has_value(), "Actual tool artifact SHA-256 unavailable");
  editor::ExtensionManifest manifest{std::move(id),
                                     "0.1.0",
                                     "fixture.vendor",
                                     Target(),
                                     foundation::kEngineAbiVersion,
                                     1,
                                     {std::move(dependency)},
                                     *hash};
  const auto encoded = editor::SignedExtensionHost::EncodeManifest(manifest);
  Require(encoded.has_value(), "Actual tool manifest encode failed");
  // RFC8032 TEST1 public fixture seed, never a user signing credential.
  const auto seed = Hex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
  auto *key = EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, nullptr, reinterpret_cast<const unsigned char *>(seed.data()), seed.size());
  auto *context = EVP_MD_CTX_new();
  std::array<std::byte, 64> signature{};
  std::size_t length = signature.size();
  const bool ok = key && context &&
                  EVP_DigestSignInit(context, nullptr, nullptr, nullptr, key) == 1 &&
                  EVP_DigestSign(context, reinterpret_cast<unsigned char *>(signature.data()),
                                 &length, reinterpret_cast<const unsigned char *>(encoded->data()),
                                 encoded->size()) == 1 &&
                  length == signature.size();
  EVP_MD_CTX_free(context);
  EVP_PKEY_free(key);
  Require(ok, "Actual tool signature failed");
  return {*encoded, signature, std::move(artifact)};
}
#endif
} // namespace
int main(int argc, char **argv) {
  using namespace nexora;
  using namespace editor;
  Require(argc == 3, "Actual production tool/reentrant fixture paths required");
  ExtensionTrust trust;
  runtime::ServiceRegistry services; // Registry/trust outlive the signed host and every call.
  NativeToolInvoker invoker, second_invoker;
  SignedExtensionHost host(trust, foundation::kEngineAbiVersion);
  const auto source = Bytes("NEXORA_MATERIAL 1\nbase_color .2 .3 .4\nmetallic .5\nroughness .6\n"
                            "occlusion 1\nemission 0 0 0\n");
  auto outcome = host.InvokeTool(invoker, services, 1, kScalarMaterialToolService,
                                 NativeToolOperation::Inspect, source);
  Require(outcome.state == NativeToolState::Unavailable && !outcome.callback_result,
          "Unsigned/missing admission reached native code");
  std::thread other([&] {
    outcome = host.InvokeTool(invoker, services, 1, kScalarMaterialToolService,
                              NativeToolOperation::Inspect, source);
  });
  other.join();
  Require(outcome.state == NativeToolState::WrongThread && !outcome.callback_result &&
              host.Snapshot().empty(),
          "Foreign thread accessed signed admission state");
#if defined(__linux__) && !defined(__ANDROID__) && defined(NEXORA_TEST_OPENSSL)
  const auto public_key = Hex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
  Require(trust.SetPublisher("fixture.vendor", public_key), "Actual fixture publisher rejected");
  ExtensionAdmissionPolicy policy{
      foundation::kEngineAbiVersion, 1, Target(), {"Editor", "Foundation"}};
  Require(host.SetPolicy(policy), "Actual tool policy rejected");
  const auto package = Package(argv[1], "org.nexora.editor.scalar-material", "Editor");
  const auto prepared = host.Prepare(package);
  Require(prepared.has_value(), "Actual signed material preparation failed");
  const auto admission = host.Load(*prepared, &services);
  Require(admission.error == ExtensionAdmissionError::None && admission.native.loaded &&
              admission.native.cooperative,
          "Actual sealed material image failed native admission");
  outcome = host.InvokeTool(invoker, services, admission.native.id, kScalarMaterialToolService,
                            NativeToolOperation::Inspect, source);
  Require(outcome.state == NativeToolState::Success && outcome.callback_result == 0,
          "Actual signed production backend did not execute");
  const auto retained = outcome;
  const auto parsed =
      ImportMaterial({reinterpret_cast<const char *>(outcome.bytes.data()), outcome.bytes.size()});
  Require(parsed.material && parsed.material->base_color[0] == .2F &&
              parsed.material->roughness == .6F,
          "Actual signed backend result did not use production material semantics");
  const runtime::ServiceRegistry copied = services;
  outcome = host.InvokeTool(invoker, copied, admission.native.id, kScalarMaterialToolService,
                            NativeToolOperation::Serialize, retained.bytes);
  Require(outcome.state == NativeToolState::Success && outcome.bytes == retained.bytes,
          "Signed copied registry or canonical serialization failed");
  {
    runtime::ServiceRegistry foreign_services;
    SignedExtensionHost foreign(trust, foundation::kEngineAbiVersion);
    Require(foreign.SetPolicy(policy), "Foreign actual signed policy failed");
    const auto foreign_prepared = foreign.Prepare(package);
    Require(foreign_prepared.has_value(), "Foreign actual signed preparation failed");
    const auto loaded = foreign.Load(*foreign_prepared, &foreign_services);
    Require(loaded.native.loaded && loaded.native.id == admission.native.id,
            "Actual same-ID signed fixture failed");
    outcome = host.InvokeTool(invoker, foreign_services, admission.native.id,
                              kScalarMaterialToolService, NativeToolOperation::Inspect, source);
    Require(outcome.state == NativeToolState::Unavailable && !outcome.callback_result,
            "Another signed host's numeric ID/provider reached the callback");
    Require(foreign.RequestUnload(loaded.native.id) == runtime::PluginState::Unloaded,
            "Foreign actual signed fixture failed unload");
  }
  NexoraEditorToolServiceV1 manual{};
  Require(services.Unregister(kScalarMaterialToolService) &&
              services.Register(std::string(kScalarMaterialToolService), &manual),
          "Signed manual replacement setup failed");
  outcome = host.InvokeTool(invoker, services, admission.native.id, kScalarMaterialToolService,
                            NativeToolOperation::Inspect, source);
  Require(outcome.state == NativeToolState::Unavailable && !outcome.callback_result,
          "Manual same-name provider acquired signed tool authority");
  Require(trust.RemovePublisher("fixture.vendor"), "Actual trust revocation failed");
  Require(host.Snapshot().front().state == runtime::PluginState::Loaded,
          "Trust fixture unexpectedly polled before invocation");
  outcome = host.InvokeTool(invoker, copied, admission.native.id, kScalarMaterialToolService,
                            NativeToolOperation::Inspect, source);
  Require(outcome.state == NativeToolState::Unavailable && !outcome.callback_result &&
              host.Snapshot().front().state == runtime::PluginState::Loaded,
          "Trust drift reached a callback or invocation mutated lifecycle");
  Require(trust.SetPublisher("fixture.vendor", public_key), "Actual trust restoration failed");
  outcome = host.InvokeTool(invoker, copied, admission.native.id, kScalarMaterialToolService,
                            NativeToolOperation::Inspect, source);
  Require(outcome.state == NativeToolState::Unavailable,
          "Restored trust revived an old signed observation");
  host.PollShutdown();
  runtime::ServiceRegistry renewed_services;
  const auto renewed_prepared = host.Prepare(package);
  Require(renewed_prepared.has_value(), "Fresh signed observation failed");
  const auto renewed = host.Load(*renewed_prepared, &renewed_services);
  Require(renewed.native.loaded && renewed.native.id != admission.native.id,
          "Fresh signed material reload failed");
  outcome = host.InvokeTool(invoker, renewed_services, renewed.native.id,
                            kScalarMaterialToolService, NativeToolOperation::Inspect, source);
  Require(outcome.state == NativeToolState::Success && outcome.bytes == retained.bytes,
          "Fresh signed material call failed");
  auto denied = policy;
  denied.allowed_permissions = 0;
  Require(host.SetPolicy(denied), "Actual policy revocation failed");
  outcome = host.InvokeTool(invoker, renewed_services, renewed.native.id,
                            kScalarMaterialToolService, NativeToolOperation::Inspect, source);
  Require(outcome.state == NativeToolState::Unavailable && !outcome.callback_result,
          "Policy drift reached a signed callback");
  Require(host.SetPolicy(policy), "Actual policy restoration failed");
  runtime::ServiceRegistry recursive_services;
  const auto recursive_prepared =
      host.Prepare(Package(argv[2], "org.nexora.native-reentrant", "Foundation"));
  Require(recursive_prepared.has_value(), "Actual signed recursive preparation failed");
  const auto recursive = host.Load(*recursive_prepared, &recursive_services);
  Require(recursive.native.loaded, "Actual signed recursive native load failed");
  auto *bridge = static_cast<NativeToolFixtureBridge *>(recursive_services.Find("fixture.bridge"));
  Require(bridge != nullptr, "Actual signed recursive bridge missing");
  struct Context final {
    SignedExtensionHost &host;
    NativeToolInvoker &invoker;
    runtime::ServiceRegistry &services;
    std::uint64_t id;
    NativeToolOutcome nested;
  } context{host, second_invoker, recursive_services, recursive.native.id, {}};
  bridge->context = &context;
  bridge->call = [](void *value) {
    auto &c = *static_cast<Context *>(value);
    c.nested = c.host.InvokeTool(c.invoker, c.services, c.id, "fixture.tool",
                                 NativeToolOperation::Inspect, {});
  };
  outcome = host.InvokeTool(invoker, recursive_services, recursive.native.id, "fixture.tool",
                            NativeToolOperation::Inspect, source);
  bridge->call = nullptr;
  bridge->context = nullptr;
  bridge = nullptr;
  Require(outcome.state == NativeToolState::Success && outcome.bytes == source &&
              context.nested.state == NativeToolState::Reentrant &&
              !context.nested.callback_result &&
              *static_cast<const std::uint32_t *>(recursive_services.Find("fixture.calls")) == 1,
          "Actual signed native callback reentered host admission state");
  Require(host.RequestUnload(recursive.native.id) == runtime::PluginState::Unloaded,
          "Actual signed recursive fixture did not unload");
  outcome = host.InvokeTool(invoker, copied, admission.native.id, kScalarMaterialToolService,
                            NativeToolOperation::Inspect, source);
  Require(outcome.state == NativeToolState::Unavailable &&
              retained.bytes == Bytes(*ExportMaterial(*parsed.material).source),
          "Retired signed admission revived or owning material output expired");
  std::cout << "Actual signed sealed-image material and recursive callbacks verified.\n";
#else
  static_cast<void>(argv);
  std::cout << "Signed native image backend unavailable; missing/context rejection verified.\n";
#endif
}
