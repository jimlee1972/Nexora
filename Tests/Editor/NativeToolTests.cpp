#include "NativeToolFixtureBridge.h"
#include "Nexora/Editor/NativeTool.h"
#include "Nexora/Foundation/BuildInfo.h"

#include <cstdlib>
#include <iostream>

namespace {
void Require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}
} // namespace

int main(int argc, char **argv) {
  using namespace nexora::editor;
  using namespace nexora::runtime;
  Require(argc == 15, "Actual native tool fixture paths missing");
  PluginHost host(nexora::foundation::kEngineAbiVersion);
  ServiceRegistry registry;
  NativeToolInvoker invoker;
  NativeToolInvoker second_invoker;
  const auto first = host.Load(argv[1], &registry);
  Require(first.loaded && first.registered && first.cooperative, "Real tool admission failed");
  const auto count = [&](const PluginLoadResult &admission, const ServiceRegistry &services) {
    const auto *value = static_cast<const std::uint32_t *>(
        host.FindService(admission.id, services, "fixture.calls"));
    Require(value != nullptr, "Real native call counter unavailable");
    return *value;
  };
  const std::vector<std::byte> input{std::byte{0}, std::byte{255}, std::byte{17}};
  auto result =
      invoker.Invoke(host, registry, first.id, "fixture.tool", NativeToolOperation::Edit, input);
  Require(result.state == NativeToolState::Success && result.callback_result == 0 &&
              result.bytes == input && count(first, registry) == 1,
          "Actual native byte callback did not execute with exact binary output");
  const auto retained = result;
  const ServiceRegistry copied = registry;
  result =
      invoker.Invoke(host, copied, first.id, "fixture.tool", NativeToolOperation::Serialize, {});
  Require(result.state == NativeToolState::Success && result.bytes.empty() &&
              count(first, registry) == 2,
          "Copied registry or empty byte invocation failed");
  const std::vector<std::byte> maximum(NativeToolInvoker::kMaximumInputBytes, std::byte{91});
  result = invoker.Invoke(host, registry, first.id, "fixture.tool", NativeToolOperation::Preview,
                          maximum);
  Require(result.state == NativeToolState::Success && result.bytes == maximum,
          "Exact native input/output byte boundary failed");
  auto oversized = maximum;
  oversized.push_back(std::byte{0});
  const auto before = count(first, registry);
  result = invoker.Invoke(host, registry, first.id, "fixture.tool", NativeToolOperation::Inspect,
                          oversized);
  Require(result.state == NativeToolState::Invalid && !result.callback_result &&
              result.bytes.empty() && count(first, registry) == before,
          "Oversized input reached native callback");
  result = invoker.Invoke(host, registry, first.id, "fixture.tool",
                          static_cast<NativeToolOperation>(3), input);
  Require(result.state == NativeToolState::Invalid && count(first, registry) == before,
          "Combined operation reached native callback");
  std::thread other([&] {
    result = invoker.Invoke(host, registry, first.id, "fixture.tool", NativeToolOperation::Inspect,
                            input);
  });
  other.join();
  Require(result.state == NativeToolState::WrongThread && !result.callback_result &&
              count(first, registry) == before,
          "Foreign-thread invocation reached registry or native callback");
  PluginHost foreign(nexora::foundation::kEngineAbiVersion);
  ServiceRegistry foreign_registry;
  const auto foreign_admission = foreign.Load(argv[1], &foreign_registry);
  Require(foreign_admission.loaded && foreign_admission.id == first.id,
          "Real same-ID fixture failed");
  result = invoker.Invoke(host, foreign_registry, first.id, "fixture.tool",
                          NativeToolOperation::Inspect, input);
  Require(result.state == NativeToolState::Unavailable && !result.callback_result,
          "Another host's same numeric ID reached native callback");
  Require(foreign.RequestUnload(foreign_admission.id) == PluginState::Unloaded,
          "Foreign fixture failed cooperative unload");
  NexoraEditorToolServiceV1 manual{};
  Require(registry.Unregister("fixture.tool") && registry.Register("fixture.tool", &manual),
          "Manual replacement setup failed");
  result =
      invoker.Invoke(host, registry, first.id, "fixture.tool", NativeToolOperation::Inspect, input);
  Require(result.state == NativeToolState::Unavailable && !result.callback_result,
          "Manual replacement acquired native tool authority");
  Require(host.RequestUnload(first.id) == PluginState::Unloaded, "Real tool did not unload");
  result =
      invoker.Invoke(host, copied, first.id, "fixture.tool", NativeToolOperation::Inspect, input);
  Require(result.state == NativeToolState::Unavailable && retained.bytes == input &&
              retained.callback_result == 0 && retained.state == NativeToolState::Success,
          "Revoked copied registry reached native code or invalidated owning outcome");

  for (int mode = 2; mode <= 14; ++mode) {
    ServiceRegistry services;
    const auto admission = host.Load(argv[mode], &services);
    Require(admission.loaded && admission.cooperative, "Actual tool fixture did not load");
    if (mode <= 7) {
      result = invoker.Invoke(host, services, admission.id, "fixture.tool",
                              NativeToolOperation::Inspect, input);
      Require(result.state == NativeToolState::Invalid && !result.callback_result &&
                  count(admission, services) == 0,
              "Malformed native table reached callback");
    } else if (mode == 8 || mode == 9) {
      result = invoker.Invoke(host, services, admission.id, "fixture.tool",
                              NativeToolOperation::Inspect, input);
      Require(result.state == NativeToolState::Invalid && result.bytes.empty() &&
                  result.callback_result == (mode == 8 ? 42 : 0) && count(admission, services) == 1,
              "Unknown result or over-capacity output accepted");
    } else if (mode == 10) {
      for (int status = 1; status <= 3; ++status) {
        const std::vector<std::byte> code{static_cast<std::byte>(status)};
        result = invoker.Invoke(host, services, admission.id, "fixture.tool",
                                NativeToolOperation::Inspect, code);
        const auto expected = status == 1   ? NativeToolState::Rejected
                              : status == 2 ? NativeToolState::Unavailable
                                            : NativeToolState::Failed;
        Require(result.state == expected && result.callback_result == status &&
                    result.bytes.empty(),
                "Failure bytes leaked or known native status was lost");
      }
    } else if (mode == 11) {
      result = invoker.Invoke(host, services, admission.id, "fixture.tool",
                              NativeToolOperation::Inspect, input);
      Require(result.state == NativeToolState::Failed && !result.callback_result &&
                  result.bytes.empty(),
              "Unexpected native exception escaped or retained partial output");
    } else if (mode == 12) {
      result = invoker.Invoke(host, services, admission.id, "fixture.tool",
                              NativeToolOperation::Edit, {});
      Require(result.state == NativeToolState::Unavailable && count(admission, services) == 0,
              "Undeclared operation reached callback");
      result = invoker.Invoke(host, services, admission.id, "fixture.tool",
                              NativeToolOperation::Inspect, input);
      Require(result.state == NativeToolState::Rejected && result.callback_result == 1 &&
                  result.bytes.empty(),
              "Actual provider rejected output capacity was lost");
      result = invoker.Invoke(host, services, admission.id, "fixture.tool",
                              NativeToolOperation::Inspect, std::span(input).first(2));
      Require(result.state == NativeToolState::Success &&
                  result.bytes == std::vector<std::byte>(input.begin(), input.begin() + 2),
              "Exact provider-specific output capacity failed");
      result = invoker.Invoke(host, services, admission.id, "fixture.tool",
                              NativeToolOperation::Inspect, std::span(maximum).first(4));
      Require(result.state == NativeToolState::Rejected && !result.callback_result,
              "Provider-specific input budget reached native callback");
    } else if (mode == 13) {
      auto *bridge = static_cast<NativeToolFixtureBridge *>(
          host.FindService(admission.id, services, "fixture.bridge"));
      Require(bridge != nullptr, "Actual recursive fixture bridge missing");
      struct Context final {
        NativeToolInvoker &invoker;
        PluginHost &host;
        ServiceRegistry &services;
        std::uint64_t id;
        NativeToolOutcome nested;
      } context{second_invoker, host, services, admission.id, {}};
      bridge->context = &context;
      bridge->call = [](void *value) {
        auto &c = *static_cast<Context *>(value);
        c.nested = c.invoker.Invoke(c.host, c.services, c.id, "fixture.tool",
                                    NativeToolOperation::Inspect, {});
      };
      result = invoker.Invoke(host, services, admission.id, "fixture.tool",
                              NativeToolOperation::Inspect, input);
      bridge->call = nullptr;
      bridge->context = nullptr;
      bridge = nullptr; // Release the native control borrow before unload.
      Require(result.state == NativeToolState::Success && result.bytes == input &&
                  context.nested.state == NativeToolState::Reentrant &&
                  !context.nested.callback_result && count(admission, services) == 1,
              "Actual native callback reentered its host invoker");
    } else {
      result = invoker.Invoke(host, services, admission.id, "fixture.tool",
                              NativeToolOperation::Inspect, input);
      Require(result.state == NativeToolState::Invalid && result.callback_result == 0 &&
                  result.bytes.empty() && count(admission, services) == 1,
              "Native success without an explicit output size was accepted");
    }
    Require(host.RequestUnload(admission.id) == PluginState::Unloaded,
            "Native fixture failed unload");
  }
  ServiceRegistry reloaded_services;
  const auto reloaded = host.Load(argv[1], &reloaded_services);
  result = invoker.Invoke(host, reloaded_services, first.id, "fixture.tool",
                          NativeToolOperation::Inspect, input);
  Require(result.state == NativeToolState::Unavailable && count(reloaded, reloaded_services) == 0,
          "Retired admission resolved same-name reloaded tool");
  result = invoker.Invoke(host, reloaded_services, reloaded.id, "fixture.tool",
                          NativeToolOperation::Inspect, input);
  Require(result.state == NativeToolState::Success && result.bytes == input &&
              host.RequestUnload(reloaded.id) == PluginState::Unloaded,
          "Guard did not release after failures or actual new admission failed");
  return 0;
}
