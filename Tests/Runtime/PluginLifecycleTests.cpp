#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Runtime/EditorSdk.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace nexora::runtime;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
std::string Utf8(const std::filesystem::path &path) {
  const auto value = path.u8string();
  return {value.begin(), value.end()};
}
void SetEvents(const std::filesystem::path &path) {
  const auto text = Utf8(path);
#if defined(_WIN32)
  Require(_putenv_s("NEXORA_PLUGIN_EVENT_FILE", text.c_str()) == 0, "event env failed");
#else
  Require(setenv("NEXORA_PLUGIN_EVENT_FILE", text.c_str(), 1) == 0, "event env failed");
#endif
}
std::string Events(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}
bool HasEvent(const std::filesystem::path &path, int mode, std::string_view event) {
  const auto events = Events(path);
  const auto expected = std::to_string(mode) + " " + std::string(event) + '\n';
  std::size_t offset{};
  while ((offset = events.find(expected, offset)) != std::string::npos) {
    if (offset == 0 || events[offset - 1] == '\n')
      return true;
    ++offset;
  }
  return false;
}
constexpr auto kAbi = nexora::foundation::kEngineAbiVersion;

void VerifyCooperative(const std::string &path, const std::filesystem::path &journal) {
  PluginHost host(kAbi);
  ServiceRegistry registry;
  int manual = 42;
  Require(registry.Register("manual", &manual), "manual registry fixture failed");
  const auto loaded = host.Load(path, &registry);
  Require(loaded.loaded && loaded.cooperative && loaded.id && loaded.registered &&
              registry.Size() == 2 && registry.Find("fixture.marker"),
          "real coop admission failed");
  auto copied = registry;
  const auto observation = host.Snapshot();
  Require(observation.size() == 1 && observation[0].registered_services == 1 &&
              observation[0].library_path == path && observation[0].state == PluginState::Loaded,
          "owning plugin observation lost path/metadata");
  Require(host.RequestUnload(loaded.id) == PluginState::Unloaded && host.LoadedCount() == 0 &&
              !registry.Find("fixture.marker") && !copied.Find("fixture.marker") &&
              registry.Size() == 1 && copied.Size() == 1 && registry.Find("manual") == &manual &&
              HasEvent(journal, 1, "request") && HasEvent(journal, 1, "poll") &&
              HasEvent(journal, 1, "native-unload"),
          "actual shutdown/quiescence/native unload or copied-registry revocation failed");
  Require(observation[0].state == PluginState::Loaded &&
              host.Snapshot()[0].state == PluginState::Unloaded &&
              host.RequestUnload(loaded.id) == PluginState::Unloaded &&
              host.RequestUnload(UINT64_MAX) == PluginState::Missing &&
              registry.Register("fixture.marker", &manual) &&
              copied.Register("fixture.marker", &manual),
          "snapshots borrowed state or idempotent unload/revoked-name reuse failed");
  ServiceRegistry collision;
  Require(collision.Register("fixture.marker", &manual), "collision fixture failed");
  const auto rejected = host.Load(path, &collision);
  Require(!rejected.loaded && rejected.error == PluginLoadError::RegistrationRejected &&
              host.LoadedCount() == 0 && collision.Find("fixture.marker") == &manual &&
              collision.Size() == 1,
          "failed native registration displaced a caller service");
  // Host retains no registry pointer after registration; this scope may end first.
  {
    ServiceRegistry temporary;
    Require(host.Load(path, &temporary).loaded, "short-lived registry fixture failed");
  }
  host.UnloadAll();
  Require(host.LoadedCount() == 0, "host dereferenced a destroyed registry or lost quiescence");
}
void VerifyPending(const std::string &path, const std::filesystem::path &journal) {
  PluginHost host(kAbi);
  ServiceRegistry registry;
  const auto loaded = host.Load(path, &registry);
  Require(loaded.loaded && host.RequestUnload(loaded.id) == PluginState::ShutdownPending &&
              !registry.Find("fixture.marker") && host.LoadedCount() == 1 &&
              !HasEvent(journal, 2, "native-unload"),
          "native work unloaded before quiescence");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (host.LoadedCount() && std::chrono::steady_clock::now() < deadline) {
    host.PollShutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Require(!host.LoadedCount() && host.Snapshot()[0].state == PluginState::Unloaded &&
              HasEvent(journal, 2, "native-unload"),
          "real worker quiescence did not unload");
}
void VerifyFailure(const std::string &path, int mode, const std::filesystem::path &journal) {
  ServiceRegistry registry;
  PluginHost host(kAbi);
  const auto result = host.Load(path, &registry);
  if (mode == 3 || mode == 9 || mode == 10) {
    Require(result.loaded && host.RequestUnload(result.id) == PluginState::RestartRequired &&
                host.LoadedCount() == 1 && registry.Size() == 0 &&
                !registry.Find("fixture.marker") && !HasEvent(journal, mode, "native-unload"),
            "legacy/rejected shutdown was forced unloaded or left service visible");
    const auto row = host.Snapshot()[0];
    Require(row.lifecycle_error == (mode == 3   ? PluginLifecycleError::LegacyNeedsRestart
                                    : mode == 9 ? PluginLifecycleError::RequestFailed
                                                : PluginLifecycleError::QuiescenceFailed) &&
                row.lifecycle_result == (mode == 3   ? 0
                                         : mode == 9 ? -7
                                                     : -8),
            "copied lifecycle diagnostics were invented or lost");
    return;
  }
  Require(!result.loaded && !host.LoadedCount() && registry.Size() == 0 &&
              !registry.Find("fixture.marker"),
          "invalid plugin partially published a service");
  if (mode == 4 || mode == 5) {
    Require(result.error == (mode == 4 ? PluginLoadError::MissingAbiSymbol
                                       : PluginLoadError::AbiMismatch) &&
                !HasEvent(journal, mode, "inspect") && !HasEvent(journal, mode, "register"),
            "ABI rejection reached lifecycle/registration");
  } else if (mode <= 8) {
    Require(result.error == PluginLoadError::InvalidLifecycle &&
                !HasEvent(journal, mode, "register"),
            "invalid lifecycle reached registration");
  } else {
    Require(result.error == PluginLoadError::RegistrationRejected &&
                HasEvent(journal, mode, "request") && HasEvent(journal, mode, "native-unload"),
            "partial/oversized registration was not revoked/cooperatively closed");
  }
}
void VerifyBudgets(const std::string &path) {
  PluginHost host(kAbi);
  Require(host.Load(std::string(32769, 'x')).error == PluginLoadError::InvalidPath &&
              host.Load(std::string("a\0b", 3)).error == PluginLoadError::InvalidPath &&
              host.Load("").error == PluginLoadError::InvalidPath && host.LoadedCount() == 0,
          "invalid bounded library path opened a prefix or allocated a row");
  for (std::size_t i = 0; i < PluginHost::kMaximumPlugins; ++i) {
    const auto loaded = host.Load(path);
    Require(loaded.loaded && loaded.id == i + 1 && !loaded.registered &&
                host.RequestUnload(loaded.id) == PluginState::Unloaded,
            "plugin admission quota failed");
  }
  Require(host.Load(path).error == PluginLoadError::BudgetExceeded && !host.LoadedCount() &&
              host.Snapshot().size() == 128,
          "native reload recycled the lifetime admission budget");
}
void VerifyServiceBoundary(const std::string &path) {
  PluginHost host(kAbi);
  ServiceRegistry registry;
  const auto result = host.Load(path, &registry);
  const std::string name = std::string(254, 'x') + "\xC2\xB5";
  Require(result.loaded && registry.Size() == 64 && registry.Find(name) &&
              host.Snapshot()[0].registered_services == 64,
          "exact service/name/UTF-8 boundaries were rejected");
  Require(host.RequestUnload(result.id) == PluginState::Unloaded && registry.Size() == 0 &&
              !registry.Find(name),
          "exact-boundary services survived native unload");
}
void VerifyRevokedHostDestruction(const std::string &path, const std::filesystem::path &journal) {
  ServiceRegistry registry, copied;
  int manual = 9;
  Require(registry.Register("manual", &manual), "destruction manual-service fixture failed");
  {
    PluginHost host(kAbi);
    Require(host.Load(path, &registry).loaded && registry.Find("fixture.marker"),
            "destruction legacy-service fixture failed");
    copied = registry;
  }
  Require(!registry.Find("fixture.marker") && !copied.Find("fixture.marker") &&
              registry.Size() == 1 && copied.Size() == 1 && copied.Find("manual") == &manual &&
              !HasEvent(journal, 3, "native-unload") &&
              registry.Register("fixture.marker", &manual) &&
              copied.Register("fixture.marker", &manual),
          "destroyed host left provider visibility or forced legacy native unload");
}
} // namespace

