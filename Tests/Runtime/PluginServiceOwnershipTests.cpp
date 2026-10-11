#include "Nexora/Foundation/BuildInfo.h"
#include "Nexora/Runtime/EditorSdk.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <thread>

namespace {
void Require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}
} // namespace

int main(int argc, char **argv) {
  using namespace nexora::runtime;
  Require(argc == 5, "Actual example/lifecycle plugin paths missing");
  ServiceRegistry first, second;
  PluginHost first_host(nexora::foundation::kEngineAbiVersion);
  PluginHost second_host(nexora::foundation::kEngineAbiVersion);
  const auto a = first_host.Load(argv[1], &first);
  const auto b = second_host.Load(argv[1], &second);
  Require(a.loaded && b.loaded && a.registered && b.registered && a.cooperative && b.cooperative &&
              a.id == b.id,
          "Real two-host same-ID registration fixture failed");
  constexpr auto name = "example.marker";
  Require(first_host.FindService(a.id, first, name) != nullptr &&
              first_host.FindService(a.id, first, name) == first.Find(name) &&
              std::strcmp(static_cast<const char *>(first_host.FindService(a.id, first, name)),
                          "Nexora example plugin") == 0,
          "Owned real native service did not resolve");
  Require(!first_host.FindService(a.id, second, name) &&
              !second_host.FindService(b.id, first, name),
          "Same numeric ID and same service name substituted another host's provider");
  ServiceRegistry copied = first;
  Require(first_host.FindService(a.id, copied, name) == first.Find(name),
          "Owning registry copy lost active provider identity");
  Require(!first_host.FindService(0, first, name) &&
              !first_host.FindService(std::numeric_limits<std::uint64_t>::max(), first, name) &&
              !first_host.FindService(a.id, first, "missing") &&
              !first_host.FindService(a.id, first, ""),
          "Missing admission or service unexpectedly resolved");
  std::string embedded_null{name};
  embedded_null.push_back('\0');
  embedded_null += "suffix";
  Require(!first_host.FindService(a.id, first, embedded_null) &&
              !first_host.FindService(a.id, first,
                                      std::string(PluginHost::kMaximumServiceNameBytes + 1, 'x')),
          "Unbounded or embedded-NUL lookup accepted");
  Require(first.Size() == 1 && copied.Size() == 1 && second.Size() == 1 &&
              first_host.LoadedCount() == 1 && second_host.LoadedCount() == 1,
          "Rejected lookup mutated native or registry state");

  // Every borrowed result above is consumed within its expression, before any mutation/unload.
  int manual = 7;
  Require(first.Unregister(name) && first.Register(name, &manual) && first.Find(name) == &manual &&
              !first_host.FindService(a.id, first, name) &&
              first_host.FindService(a.id, copied, name) != nullptr,
          "Manual replacement acquired the still-loaded provider's identity");
  Require(first_host.RequestUnload(a.id) == PluginState::Unloaded && !copied.Find(name) &&
              !first_host.FindService(a.id, copied, name) &&
              !first_host.FindService(a.id, first, name) && first.Find(name) == &manual,
          "Unload retained qualified visibility or removed manual replacement");
  Require(second_host.FindService(b.id, second, name) != nullptr,
          "Another host's live provider was revoked");
  const auto replacement = first_host.Load(argv[1], &copied);
  Require(replacement.loaded && replacement.id != a.id &&
              !first_host.FindService(a.id, copied, name) &&
              first_host.FindService(replacement.id, copied, name) != nullptr,
          "Same-name reload substituted a retired admission");
  Require(first_host.RequestUnload(replacement.id) == PluginState::Unloaded &&
              second_host.RequestUnload(b.id) == PluginState::Unloaded &&
              !first_host.FindService(replacement.id, copied, name) &&
              !second_host.FindService(b.id, second, name),
          "Final native shutdown retained owned services");

  ServiceRegistry pending_services;
  PluginHost pending_host(nexora::foundation::kEngineAbiVersion);
  const auto pending = pending_host.Load(argv[2], &pending_services);
  Require(pending.loaded &&
              pending_host.FindService(pending.id, pending_services, "fixture.marker") != nullptr,
          "Actual worker fixture service missing");
  const ServiceRegistry pending_copy = pending_services;
  Require(pending_host.RequestUnload(pending.id) == PluginState::ShutdownPending &&
              pending_host.LoadedCount() == 1 &&
              !pending_host.FindService(pending.id, pending_services, "fixture.marker") &&
              !pending_host.FindService(pending.id, pending_copy, "fixture.marker"),
          "Still-mapped shutdown-pending worker retained qualified service visibility");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (pending_host.LoadedCount() && std::chrono::steady_clock::now() < deadline) {
    pending_host.PollShutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Require(pending_host.LoadedCount() == 0, "Actual worker did not cooperatively quiesce");

  ServiceRegistry legacy_services;
  PluginHost legacy_host(nexora::foundation::kEngineAbiVersion);
  const auto legacy = legacy_host.Load(argv[3], &legacy_services);
  Require(legacy.loaded && !legacy.cooperative &&
              legacy_host.FindService(legacy.id, legacy_services, "fixture.marker") != nullptr &&
              legacy_host.RequestUnload(legacy.id) == PluginState::RestartRequired &&
              legacy_host.LoadedCount() == 1 &&
              !legacy_host.FindService(legacy.id, legacy_services, "fixture.marker"),
          "Still-mapped legacy provider retained qualified service visibility");

  ServiceRegistry maximum_services;
  PluginHost maximum_host(nexora::foundation::kEngineAbiVersion);
  const auto maximum = maximum_host.Load(argv[4], &maximum_services);
  const auto maximum_name = std::string(254, 'x') + "\xC2\xB5";
  Require(maximum.loaded && maximum_services.Size() == PluginHost::kMaximumServices &&
              maximum_name.size() == PluginHost::kMaximumServiceNameBytes &&
              maximum_host.FindService(maximum.id, maximum_services, maximum_name) != nullptr &&
              !maximum_host.FindService(maximum.id, maximum_services, maximum_name + "x") &&
              maximum_host.RequestUnload(maximum.id) == PluginState::Unloaded,
          "Exact UTF-8 name/service boundary rejected or oversized lookup accepted");
  return 0;
}
