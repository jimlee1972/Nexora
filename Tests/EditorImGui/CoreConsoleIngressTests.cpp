#include "CoreConsoleIngress.h"
#include "PlayGameplayModule.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
NexoraGameplayHostV3 api{};
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
int32_t Create(void **state, const NexoraGameplayHostV3 *host) {
  api = *host;
  *state = api.allocate(api.context, 7, 16, 16);
  return *state ? NEXORA_GAMEPLAY_OK : NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
}
int32_t Start(void *) {
  for (std::uint32_t level = 0; level <= static_cast<std::uint32_t>(core::LogLevel::Fatal);
       ++level) {
    const char message[] = "real gameplay producer";
    api.log(api.context, level, message, sizeof(message) - 1);
  }
  return NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *, double) { return NEXORA_GAMEPLAY_OK; }
void Stop(void *) {
  const char message[] = "module stopped";
  api.log(api.context, static_cast<std::uint32_t>(core::LogLevel::Warning), message,
          sizeof(message) - 1);
}
void Destroy(void *state) {
  const char message[] = "module destroyed";
  api.log(api.context, static_cast<std::uint32_t>(core::LogLevel::Info), message,
          sizeof(message) - 1);
  api.deallocate(api.context, 7, state, 16, 16);
}
int32_t Load(std::uint32_t requested, NexoraGameModuleV3 *module) {
  if (requested != NEXORA_GAMEPLAY_ABI_VERSION)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = requested;
  module->create = Create;
  module->on_start = Start;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
} // namespace
int main() {
  try {
    core::AsyncLogService producer{2};
    runtime::RuntimeConsole console{2};
    editor::preview::CoreConsoleIngress bridge{producer, console, "Core producer"};
    for (const auto &source : {std::string(runtime::RuntimeConsole::kMaxSourceBytes + 1, 'x'),
                               std::string("a\0b", 3), std::string("\xc0\xaf", 2)}) {
      bool rejected = false;
      try {
        editor::preview::CoreConsoleIngress invalid{producer, console, source};
      } catch (const std::invalid_argument &) {
        rejected = true;
      }
      Require(rejected && console.Snapshot().empty(), "invalid producer source was retained");
    }
    producer.Start();
    producer.Write(core::LogLevel::Info, "first", "observed");
    producer.Flush();
    bridge.Poll();
    const auto owning = console.Snapshot();
    for (unsigned index = 0; index < 5; ++index)
      producer.Write(core::LogLevel::Warning, "next", "retained");
    producer.Flush();
    bridge.Poll();
    Require(console.Snapshot().size() == 2 && console.Snapshot().back().sequence == 3 &&
                console.DroppedCount() == 4 && bridge.ForwardedCount() == 3,
            "unread source eviction and Console eviction were double-counted or concealed");
    for (unsigned iteration = 0; iteration < 100; ++iteration)
      bridge.Poll();
    Require(console.DroppedCount() == 4 && console.Snapshot().back().sequence == 3,
            "repeated polling duplicated records or loss counts");
    producer.Write(static_cast<core::LogLevel>(255), "invalid", "level");
    producer.Write(core::LogLevel::Info, "invalid", std::string("a\0b", 3));
    producer.Write(core::LogLevel::Info, "invalid",
                   std::string(core::AsyncLogService::kMaximumMessageBytes + 1, 'x'));
    bridge.Poll();
    Require(console.DroppedCount() == 7 && console.Snapshot().back().sequence == 3,
            "producer rejection counts changed Console records");
    producer.Stop();
    producer.Write(core::LogLevel::Info, "stopped", "rejected");
    bridge.Poll();
    Require(console.DroppedCount() == 8, "stopped producer rejection was not observed");
    producer.Start();
    producer.Write(core::LogLevel::Fatal, "restart", "new sequence");
    producer.Stop();
    bridge.Poll();
    Require(console.Snapshot().back().severity == runtime::RuntimeLogSeverity::Fatal &&
                console.DroppedCount() == 9 && bridge.ForwardedCount() == 4 &&
                owning.front().message == "observed",
            "restart/drain/copy ownership or cursor continuity failed");
    console.ReportDropped(UINT64_MAX);
    console.ReportDropped(1);
    Require(console.DroppedCount() == UINT64_MAX, "external loss count wrapped");

    core::AsyncLogService source{64};
    runtime::RuntimeConsole destination{64};
    editor::preview::CoreConsoleIngress ingress{source, destination, "Gameplay worker"};
    source.Start();
    runtime::World world;
    const auto scene = world.LoadScene("Console producer Play");
    Require(world.Activate(scene), "scene activation failed");
    const auto entity = world.CreateEntity(scene).id;
    runtime::PlaySession play(world);
    editor::preview::PlayGameplayModule module(
        [&](std::uint32_t level, std::string text) {
          source.Write(static_cast<core::LogLevel>(level), "Gameplay", std::move(text));
        },
        [&] { source.ReportRejected(); });
    Require(play.Start(.1, [](runtime::World &, double) { return true; }) &&
                module.Load(*play.PlayWorld(), Load),
            "real Play producer failed to start");
    source.Flush();
    ingress.Poll();
    const auto logs = destination.Snapshot();
    Require(logs.size() == 6 && logs[0].severity == runtime::RuntimeLogSeverity::Trace &&
                logs[1].severity == runtime::RuntimeLogSeverity::Trace &&
                logs[2].severity == runtime::RuntimeLogSeverity::Info &&
                logs[3].severity == runtime::RuntimeLogSeverity::Warning &&
                logs[4].severity == runtime::RuntimeLogSeverity::Error &&
                logs[5].severity == runtime::RuntimeLogSeverity::Fatal &&
                logs[0].category == "Gameplay" && logs[0].source == "Gameplay worker" &&
                logs[0].timestamp_nanoseconds != 0 && destination.DroppedCount() == 0,
            "real Core/gameplay severity, source, time or text mapping failed");
    const char invalid_utf8[]{'\xc0', '\xaf'};
    const char nul[]{'a', 0, 'b'};
    const std::string over(editor::preview::PlayGameplayModule::kMaximumLogMessageBytes + 1, 'x');
    api.log(api.context, 256, "invalid level", 13);
    api.log(api.context, 1, invalid_utf8, sizeof(invalid_utf8));
    api.log(api.context, 1, nul, sizeof(nul));
    api.log(api.context, 1, nullptr, 3);
    api.log(api.context, 1, over.data(), static_cast<std::uint32_t>(over.size()));
    // Oversize rejection must precede dereferencing this intentionally invalid pointer.
    api.log(api.context, 1, reinterpret_cast<const char *>(1), UINT32_MAX);
    ingress.Poll();
    Require(destination.DroppedCount() == 6 && destination.Snapshot().size() == 6,
            "malformed/oversized gameplay logs were truncated, read or lost without reporting");
    std::string exact(editor::preview::PlayGameplayModule::kMaximumLogMessageBytes - 4, 'x');
    exact += "\xf0\x9f\x98\x80";
    api.log(api.context, 1, exact.data(), static_cast<std::uint32_t>(exact.size()));
    api.log(api.context, 1, nullptr, 0);
    source.Flush();
    ingress.Poll();
    Require(destination.Snapshot()[6].message == exact &&
                destination.Snapshot()[7].message.empty() && destination.DroppedCount() == 6,
            "exact-budget UTF-8 or zero-length pointer log rejected");
    Require(play.Tick() && play.Pause() && play.Step(), "Play tick/pause/step failed");
    module.Unload();
    Require(play.Stop() && world.FindEntity(entity) && world.FindEntity(entity)->transform.x == 0,
            "logging changed authored World or retained Play");
    source.Stop();
    ingress.Poll();
    const auto final = destination.Snapshot();
    Require(final.size() == 10 && final[8].message == "module stopped" &&
                final[9].message == "module destroyed" && ingress.ForwardedCount() == 10,
            "shutdown lost module lifecycle records before Console teardown");
    runtime::RuntimeConsole disabled{0};
    editor::preview::CoreConsoleIngress disabled_ingress{source, disabled, "disabled Console"};
    disabled_ingress.Poll();
    disabled_ingress.Poll();
    Require(disabled.Snapshot().empty() && disabled.DroppedCount() == 16,
            "disabled Console concealed or duplicated producer/Console drops");
    std::cout << "PASS: Core-to-Console mapping, loss accounting, real gameplay and teardown\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