int main(int argc, char **argv) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-plugin-lifecycle-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup final {
    std::filesystem::path root;
    ~Cleanup() {
      std::error_code error;
      std::filesystem::remove_all(root, error);
    }
  } cleanup{root};
  try {
    Require(argc == 15, "fourteen actual compiled fixtures required");
    const auto unicode = root / std::filesystem::path{std::u8string{u8"外掛 µ folder with spaces"}};
    std::filesystem::create_directories(unicode);
    const auto journal = root / "events.log";
    SetEvents(journal);
    std::vector<std::string> paths;
    for (int mode = 1; mode <= 14; ++mode) {
      const std::filesystem::path original{
          std::u8string(argv[mode], argv[mode] + std::char_traits<char>::length(argv[mode]))};
      const auto copied =
          unicode / ("fixture-" + std::to_string(mode) + original.extension().string());
      std::filesystem::copy_file(original, copied);
      paths.push_back(Utf8(copied));
    }
    VerifyCooperative(paths[0], journal);
    VerifyPending(paths[1], journal);
    for (int mode = 3; mode <= 12; ++mode)
      VerifyFailure(paths[mode - 1], mode, journal);
    VerifyServiceBoundary(paths[12]);
    VerifyRevokedHostDestruction(paths[2], journal);
    VerifyFailure(paths[13], 14, journal);
    VerifyBudgets(paths[0]);
    Require(!HasEvent(journal, 3, "native-unload") && !HasEvent(journal, 9, "native-unload") &&
                !HasEvent(journal, 10, "native-unload"),
            "host destruction forced restart-required native code out");
    std::cout
        << "Fourteen actual plugin lifecycle modules and Unicode/revocation contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
