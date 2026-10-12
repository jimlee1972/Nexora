#include "../EditorImGui/TemporaryDirectoryCleanup.h"
#include "NativeToolFixtureBridge.h"
#include "Nexora/Editor/MaterialImport.h"
#include "Nexora/Editor/PluginManager.h"
#include "Nexora/Editor/ScalarMaterialTool.h"
#include "Nexora/Foundation/BuildInfo.h"
#include <bit>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#if defined(__linux__) && !defined(__ANDROID__) && defined(NEXORA_TEST_OPENSSL)
#include <openssl/evp.h>
#endif
namespace {
using namespace nexora;
namespace fs = std::filesystem;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::vector<std::byte> Bytes(std::string_view text) {
  const auto view = std::as_bytes(std::span(text.data(), text.size()));
  return {view.begin(), view.end()};
}
#if defined(__linux__) && !defined(__ANDROID__) && defined(NEXORA_TEST_OPENSSL)
void Write(const fs::path &path, std::span<const std::byte> bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
  file.close();
  Require(!file.fail(), "Actual fixture write failed");
}
std::vector<std::byte> Read(const fs::path &path) {
  std::ifstream file(path, std::ios::binary);
  Require(bool(file), "Actual fixture read failed");
  const std::string text{std::istreambuf_iterator<char>{file}, {}};
  return Bytes(text);
}
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
void Run(int argc, char **argv) {
  using namespace editor;
  const auto root = fs::temp_directory_path() /
                    ("nexora-managed-tool-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Require(fs::create_directory(root), "Temporary root creation failed");
  test::TemporaryDirectoryCleanup cleanup(root);
  ProjectWorkspace workspace;
  Require(workspace.Create(root, "Managed tool fixture") && workspace.SaveWorkspace({}),
          "Actual project failed");
  PluginManager manager;
  Require(manager.BindProject(workspace), "Actual manager binding failed");
  const auto source = Bytes("NEXORA_MATERIAL 1\nbase_color .2 .3 .4\nmetallic .5\nroughness "
                            ".6\nocclusion 1\nemission 0 0 0\n");
  ManagedToolSelection missing;
  auto result = manager.InvokeTool(workspace, missing, kScalarMaterialToolService,
                                   NativeToolOperation::Inspect, source);
  Require(result.state == NativeToolState::Unavailable && !result.callback_result &&
              !manager.SelectTool("missing", "0.1.0"),
          "Missing selection reached callback");
  bool thread_selected{};
  std::thread thread([&] {
    result = manager.InvokeTool(workspace, missing, kScalarMaterialToolService,
                                NativeToolOperation::Inspect, source);
    thread_selected = manager.SelectTool("missing", "0.1.0").has_value();
  });
  thread.join();
  Require(result.state == NativeToolState::WrongThread && !result.callback_result &&
              !thread_selected,
          "Foreign-thread invocation inspected manager");
#if defined(__linux__) && !defined(__ANDROID__) && defined(NEXORA_TEST_OPENSSL)
  Require(argc == 3, "Actual production and recursive module paths missing");
  const auto key = Hex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
  Require(manager.SetPublisher("fixture.vendor", key) && manager.SetAllowedPermissions(1),
          "Actual manager trust policy failed");
  const std::string id = "org.nexora.editor.scalar-material", version = "0.1.0";
  const auto Select = [](const PluginManager &owner, std::string_view identity) {
    auto selected = owner.SelectTool(identity, "0.1.0");
    Require(selected.has_value(), "Enabled tool selection unavailable");
    return *selected;
  };
  const auto encoded = PluginManager::EncodePackage(Package(argv[1], id, "Editor"));
  Require(encoded.has_value(), "Actual production package encoding failed");
  const auto input = root / "material.nxpkg";
  Write(input, *encoded);
  Require(manager.Review(workspace, input), "Actual signed material review failed");
  const auto review = *manager.ReviewSnapshot();
  Require(manager.Install(workspace, review.scope, review.configuration) &&
              !manager.SelectTool(id, version) && manager.Enable(workspace, id, version),
          "Actual material install/explicit enable failed");
  auto selection = Select(manager, id);
  std::thread loaded_thread([&] {
    result = manager.InvokeTool(workspace, selection, kScalarMaterialToolService,
                                NativeToolOperation::Inspect, source);
    thread_selected = manager.SelectTool(id, version).has_value();
  });
  loaded_thread.join();
  Require(result.state == NativeToolState::WrongThread && !result.callback_result &&
              !thread_selected && manager.Snapshot()[0].state == ManagedExtensionState::Loaded,
          "Foreign thread inspected enabled manager state or invoked native code");
  const auto installed = root / manager.Snapshot()[0].relative_path;
  auto call = [&](const ManagedToolSelection &chosen,
                  NativeToolOperation operation = NativeToolOperation::Inspect,
                  std::span<const std::byte> bytes = {}) {
    return manager.InvokeTool(workspace, chosen, kScalarMaterialToolService, operation,
                              bytes.empty() ? std::span<const std::byte>(source) : bytes);
  };
  result = call(selection);
  Require(result.state == NativeToolState::Success && result.callback_result == 0,
          "Actual signed manager backend did not execute");
  const auto retained = result.bytes;
  result = call(selection, NativeToolOperation::Serialize, retained);
  Require(result.state == NativeToolState::Success && result.bytes == retained,
          "Managed serialization failed");
  auto edit = Bytes("NXM1");
  for (auto number :
       {std::uint32_t(ScalarMaterialLane::Roughness), std::bit_cast<std::uint32_t>(.75F)})
    for (unsigned i = 0; i < 4; ++i)
      edit.push_back(std::byte(number >> (i * 8)));
  edit.insert(edit.end(), source.begin(), source.end());
  result = call(selection, NativeToolOperation::Edit, edit);
  const auto parsed =
      ImportMaterial({reinterpret_cast<const char *>(result.bytes.data()), result.bytes.size()});
  Require(result.state == NativeToolState::Success && parsed.material &&
              parsed.material->roughness == .75F && parsed.material->base_color[0] == .2F &&
              Read(installed) == *encoded,
          "Managed edit changed unrelated values or installed bytes");
  result = call(selection, NativeToolOperation::Preview);
  Require(result.state == NativeToolState::Unavailable && !result.callback_result,
          "Unavailable native GPU preview executed");
  for (unsigned field = 0; field < 8; ++field) {
    auto stale = selection;
    switch (field) {
    case 0:
      ++stale.manager;
      break;
    case 1:
      ++stale.scope;
      break;
    case 2:
      ++stale.configuration;
      break;
    case 3:
      ++stale.native_id;
      break;
    case 4:
      stale.id = "foreign";
      break;
    case 5:
      stale.version = "0.2.0";
      break;
    case 6:
      stale.id.assign(129, 'x');
      break;
    default:
      stale.native_id = 0;
    }
    result = call(stale);
    Require(result.state == NativeToolState::Unavailable && !result.callback_result &&
                result.bytes.empty() && Read(installed) == *encoded,
            "Stale selection reached callback or mutated bytes");
  }
  {
    PluginManager foreign;
    Require(foreign.BindProject(workspace) && foreign.SetPublisher("fixture.vendor", key) &&
                foreign.SetAllowedPermissions(1) && foreign.Enable(workspace, id, version),
            "Foreign manager setup failed");
    const auto other = Select(foreign, id);
    Require(other.native_id == selection.native_id && other.scope == selection.scope &&
                other.configuration == selection.configuration &&
                other.manager != selection.manager,
            "Exact same-ID manager test precondition failed");
    result = call(other);
    Require(result.state == NativeToolState::Unavailable && !result.callback_result,
            "Foreign manager selection reached current provider");
    result = foreign.InvokeTool(workspace, selection, kScalarMaterialToolService,
                                NativeToolOperation::Inspect, source);
    Require(result.state == NativeToolState::Unavailable && !result.callback_result &&
                foreign.Disable(id, version),
            "Reverse foreign selection or unload failed");
  }
  ProjectWorkspace observer;
  Require(observer.Open(root, ProjectAccess::ReadOnly), "Read-only observer failed");
  result = manager.InvokeTool(observer, selection, kScalarMaterialToolService,
                              NativeToolOperation::Edit, edit);
  Require(result.state == NativeToolState::Unavailable && !result.callback_result &&
              Read(installed) == *encoded,
          "Read-only observer executed edit");
  const auto recovery = root / ".nexora/workspace.recovery";
  Require(fs::create_directory(recovery), "Recovery fixture creation failed");
  result = call(selection);
  Require(result.state == NativeToolState::Unavailable && !result.callback_result &&
              manager.Snapshot()[0].state == ManagedExtensionState::Loaded &&
              fs::is_directory(recovery),
          "Invocation bypassed unresolved recovery or changed lifecycle");
  fs::remove(recovery);
  const auto workspace_path = root / ".nexora/workspace";
  const auto original = Read(workspace_path);
  const auto time = fs::last_write_time(workspace_path);
  Write(workspace_path, Bytes("external change"));
  fs::last_write_time(workspace_path, time + std::chrono::seconds(2));
  result = call(selection);
  Require(result.state == NativeToolState::Unavailable && !result.callback_result &&
              Read(workspace_path) == Bytes("external change") &&
              manager.Snapshot()[0].state == ManagedExtensionState::Loaded,
          "External change granted callback or was overwritten");
  Write(workspace_path, original);
  fs::last_write_time(workspace_path, time);
  Require(call(selection).state == NativeToolState::Success, "Exact workspace restoration failed");
  Require(manager.Disable(id, version) && !manager.SelectTool(id, version),
          "Explicit disable failed");
  result = call(selection);
  const auto preserved =
      ImportMaterial({reinterpret_cast<const char *>(retained.data()), retained.size()});
  Require(preserved.material.has_value(), "Retained owning material became invalid");
  const auto exported = ExportMaterial(*preserved.material);
  Require(result.state == NativeToolState::Unavailable && !result.callback_result &&
              exported.source && retained == Bytes(*exported.source),
          "Disabled callback or owning result expiry");
  Require(manager.Enable(workspace, id, version), "Explicit material re-enable failed");
  const auto renewed = Select(manager, id);
  Require(renewed.native_id != selection.native_id &&
              call(selection).state == NativeToolState::Unavailable &&
              call(renewed).state == NativeToolState::Success,
          "Retired native ID revived");
  Require(manager.RemovePublisher("fixture.vendor"), "Actual manager trust revocation failed");
  result = call(renewed);
  Require(result.state == NativeToolState::Unavailable && !result.callback_result,
          "Trust revocation retained tool authority");
  Require(manager.SetPublisher("fixture.vendor", key) && manager.Enable(workspace, id, version),
          "Fresh manager trust re-enable failed");
  selection = Select(manager, id);
  Require(manager.SetAllowedPermissions(0), "Actual manager permission revocation failed");
  result = call(selection);
  Require(result.state == NativeToolState::Unavailable && !result.callback_result,
          "Policy revocation retained callback authority");
  Require(manager.SetAllowedPermissions(1) && manager.Enable(workspace, id, version),
          "Fresh policy re-enable failed");
  selection = Select(manager, id);
  manager.Detach();
  result = call(selection);
  Require(result.state == NativeToolState::Unavailable && !result.callback_result &&
              Read(installed) == *encoded,
          "Detached project retained tool authority");
  Require(manager.BindProject(workspace), "Project rebind failed");
  const auto recursive =
      PluginManager::EncodePackage(Package(argv[2], "fixture.recursive", "Foundation"));
  Require(recursive.has_value(), "Recursive package encoding failed");
  Write(root / "recursive.nxpkg", *recursive);
  Require(manager.SetAllowedPermissions(1) && manager.Review(workspace, root / "recursive.nxpkg"),
          "Recursive package review failed");
  const auto recursive_review = *manager.ReviewSnapshot();
  Require(manager.Install(workspace, recursive_review.scope, recursive_review.configuration) &&
              manager.Enable(workspace, "fixture.recursive", version),
          "Recursive explicit install/enable failed");
  const auto recursive_selection = Select(manager, "fixture.recursive");
  auto *bridge = static_cast<NativeToolFixtureBridge *>(manager.FindService("fixture.bridge"));
  Require(bridge != nullptr, "Actual recursive bridge unavailable");
  struct Context {
    PluginManager &manager;
    ProjectWorkspace &workspace;
    ManagedToolSelection selection;
    NativeToolOutcome nested;
    bool selected{};
  } context{manager, workspace, recursive_selection, {}};
  bridge->context = &context;
  bridge->call = [](void *value) {
    auto &c = *static_cast<Context *>(value);
    c.selected = c.manager.SelectTool("fixture.recursive", "0.1.0").has_value();
    c.nested = c.manager.InvokeTool(c.workspace, c.selection, "fixture.tool",
                                    NativeToolOperation::Inspect, {});
  };
  result = manager.InvokeTool(workspace, recursive_selection, "fixture.tool",
                              NativeToolOperation::Inspect, source);
  bridge->context = nullptr;
  bridge->call = nullptr;
  bridge = nullptr;
  Require(result.state == NativeToolState::Success && result.bytes == source &&
              context.nested.state == NativeToolState::Reentrant &&
              !context.nested.callback_result && !context.selected &&
              *static_cast<const std::uint32_t *>(manager.FindService("fixture.calls")) == 1,
          "Actual managed callback reentered manager state");
  Require(manager.Disable("fixture.recursive", version), "Recursive actual unload failed");
  std::cout << "Actual signed installed/enabled material and scoped managed calls verified.\n";
#else
  static_cast<void>(argc);
  static_cast<void>(argv);
  std::cout << "Native signed backend unavailable; missing selection and owner context verified.\n";
#endif
}
} // namespace
int main(int argc, char **argv) {
  try {
    Run(argc, argv);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
