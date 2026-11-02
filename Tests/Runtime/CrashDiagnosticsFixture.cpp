#include "Nexora/Core/Diagnostics.h"
#include "Nexora/Core/Engine.h"
#include "Nexora/Runtime/GameplayModuleHost.h"
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#if !defined(_WIN32)
#include <sys/resource.h>
#endif

namespace {
void ExhaustStack(unsigned depth);
void (*recurse)(unsigned) = ExhaustStack;
void ExhaustStack(unsigned depth) {
  volatile unsigned char buffer[4096];
  buffer[depth % sizeof(buffer)] = static_cast<unsigned char>(depth);
  recurse(depth + 1);
  if (buffer[depth % sizeof(buffer)] == 255)
    std::abort();
}
} // namespace

int main(int argc, char **argv) {
#if !defined(_WIN32)
  // Exercise fatal termination without producing large core files in CI.
  const rlimit disabled{0, 0};
  (void)setrlimit(RLIMIT_CORE, &disabled);
#endif
  if (argc != 3)
    return 2;
  const std::string mode = argv[1];
  return nexora::core::RunDiagnosed("CrashDiagnosticsFixture", [&]() -> int {
    using namespace nexora;
    using namespace std::string_view_literals;
    core::DiagnosticScope scope("Fixture.main");
    core::SetDiagnosticMetadata("game.project", "diagnostic fixture");
    core::SetDiagnosticMetadata("game.primary_scene", std::uint64_t{41});
    core::SetDiagnosticMetadata("game.primary_scene", std::uint64_t{42});
    const auto directory = core::DiagnosticSession::Directory();
    core::Engine engine;
    engine.Initialize();
    if (directory != core::DiagnosticSession::Directory())
      throw std::runtime_error("nested session changed directory");
    engine.Services().log->Write(core::LogLevel::Info, "Game", "accepted immediately before crash");
    if (mode == "exception")
      throw std::runtime_error("game exception details");
    if (mode == "unknown-exception")
      throw 7;
    if (mode == "terminate")
      std::terminate();
    if (mode == "abort")
      std::abort();
    if (mode == "stack-overflow") {
      core::DiagnosticScope stack_scope("Gameplay.stack");
      ExhaustStack(0);
      return 2;
    }
    if (mode == "worker-crash") {
      std::thread worker([] {
        core::DiagnosticScope worker_scope("Gameplay.worker");
        std::abort();
      });
      worker.join();
    }
    if (mode == "rotation") {
      for (int i = 0; i < 1200; ++i)
        core::WriteDiagnostic(core::LogLevel::Info, "Rotation", std::string(4096, 'a'));
    }
    if (mode == "job-exception") {
      auto job = engine.Services().jobs->Submit(
          {[](const core::CancellationToken &) { throw std::runtime_error("game job exception"); },
           core::JobPriority::Normal,
           {},
           "game job"});
      try {
        engine.Services().jobs->Wait(job);
      } catch (const std::runtime_error &) {
        return 0;
      }
      return 2;
    }
    if (mode == "job-crash") {
      auto job =
          engine.Services().jobs->Submit({[](const core::CancellationToken &) { std::abort(); },
                                          core::JobPriority::Normal,
                                          {},
                                          "game job"});
      engine.Services().jobs->Wait(job);
      return 2;
    }
    if (mode == "normal") {
      std::vector<std::thread> writers;
      for (int i = 0; i < 4; ++i)
        writers.emplace_back([i] {
          for (int j = 0; j < 16; ++j)
            core::WriteDiagnostic(core::LogLevel::Info, "Concurrent",
                                  "writer=" + std::to_string(i) + " record=" + std::to_string(j));
        });
      for (auto &writer : writers)
        writer.join();
      core::WriteDiagnostic(core::LogLevel::Info, "Escape", "first\nforged\0tail"sv);
      core::WriteGameplayDiagnostic(2, "invalid", UINT32_MAX);
      core::WriteDiagnostic(core::LogLevel::Error, "Game", "recoverable game failure");
      return 0;
    }
    if (mode == "rotation")
      return 0;
    NexoraGameplayHostV3 host{};
    host.struct_size = sizeof(host);
    host.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
    host.context = const_cast<char *>(mode.c_str());
    host.log = [](void *, std::uint32_t level, const char *text, std::uint32_t size) {
      core::WriteGameplayDiagnostic(level, text, size);
    };
    runtime::GameplayModuleHost module(host);
    if (!module.Load(std::filesystem::path(reinterpret_cast<const char8_t *>(argv[2]))))
      throw std::runtime_error("module load failed");
    if (mode == "stop-crash" || mode == "destroy-crash") {
      module.Unload();
      return 2;
    }
    if (mode == "fixed-crash" || mode == "fixed-error") {
      if (module.FixedUpdate(1.0 / 60.0))
        return 2;
    } else if (module.Update(1.0 / 60.0))
      return 2;
    return 0;
  });
}
